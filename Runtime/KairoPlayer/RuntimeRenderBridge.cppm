module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "RuntimeRenderResourceTracking.hpp"

// TEACHING INTEGRATION: RuntimeProject owns the authored Scene and AssetRegistry;
// this bridge owns uploaded RendererRuntime mesh/texture HANDLES and imported
// animation clocks. RealtimeSceneBridge owns the shared Editor/Player CPU
// extraction algorithm, not GPU resources. Destruct this bridge BEFORE the
// RendererRuntime it borrows. See docs/ASSET_SCENE_TO_GPU_LIFETIME_LAB.md.
export module Kairo.Player.RuntimeRenderBridge;

import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Renderer;
import Kairo.Runtime.RealtimeSceneBridge;
import Kairo.Player.RuntimeProject;

export namespace kairo::player
{
    // Per-entity process-local animation state, not a serialized Scene
    // component. Explicit distinguishes gameplay selection from automatic
    // clip-zero looping; clearing control returns to the compatibility path.
    struct RuntimeAnimationPlaybackState final
    {
        std::uint32_t ClipIndex = 0u;
        float TimeSeconds = 0.0f;
        float PlaybackRate = 1.0f;
        kairo::engine::AnimationTimeMode TimeMode =
            kairo::engine::AnimationTimeMode::Loop;
        bool Paused = false;
        bool Explicit = false;
    };

    /// Runtime-owned bridge from persistent project asset IDs to process-local
    /// renderer handles. It owns GPU allocations plus transient imported-animation
    /// playback state. Gameplay may explicitly own a scene instance's clip/clock;
    /// unowned instances retain the compatibility policy of looping clip zero.
    // RendererRuntime and RuntimeProject are borrowed references; the bridge
    // is noncopyable and tracks only the GPU HANDLEs it created. GPU storage
    // remains owned by the renderer device/backend. Source glTF metadata is
    // retained in RenderAssetBindings to support skin palette evaluation.
    class RuntimeRenderBridge final
    {
    public:
        // Borrow RendererRuntime and validated RuntimeProject; own only the native
        // mesh/texture handles created during import. If loading throws, release
        // already-created GPU handles before propagating the failure.
        RuntimeRenderBridge(kairo::renderer::RendererRuntime& renderer,
            const RuntimeProject& project)
            : m_Renderer(renderer),
              m_Project(project),
              m_Assets(project.Assets()),
              m_Cache(project.Root() / ".kairo" / "derived-data")
        {
            try { LoadAssets(); }
            catch (...)
            {
                const auto failures = ReleaseResources();
                if (failures != 0u)
                    std::fprintf(stderr, "KairoPlayer failed to roll back %zu renderer resources.\n", failures);
                throw;
            }
        }

        ~RuntimeRenderBridge() noexcept
        {
            const auto failures = ReleaseResources();
            if (failures != 0u)
                std::fprintf(stderr, "KairoPlayer could not release %zu renderer resources.\n", failures);
        }

        RuntimeRenderBridge(const RuntimeRenderBridge&) = delete;
        RuntimeRenderBridge& operator=(const RuntimeRenderBridge&) = delete;

        // Query clips from the exact retained glTF source bound to an authored
        // SceneInstance. A plain mesh or unbound scene is not animatable.
        [[nodiscard]] std::size_t AnimationClipCount(
            kairo::engine::Entity entity) const
        {
            return RequireAnimationSource(entity).Animations.size();
        }

        // Find by source clip name without inventing a fallback index. Caller
        // must distinguish missing clip from legitimate clip zero.
        [[nodiscard]] std::optional<std::uint32_t> FindAnimationClip(
            kairo::engine::Entity entity, std::string_view name) const
        {
            const auto& source = RequireAnimationSource(entity);
            for (std::size_t index = 0u; index < source.Animations.size(); ++index)
                if (source.Animations[index].Name == name)
                    return static_cast<std::uint32_t>(index);
            return std::nullopt;
        }

        // Borrow a name view into the retained glTF artifact; the returned
        // view dies when this bridge/binding is destroyed or replaced.
        [[nodiscard]] std::string_view AnimationClipName(
            kairo::engine::Entity entity, std::uint32_t clipIndex) const
        {
            const auto& source = RequireAnimationSource(entity);
            if (clipIndex >= source.Animations.size())
                throw std::out_of_range("Runtime animation clip index is out of range.");
            return source.Animations[clipIndex].Name;
        }

        /// Gives gameplay/editor code ownership of one scene instance's imported
        /// animation state. Re-selecting the already-active clip preserves time
        /// unless restart is requested, preventing locomotion selection from
        /// restarting walk/run every frame.
        // An explicit gameplay clip selection validates glTF source, clip index and
        // playback rate, then preserves current time when the same clip is selected
        // unless restart is requested. This prevents walk/run animation restarting each
        // frame.
        void SetAnimation(kairo::engine::Entity entity,
            std::uint32_t clipIndex,
            kairo::engine::AnimationTimeMode timeMode =
                kairo::engine::AnimationTimeMode::Loop,
            float playbackRate = 1.0f,
            bool restart = false)
        {
            if (!std::isfinite(playbackRate) || playbackRate < 0.0f ||
                playbackRate > 16.0f)
                throw std::invalid_argument(
                    "Runtime animation playback rate must be finite within [0, 16].");
            const auto& source = RequireAnimationSource(entity);
            if (clipIndex >= source.Animations.size())
                throw std::out_of_range("Runtime animation clip index is out of range.");

            m_Controllers.erase(entity.Value);
            auto found = m_AnimationStates.find(entity.Value);
            if (found == m_AnimationStates.end())
            {
                m_AnimationStates.emplace(entity.Value,
                    RuntimeAnimationPlaybackState{
                        clipIndex, 0.0f, playbackRate, timeMode, false, true });
                return;
            }
            auto& state = found->second;
            if (restart || state.ClipIndex != clipIndex)
                state.TimeSeconds = 0.0f;
            state.ClipIndex = clipIndex;
            state.PlaybackRate = playbackRate;
            state.TimeMode = timeMode;
            state.Explicit = true;
        }

        // Map an authored clip name to a validated index, then share the
        // SetAnimation restart/rate rules; unknown names throw explicitly.
        void SetAnimationByName(kairo::engine::Entity entity,
            std::string_view clipName,
            kairo::engine::AnimationTimeMode timeMode =
                kairo::engine::AnimationTimeMode::Loop,
            float playbackRate = 1.0f,
            bool restart = false)
        {
            const auto clip = FindAnimationClip(entity, clipName);
            if (!clip.has_value())
                throw std::out_of_range(
                    "Runtime animation clip name is not present on this scene instance.");
            SetAnimation(entity, *clip, timeMode, playbackRate, restart);
        }

        // Apply a rigid three-node correction after clip sampling. This is
        // independent of clip controls and also works on a rest-only skin.
        // Validate the retained artifact's rest chain before publishing; a
        // later scaled/degenerate sampled chain will reject during extraction.
        void SetTwoBoneIK(kairo::engine::Entity entity,
            kairo::runtime::renderbridge::SceneTwoBoneIK request)
        {
            if(!m_Project.Scene().Contains(entity)||!m_Project.Scene().HasSceneInstance(entity))
                throw std::invalid_argument("IK requires a scene-instance entity.");
            const auto asset=m_Project.Scene().SceneInstance(entity).SceneAsset;
            const auto* source=m_Assets.ResolveGltfSource(asset);
            if(!source)throw std::invalid_argument("IK requires imported glTF metadata.");
            (void)kairo::runtime::renderbridge::ApplyGltfTwoBoneIK(*source,
                kairo::engine::BuildGltfRestPose(*source),request);
            m_Animations.SetTwoBoneIK(entity,asset.ID,request);
        }
        void ClearTwoBoneIK(kairo::engine::Entity entity) noexcept { m_Animations.ClearTwoBoneIK(entity); }

        // Pause the already-initialized runtime clock, not the authoritative
        // Scene. RequireAnimationState rejects calls before initialization.
        void SetAnimationPaused(kairo::engine::Entity entity, bool paused)
        {
            auto& state = RequireAnimationState(entity);
            state.Paused = paused;
            state.Explicit = true;
        }

        // Set an explicit finite playback time; StepAnimations later applies
        // Loop/Clamp using the selected clip's real source duration.
        void SetAnimationTime(kairo::engine::Entity entity, float timeSeconds)
        {
            if (!std::isfinite(timeSeconds))
                throw std::invalid_argument("Runtime animation time must be finite.");
            auto& state = RequireAnimationState(entity);
            state.TimeSeconds = timeSeconds;
            state.Explicit = true;
        }

        /// Releases gameplay ownership. The next StepAnimations call returns the
        /// instance to the compatibility clip-zero looping policy.
        // Release gameplay's explicit animation ownership. The next StepAnimations may
        // recreate compatibility looping clip zero if the authored instance still has
        // usable glTF animation.
        void ClearAnimationControl(kairo::engine::Entity entity) noexcept
        {
            m_AnimationStates.erase(entity.Value);
            m_Controllers.erase(entity.Value);
            m_Animations.Clear(entity);
        }

        // Return a COPY of current process-local control state so callers
        // cannot retain a reference to a mutable bridge-owned map entry.
        [[nodiscard]] std::optional<RuntimeAnimationPlaybackState> AnimationState(
            kairo::engine::Entity entity) const noexcept
        {
            const auto found = m_AnimationStates.find(entity.Value);
            if (found == m_AnimationStates.end()) return std::nullopt;
            return found->second;
        }

        /// Advances explicit gameplay clocks and compatibility clip-zero clocks.
        /// Animation sampling stays inside the shared RealtimeSceneBridge so
        /// Player, Editor and migration-imported glTF data use one evaluator.
        // Advance each live scene instance's animation clock by delta*playbackRate,
        // normalize according to Loop/Clamp and publish playback overrides for shared
        // RealtimeSceneBridge sampling. Clear stale IDs after entity removal.
        // A gameplay caller can drive declared boolean parameters without
        // overriding the controller's clip/blend ownership.
        void SetControllerBool(kairo::engine::Entity entity,std::string_view name,bool value)
        {
            const auto& source=RequireAnimationSource(entity);
            const auto asset=m_Project.Scene().SceneInstance(entity).SceneAsset;
            const auto* definition=m_Assets.ResolveController(asset);
            if(!definition)throw std::logic_error("Entity has no authored animation controller.");
            auto& binding=EnsureController(entity,asset.ID,*definition,source);
            if(!binding.Parameters.Contains(name))
                throw std::invalid_argument("Unknown controller parameter.");
            binding.Parameters.SetBool(std::string(name),value);
            m_AnimationStates.erase(entity.Value);
        }

        void StepAnimations(float deltaSeconds)
        {
            if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f ||
                deltaSeconds > 1.0f)
                throw std::invalid_argument(
                    "Runtime animation delta must be finite within [0, 1].");

            std::unordered_set<std::uint32_t> present;
            for (const auto entity : m_Project.Scene().SceneInstanceEntities())
            {
                present.emplace(entity.Value);
                const auto& instance = m_Project.Scene().SceneInstance(entity);
                m_Animations.ForgetMismatchedIK(entity,instance.SceneAsset.ID);
                const auto* source = m_Assets.ResolveGltfSource(instance.SceneAsset);
                if (source == nullptr || source->Animations.empty())
                {
                    m_Animations.Clear(entity);
                    m_AnimationStates.erase(entity.Value);
                    m_Controllers.erase(entity.Value);
                    continue;
                }

                const auto explicitState=m_AnimationStates.find(entity.Value);
                const bool explicitClip=explicitState!=m_AnimationStates.end()&&explicitState->second.Explicit;
                const auto* definition=m_Assets.ResolveController(instance.SceneAsset);
                if(definition&&!explicitClip)
                {
                    auto& binding=EnsureController(entity,instance.SceneAsset.ID,*definition,*source);
                    m_Animations.SetPose(entity,instance.SceneAsset.ID,
                        binding.Runtime.Advance(*source,binding.Parameters,deltaSeconds));
                    m_AnimationStates.erase(entity.Value);
                    continue;
                }
                m_Controllers.erase(entity.Value);
                auto [iterator, inserted] = m_AnimationStates.try_emplace(
                    entity.Value, RuntimeAnimationPlaybackState{});
                auto& state = iterator->second;
                if (inserted)
                {
                    state.ClipIndex = 0u;
                    state.TimeMode = kairo::engine::AnimationTimeMode::Loop;
                    state.Explicit = false;
                }
                if (state.ClipIndex >= source->Animations.size())
                    throw std::out_of_range(
                        "Runtime animation state references a clip outside its source asset.");

                if (!state.Paused)
                {
                    state.TimeSeconds += deltaSeconds * state.PlaybackRate;
                    if (!std::isfinite(state.TimeSeconds))
                        throw std::overflow_error("Runtime animation clock overflowed.");
                }
                const float duration =
                    source->Animations[state.ClipIndex].DurationSeconds();
                state.TimeSeconds = NormalizeAnimationTime(
                    state.TimeSeconds, duration, state.TimeMode);
                m_Animations.Set(entity, {
                    .ClipIndex = state.ClipIndex,
                    .TimeSeconds = state.TimeSeconds,
                    .TimeMode = state.TimeMode
                });
            }

            m_Animations.RetainTwoBoneIK(present);
            for(auto it=m_Controllers.begin();it!=m_Controllers.end();)
                if(!present.contains(it->first))
                {
                    m_Animations.Clear(kairo::engine::Entity{it->first});
                    it=m_Controllers.erase(it);
                }
                else ++it;
            for (auto iterator = m_AnimationStates.begin();
                iterator != m_AnimationStates.end();)
            {
                if (present.contains(iterator->first))
                {
                    ++iterator;
                    continue;
                }
                m_Animations.Clear(kairo::engine::Entity{ iterator->first });
                iterator = m_AnimationStates.erase(iterator);
            }
        }

        // Number of instances with currently tracked clocks, including the
        // implicit clip-zero compatibility path (not number of GPU meshes).
        [[nodiscard]] std::size_t AnimatedSceneInstanceCount() const noexcept
        {
            return m_AnimationStates.size()+m_Controllers.size();
        }

        // Delegate CPU scene extraction to the shared Editor/Player BuildRenderScene
        // function. This keeps object IDs, glTF skinning, PBR and photometric
        // conversion identical between authoring and shipped runtime.
        [[nodiscard]] kairo::renderer::RenderScene BuildScene() const
        {
            return kairo::runtime::renderbridge::BuildRenderScene(
                m_Project.Scene(), m_Assets, m_Animations);
        }

        // Delegate primary-camera selection to the shared bridge. Render
        // camera uses the authored WorldTransform; it is not tied to GPU API.
        [[nodiscard]] kairo::renderer::CameraPose CameraPose() const
        {
            return kairo::runtime::renderbridge::SelectRenderCamera(
                m_Project.Scene());
        }

    private:
        // Loop wraps time with fmod including a negative-time correction; Clamp pins
        // time to [0,duration]. A zero-duration clip returns zero, avoiding division by
        // zero.
        [[nodiscard]] static float NormalizeAnimationTime(float timeSeconds,
            float durationSeconds, kairo::engine::AnimationTimeMode mode)
        {
            if (durationSeconds <= 0.0f) return 0.0f;
            switch (mode)
            {
                case kairo::engine::AnimationTimeMode::Clamp:
                    return std::clamp(timeSeconds, 0.0f, durationSeconds);
                case kairo::engine::AnimationTimeMode::Loop:
                {
                    float wrapped = std::fmod(timeSeconds, durationSeconds);
                    if (wrapped < 0.0f) wrapped += durationSeconds;
                    return wrapped;
                }
            }
            throw std::invalid_argument("Runtime animation time mode is invalid.");
        }

        // Check entity still exists, has SceneInstance, and resolves to
        // retained glTF metadata with at least one clip. Returned reference
        // borrows RenderAssetBindings storage; it is never a GPU buffer.
        [[nodiscard]] const kairo::assets::GltfSceneArtifactData&
        RequireAnimationSource(kairo::engine::Entity entity) const
        {
            if (!m_Project.Scene().Contains(entity) ||
                !m_Project.Scene().HasSceneInstance(entity))
                throw std::invalid_argument(
                    "Runtime animation control requires a scene-instance entity.");
            const auto& instance = m_Project.Scene().SceneInstance(entity);
            const auto* source = m_Assets.ResolveGltfSource(instance.SceneAsset);
            if (source == nullptr || source->Animations.empty())
                throw std::invalid_argument(
                    "Runtime animation control requires an imported glTF scene with clips.");
            return *source;
        }

        // Require a previously created clock after confirming current glTF
        // source validity. This prevents pausing a non-existent implicit clip.
        [[nodiscard]] RuntimeAnimationPlaybackState& RequireAnimationState(
            kairo::engine::Entity entity)
        {
            (void)RequireAnimationSource(entity);
            const auto found = m_AnimationStates.find(entity.Value);
            if (found == m_AnimationStates.end())
                throw std::logic_error(
                    "Runtime animation state is not initialized; call SetAnimation or StepAnimations first.");
            return found->second;
        }

        // Native backends can refuse DestroyTexture/DestroyMesh while a draw
        // still refers to the resource. Clear the submitted scene FIRST; then
        // each backend handles actual GPU retirement in its destruction API.
        // Every failed deletion stays in the journal for a possible retry.
        // This is best-effort noexcept teardown; callers receive the failure
        // count and destructor/constructor rollback report it on stderr.
        [[nodiscard]] std::size_t ReleaseResources() noexcept
        {
            return detail::ReleaseSceneResources(m_OwnedMeshes, m_OwnedTextures,
                [this]
                {
                    m_Renderer.SubmitRenderScene(kairo::renderer::RenderScene{});
                },
                [this](kairo::renderer::MeshHandle handle)
                {
                    m_Renderer.DestroyMesh(handle);
                },
                [this](kairo::renderer::TextureHandle handle)
                {
                    m_Renderer.DestroyTexture(handle);
                });
        }

        // Cache a texture asset ID's upload by its import color-space/normal-map
        // semantics. The same persistent texture cannot safely act as sRGB albedo and
        // linear normal map under one native handle without an explicit separate
        // binding.
        // Cache one uploaded TextureHandle per typed AssetID+consistent
        // color-space/normal-map semantics. Incompatible use of the SAME
        // source as both sRGB color and linear data throws rather than silently
        // reinterpreting sampled bytes. ImportRenderTexture creates CPU
        // artifacts; RendererRuntime::CreateTexture allocates native storage.
        // CreateTrackBind reserves journal capacity BEFORE GPU creation and
        // records the handle BEFORE map/binding allocations that may throw.
        [[nodiscard]] kairo::renderer::TextureHandle EnsureTexture(
            kairo::assets::TextureAssetHandle asset,
            const kairo::assets::TextureImportSettings& settings)
        {
            if (const auto found = m_TextureSettings.find(asset.ID);
                found != m_TextureSettings.end())
            {
                if (found->second.ColorSpace != settings.ColorSpace ||
                    found->second.NormalMap != settings.NormalMap)
                    throw std::invalid_argument(
                        "One texture asset is referenced with incompatible color/data semantics: " +
                        asset.ID.ToString());
                return m_Assets.ResolveTexture(asset);
            }
            const auto texture = kairo::runtime::renderbridge::ImportRenderTexture(
                m_Project.Root(), asset, settings, m_Project.Assets(), m_Imports, m_Cache);
            return detail::CreateTrackBind(m_OwnedTextures,
                [this, &texture] { return m_Renderer.CreateTexture(texture); },
                [this, asset, settings](kairo::renderer::TextureHandle handle)
                {
                    m_TextureSettings.emplace(asset.ID, settings);
                    m_Assets.BindTexture(asset, handle);
                });
        }

        // Treat missing optional channels as no upload; otherwise create
        // import settings from each channel's intended color/data semantics.
        // The subsequent MakePBRMaterial resolves the now-bound GPU handle.
        void EnsureMaterialTexture(
            const std::optional<kairo::assets::TextureAssetHandle>& texture,
            kairo::assets::TextureColorSpace colorSpace,
            bool normalMap)
        {
            if (!texture.has_value()) return;
            kairo::assets::TextureImportSettings settings;
            settings.ColorSpace = colorSpace;
            settings.NormalMap = normalMap;
            (void)EnsureTexture(*texture, settings);
        }

        // Load typed material artifacts and their textures first, then builtin/imported
        // meshes and glTF scene primitives. Each successful native upload is tracked
        // for cleanup; bindings map persistent AssetIDs to runtime handles.
        // Three passes: (1) material artifacts and referenced texture mips,
        // plus environment; (2) builtin/OBJ mesh GPU uploads; (3) glTF scene
        // imports and per-primitive mesh uploads. KairoAssets importer/cache
        // owns CPU artifacts/provenance; renderer owns native allocations;
        // RenderAssetBindings maps durable AssetIDs to process-local handles.
        // The constructor catch invokes ReleaseResources on partial import.
        void LoadAssets()
        {
            // The registry is immutable during this load. Reuse its sorted
            // metadata snapshot rather than copying and sorting it three times.
            const auto metadataSnapshot = m_Project.Assets().Snapshot();
            for (const auto& metadata : metadataSnapshot)
            {
                if (metadata.Type != kairo::assets::AssetType::Material ||
                    metadata.Origin == kairo::assets::AssetOrigin::Builtin) continue;
                const auto artifact = kairo::runtime::renderbridge::LoadRenderMaterial(
                    m_Project.Root(), { metadata.ID }, m_Project.Assets());
                EnsureMaterialTexture(artifact.Textures.BaseColor,
                    kairo::assets::TextureColorSpace::SRGB, false);
                EnsureMaterialTexture(artifact.Textures.Normal,
                    kairo::assets::TextureColorSpace::Linear, true);
                EnsureMaterialTexture(artifact.Textures.MetallicRoughness,
                    kairo::assets::TextureColorSpace::Linear, false);
                EnsureMaterialTexture(artifact.Textures.Emissive,
                    kairo::assets::TextureColorSpace::SRGB, false);
                EnsureMaterialTexture(artifact.Textures.Occlusion,
                    kairo::assets::TextureColorSpace::Linear, false);
                m_Assets.BindMaterial({ metadata.ID }, kairo::renderer::MakePBRMaterial(
                    artifact, [this](kairo::assets::TextureAssetHandle texture)
                    {
                        return m_Assets.ResolveTexture(texture);
                    }));
            }

            if (const auto environment = m_Project.Scene().ActiveEnvironment();
                environment.has_value())
                EnsureMaterialTexture(
                    m_Project.Scene().Environment(*environment).EnvironmentTexture,
                    kairo::assets::TextureColorSpace::Linear, false);

            for (const auto& metadata : metadataSnapshot)
            {
                if (metadata.Type != kairo::assets::AssetType::Mesh) continue;
                std::optional<kairo::renderer::Mesh> mesh;
                if (metadata.Origin == kairo::assets::AssetOrigin::Builtin)
                    mesh = kairo::runtime::renderbridge::MakeBuiltinRenderMesh(metadata);
                else if (metadata.Origin == kairo::assets::AssetOrigin::SourceFile)
                    mesh = kairo::runtime::renderbridge::ImportRenderMesh(
                        m_Project.Root(), { metadata.ID }, m_Project.Assets(),
                        m_Imports, m_Cache).Geometry;
                if (!mesh.has_value())
                    throw std::invalid_argument(
                        "Unsupported runtime mesh asset: " + metadata.ID.ToString());
                (void)detail::CreateTrackBind(m_OwnedMeshes,
                    [this, &mesh] { return m_Renderer.CreateMesh(*mesh); },
                    [this, &metadata](kairo::renderer::MeshHandle handle)
                    {
                        m_Assets.BindMesh({ metadata.ID }, handle);
                    });
            }

            for (const auto& metadata : metadataSnapshot)
            {
                if (metadata.Type != kairo::assets::AssetType::Scene ||
                    metadata.Origin != kairo::assets::AssetOrigin::SourceFile) continue;
                // Resolve glTF image URI relative to its registered source scene path, require
                // matching typed project Texture2D metadata, and derive sRGB/linear/normal
                // import flags from the glTF semantic before GPU upload.
                const auto resolveTexture = [this, &metadata](
                    std::string_view uri, kairo::assets::TextureSemantic semantic)
                {
                    const auto path = (metadata.Path.parent_path() /
                        std::filesystem::path(uri)).lexically_normal();
                    const auto texture = m_Project.Assets().FindByPath(path);
                    if (!texture.has_value() ||
                        texture->Type != kairo::assets::AssetType::Texture2D)
                        throw std::invalid_argument(
                            "glTF image URI is not registered as a project texture asset: " +
                            path.generic_string());
                    kairo::assets::TextureImportSettings settings;
                    settings.ColorSpace = semantic == kairo::assets::TextureSemantic::Color
                        ? kairo::assets::TextureColorSpace::SRGB
                        : kairo::assets::TextureColorSpace::Linear;
                    settings.NormalMap = semantic == kairo::assets::TextureSemantic::Normal;
                    return EnsureTexture({ texture->ID }, settings);
                };

                auto imported =
                    kairo::runtime::renderbridge::ImportRenderGltfSceneWithSource(
                        m_Project.Root(), { metadata.ID }, m_Project.Assets(),
                        m_Imports, m_Cache, resolveTexture);
                std::vector<kairo::renderer::MeshHandle> handles;
                handles.reserve(imported.RenderAsset.Primitives.size());
                for (const auto& primitive : imported.RenderAsset.Primitives)
                {
                    const auto handle = detail::CreateTrackBind(m_OwnedMeshes,
                        [this, &primitive] { return m_Renderer.CreateMesh(primitive.Geometry); },
                        [](kairo::renderer::MeshHandle) {});
                    handles.push_back(handle);
                }
                m_Assets.BindGltfScene({ metadata.ID }, std::move(imported.Source),
                    imported.RenderAsset, handles,imported.CacheKey,std::move(imported.AuthoredRevision),
                    std::move(imported.Controller),std::move(imported.ControllerRevision));
            }
        }

        struct ControllerBinding
        {
            kairo::assets::AssetID Asset;
            kairo::engine::AnimationControllerRuntime Runtime;
            kairo::engine::AnimationParameterSet Parameters;
        };
        std::unordered_map<std::uint32_t,ControllerBinding> m_Controllers;
        ControllerBinding& EnsureController(kairo::engine::Entity entity,kairo::assets::AssetID asset,
            const kairo::engine::AnimationControllerDefinition& definition,
            const kairo::assets::GltfSceneArtifactData& source)
        {
            auto it=m_Controllers.find(entity.Value);
            if(it!=m_Controllers.end()&&it->second.Asset==asset)return it->second;
            ControllerBinding binding{asset,kairo::engine::AnimationControllerRuntime(definition,source),{}};
            for(const auto& transition:definition.Transitions)
                for(const auto& condition:transition.Conditions)
                    binding.Parameters.SetBool(condition.Parameter,false);
            return m_Controllers.insert_or_assign(entity.Value,std::move(binding)).first->second;
        }

        kairo::renderer::RendererRuntime& m_Renderer;
        const RuntimeProject& m_Project;
        kairo::runtime::renderbridge::RenderAssetBindings m_Assets;
        kairo::runtime::renderbridge::SceneAnimationOverrides m_Animations;
        std::unordered_map<std::uint32_t, RuntimeAnimationPlaybackState>
            m_AnimationStates;
        kairo::assets::ImportDatabase m_Imports;
        kairo::assets::DerivedDataCache m_Cache;
        std::unordered_map<kairo::assets::AssetID,
            kairo::assets::TextureImportSettings, kairo::assets::AssetIDHash>
            m_TextureSettings;
        std::vector<kairo::renderer::MeshHandle> m_OwnedMeshes;
        std::vector<kairo::renderer::TextureHandle> m_OwnedTextures;
    };
}
