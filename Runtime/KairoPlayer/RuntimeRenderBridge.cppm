module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

export module Kairo.Player.RuntimeRenderBridge;

import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Renderer;
import Kairo.Runtime.RealtimeSceneBridge;
import Kairo.Player.RuntimeProject;

export namespace kairo::player
{
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
    class RuntimeRenderBridge final
    {
    public:
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
                ReleaseResources();
                throw;
            }
        }

        ~RuntimeRenderBridge() noexcept { ReleaseResources(); }

        RuntimeRenderBridge(const RuntimeRenderBridge&) = delete;
        RuntimeRenderBridge& operator=(const RuntimeRenderBridge&) = delete;

        [[nodiscard]] std::size_t AnimationClipCount(
            kairo::engine::Entity entity) const
        {
            return RequireAnimationSource(entity).Animations.size();
        }

        [[nodiscard]] std::optional<std::uint32_t> FindAnimationClip(
            kairo::engine::Entity entity, std::string_view name) const
        {
            const auto& source = RequireAnimationSource(entity);
            for (std::size_t index = 0u; index < source.Animations.size(); ++index)
                if (source.Animations[index].Name == name)
                    return static_cast<std::uint32_t>(index);
            return std::nullopt;
        }

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

        void SetAnimationPaused(kairo::engine::Entity entity, bool paused)
        {
            auto& state = RequireAnimationState(entity);
            state.Paused = paused;
            state.Explicit = true;
        }

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
        void ClearAnimationControl(kairo::engine::Entity entity) noexcept
        {
            m_AnimationStates.erase(entity.Value);
            m_Animations.Clear(entity);
        }

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
                const auto* source = m_Assets.ResolveGltfSource(instance.SceneAsset);
                if (source == nullptr || source->Animations.empty())
                {
                    m_Animations.Clear(entity);
                    m_AnimationStates.erase(entity.Value);
                    continue;
                }

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

        [[nodiscard]] std::size_t AnimatedSceneInstanceCount() const noexcept
        {
            return m_AnimationStates.size();
        }

        [[nodiscard]] kairo::renderer::RenderScene BuildScene() const
        {
            return kairo::runtime::renderbridge::BuildRenderScene(
                m_Project.Scene(), m_Assets, m_Animations);
        }

        [[nodiscard]] kairo::renderer::CameraPose CameraPose() const
        {
            return kairo::runtime::renderbridge::SelectRenderCamera(
                m_Project.Scene());
        }

    private:
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

        void ReleaseResources() noexcept
        {
            for (auto iterator = m_OwnedMeshes.rbegin(); iterator != m_OwnedMeshes.rend(); ++iterator)
            {
                try { m_Renderer.DestroyMesh(*iterator); }
                catch (...) {}
            }
            for (auto iterator = m_OwnedTextures.rbegin();
                iterator != m_OwnedTextures.rend(); ++iterator)
            {
                try { m_Renderer.DestroyTexture(*iterator); }
                catch (...) {}
            }
            m_OwnedMeshes.clear();
            m_OwnedTextures.clear();
        }

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
            const auto handle = m_Renderer.CreateTexture(texture);
            m_TextureSettings.emplace(asset.ID, settings);
            m_Assets.BindTexture(asset, handle);
            m_OwnedTextures.push_back(handle);
            return handle;
        }

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

        void LoadAssets()
        {
            for (const auto& metadata : m_Project.Assets().Snapshot())
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

            for (const auto& metadata : m_Project.Assets().Snapshot())
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
                const auto handle = m_Renderer.CreateMesh(*mesh);
                m_Assets.BindMesh({ metadata.ID }, handle);
                m_OwnedMeshes.push_back(handle);
            }

            for (const auto& metadata : m_Project.Assets().Snapshot())
            {
                if (metadata.Type != kairo::assets::AssetType::Scene ||
                    metadata.Origin != kairo::assets::AssetOrigin::SourceFile) continue;
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
                    const auto handle = m_Renderer.CreateMesh(primitive.Geometry);
                    m_OwnedMeshes.push_back(handle);
                    handles.push_back(handle);
                }
                m_Assets.BindGltfScene({ metadata.ID }, std::move(imported.Source),
                    imported.RenderAsset, handles);
            }
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
