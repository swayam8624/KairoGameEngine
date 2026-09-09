module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

export module Kairo.Runtime.RealtimeSceneBridge;

import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Renderer;

export namespace kairo::runtime::renderbridge
{
    /// Complete CPU-side result of preparing one source mesh for rendering.
    struct RenderMeshImport final
    {
        kairo::renderer::Mesh Geometry;
        kairo::assets::DerivedDataKey CacheKey;
        bool CacheHit = false;
    };

    /// Task: import OBJ geometry through KairoAssets and adapt only the
    /// validated portable artifact into KairoRenderer geometry.
    [[nodiscard]] inline RenderMeshImport ImportRenderMesh(
        const std::filesystem::path& projectRoot,
        kairo::assets::MeshAssetHandle asset,
        const kairo::assets::AssetRegistry& registry,
        kairo::assets::ImportDatabase& imports,
        const kairo::assets::DerivedDataCache& cache)
    {
        const auto metadata = registry.Resolve(asset);
        if (metadata.Origin != kairo::assets::AssetOrigin::SourceFile)
            throw std::invalid_argument("Render mesh import requires a source-file asset.");
        kairo::assets::OBJMeshImporter importer;
        if (metadata.Importer != importer.Identifier())
            throw std::invalid_argument("Unsupported render mesh importer: " + metadata.Importer);
        kairo::assets::ImportRecord record{ metadata.ID, metadata.Path,
            importer.Identifier(), importer.Version(), {}, {}, 1u };
        auto outcome = kairo::assets::ImportSourceAsset(
            projectRoot, std::move(record), importer, registry, imports, cache);
        return { kairo::renderer::Mesh::FromArtifact(
            kairo::assets::ParseMeshDerivedArtifact(outcome.Artifact)),
            outcome.Key, outcome.CacheHit };
    }

    /// Complete CPU-side glTF import needed by animation-aware runtime binding.
    /// Keep the validated source artifact beside the renderer adaptation so node,
    /// skin, rest-pose, and clip metadata are not discarded after mesh upload.
    struct RenderGltfSceneImport final
    {
        kairo::assets::GltfSceneArtifactData Source;
        kairo::renderer::GltfRenderAsset RenderAsset;
        kairo::assets::DerivedDataKey CacheKey;
        bool CacheHit = false;
    };

    [[nodiscard]] inline RenderGltfSceneImport ImportRenderGltfSceneWithSource(
        const std::filesystem::path& projectRoot,
        kairo::assets::SceneAssetHandle asset,
        const kairo::assets::AssetRegistry& registry,
        kairo::assets::ImportDatabase& imports,
        const kairo::assets::DerivedDataCache& cache,
        const kairo::renderer::GltfTextureResolver& resolveTexture = {})
    {
        const auto metadata = registry.Resolve(asset);
        kairo::assets::GltfSceneImporter importer;
        if (metadata.Origin != kairo::assets::AssetOrigin::SourceFile ||
            metadata.Importer != importer.Identifier())
            throw std::invalid_argument(
                "Render scene import requires a source glTF asset using kairo.gltf.scene.");
        kairo::assets::ImportRecord record{ metadata.ID, metadata.Path,
            importer.Identifier(), importer.Version(), {}, {}, 1u };
        auto outcome = kairo::assets::ImportSourceAsset(projectRoot, std::move(record),
            importer, registry, imports, cache);
        auto source = kairo::assets::ParseGltfSceneDerivedArtifact(outcome.Artifact);
        auto renderAsset = kairo::renderer::MakeGltfRenderAsset(source, resolveTexture);
        return { std::move(source), std::move(renderAsset), outcome.Key,
            outcome.CacheHit };
    }

    /// Compatibility projection for callers that only need static render data.
    [[nodiscard]] inline kairo::renderer::GltfRenderAsset ImportRenderGltfScene(
        const std::filesystem::path& projectRoot,
        kairo::assets::SceneAssetHandle asset,
        const kairo::assets::AssetRegistry& registry,
        kairo::assets::ImportDatabase& imports,
        const kairo::assets::DerivedDataCache& cache,
        const kairo::renderer::GltfTextureResolver& resolveTexture = {})
    {
        return ImportRenderGltfSceneWithSource(projectRoot, asset, registry,
            imports, cache, resolveTexture).RenderAsset;
    }

    /// Task: import a texture with explicit color/data semantics. Those
    /// semantics participate in the derived-data key and therefore cannot be
    /// guessed by a graphics backend.
    [[nodiscard]] inline kairo::assets::TextureArtifactData ImportRenderTexture(
        const std::filesystem::path& projectRoot,
        kairo::assets::TextureAssetHandle asset,
        const kairo::assets::TextureImportSettings& settings,
        const kairo::assets::AssetRegistry& registry,
        kairo::assets::ImportDatabase& imports,
        const kairo::assets::DerivedDataCache& cache)
    {
        const auto metadata = registry.Resolve(asset);
        kairo::assets::StbTextureImporter importer;
        if (metadata.Origin != kairo::assets::AssetOrigin::SourceFile ||
            metadata.Importer != importer.Identifier())
            throw std::invalid_argument(
                "Render texture import requires a source texture using kairo.texture.stb.");
        kairo::assets::ImportRecord record{ metadata.ID, metadata.Path,
            importer.Identifier(), importer.Version(),
            kairo::assets::CanonicalTextureImportSettings(settings), {}, 1u };
        auto outcome = kairo::assets::ImportSourceAsset(projectRoot, std::move(record),
            importer, registry, imports, cache);
        return kairo::assets::ParseTextureDerivedArtifact(outcome.Artifact);
    }

    /// Task: load the canonical generated material artifact. Source parsing and
    /// backend-specific material guesses are intentionally not permitted here.
    [[nodiscard]] inline kairo::assets::MaterialArtifactData LoadRenderMaterial(
        const std::filesystem::path& projectRoot,
        kairo::assets::MaterialAssetHandle asset,
        const kairo::assets::AssetRegistry& registry)
    {
        const auto metadata = registry.Resolve(asset);
        if (metadata.Type != kairo::assets::AssetType::Material ||
            metadata.Origin == kairo::assets::AssetOrigin::Builtin)
            throw std::invalid_argument(
                "Render material loading requires a non-builtin material asset.");
        const auto path = projectRoot / metadata.Path;
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) throw std::runtime_error("Cannot open material artifact: " + path.string());
        const auto length = input.tellg();
        if (length < 0)
            throw std::runtime_error("Cannot measure material artifact: " + path.string());
        std::vector<std::byte> bytes(static_cast<std::size_t>(length));
        input.seekg(0);
        if (!bytes.empty()) input.read(reinterpret_cast<char*>(bytes.data()), length);
        if (!input) throw std::runtime_error("Cannot read material artifact: " + path.string());
        return kairo::assets::ParseMaterialDerivedArtifact(
            kairo::assets::ParseDerivedArtifact(bytes));
    }

    /// Input: one registered builtin mesh metadata record.
    /// Output: canonical procedural geometry, or std::nullopt when the record
    /// is not a builtin mesh.
    /// Task: give Editor and Player one mapping from persistent primitive IDs
    /// to renderer geometry. Unknown builtin importers fail instead of silently
    /// disappearing from one consumer.
    [[nodiscard]] inline std::optional<kairo::renderer::Mesh> MakeBuiltinRenderMesh(
        const kairo::assets::AssetMetadata& metadata)
    {
        if (metadata.Type != kairo::assets::AssetType::Mesh ||
            metadata.Origin != kairo::assets::AssetOrigin::Builtin)
            return std::nullopt;
        if (metadata.Importer == "kairo.builtin.cube") return kairo::renderer::Mesh::MakeCube();
        if (metadata.Importer == "kairo.builtin.plane") return kairo::renderer::Mesh::MakePlane();
        if (metadata.Importer == "kairo.builtin.uv-sphere") return kairo::renderer::Mesh::MakeUVSphere();
        if (metadata.Importer == "kairo.builtin.cylinder") return kairo::renderer::Mesh::MakeCylinder();
        return std::nullopt;
    }

    /// Input: one validated EngineCore scene.
    /// Output: the active primary authored camera, the first active camera when
    /// no camera is marked primary, or KairoRenderer's default pose when the
    /// scene has no active camera.
    /// Task: keep Player, packaged games, previews, and capture tools on one
    /// deterministic camera-selection rule. Multiple active primary cameras
    /// are rejected because silently choosing one makes runtime output depend
    /// on incidental container order.
    [[nodiscard]] inline kairo::renderer::CameraPose SelectRenderCamera(
        const kairo::engine::Scene& scene)
    {
        std::optional<kairo::engine::Entity> selected;
        for (const auto entity : scene.Entities())
        {
            if (!scene.IsActiveInHierarchy(entity) || !scene.HasCamera(entity))
                continue;
            const auto& camera = scene.Camera(entity);
            if (!camera.Primary && selected.has_value()) continue;
            if (camera.Primary && selected.has_value() &&
                scene.Camera(*selected).Primary)
                throw std::runtime_error(
                    "Scene contains more than one active primary camera.");
            if (camera.Primary || !selected.has_value()) selected = entity;
        }
        if (!selected.has_value()) return {};
        const auto transform = scene.WorldTransform(*selected);
        return { transform.Translation,
            transform.Translation + transform.Forward(), transform.Up() };
    }

    /// Process-local mapping from persistent KairoAssets identities to opaque
    /// KairoRenderer handles. The bridge owns no GPU resources; the host that
    /// created each handle remains responsible for destruction.
    class RenderAssetBindings final
    {
    public:
        struct ScenePrimitive final
        {
            kairo::renderer::MeshHandle Mesh = kairo::renderer::InvalidMeshHandle;
            kairo::renderer::PBRMaterial Material;
            kairo::foundation::math::Mat4f LocalToAsset =
                kairo::foundation::math::Mat4f::Identity();
            std::uint32_t NodeIndex = kairo::assets::GltfMissingIndex;
            std::uint32_t PrimitiveIndex = kairo::assets::GltfMissingIndex;
            std::uint32_t SkinIndex = kairo::assets::GltfMissingIndex;
        };

        struct SceneBinding final
        {
            std::vector<ScenePrimitive> Primitives;
            std::optional<kairo::assets::GltfSceneArtifactData> GltfSource;
        };

        explicit RenderAssetBindings(const kairo::assets::AssetRegistry& registry) noexcept
            : m_Registry(registry) {}

        void BindMesh(kairo::assets::MeshAssetHandle asset, kairo::renderer::MeshHandle handle)
        {
            (void)m_Registry.Resolve(asset);
            if (handle == kairo::renderer::InvalidMeshHandle)
                throw std::invalid_argument("A render mesh binding requires a valid handle.");
            if (!m_Meshes.emplace(asset.ID, handle).second)
                throw std::invalid_argument("A render mesh asset is already bound.");
        }

        [[nodiscard]] kairo::renderer::MeshHandle ResolveMesh(
            kairo::assets::MeshAssetHandle asset) const
        {
            (void)m_Registry.Resolve(asset);
            const auto found = m_Meshes.find(asset.ID);
            if (found == m_Meshes.end())
                throw std::out_of_range(
                    "No renderer mesh is bound for asset ID: " + asset.ID.ToString());
            return found->second;
        }

        void BindMaterial(kairo::assets::MaterialAssetHandle asset,
            kairo::renderer::PBRMaterial material)
        {
            (void)m_Registry.Resolve(asset);
            material.Validate();
            if (!m_Materials.emplace(asset.ID, material).second)
                throw std::invalid_argument("A render material asset is already bound.");
        }

        [[nodiscard]] kairo::renderer::PBRMaterial ResolveMaterial(
            kairo::assets::MaterialAssetHandle asset) const
        {
            const auto metadata = m_Registry.Resolve(asset);
            const auto found = m_Materials.find(asset.ID);
            if (found != m_Materials.end()) return found->second;
            if (metadata.Origin == kairo::assets::AssetOrigin::Builtin) return {};
            throw std::out_of_range(
                "No renderer material is bound for asset ID: " + asset.ID.ToString());
        }

        void BindTexture(kairo::assets::TextureAssetHandle asset,
            kairo::renderer::TextureHandle handle)
        {
            (void)m_Registry.Resolve(asset);
            if (handle == kairo::renderer::InvalidTextureHandle)
                throw std::invalid_argument("A render texture binding requires a valid handle.");
            if (!m_Textures.emplace(asset.ID, handle).second)
                throw std::invalid_argument("A render texture asset is already bound.");
        }

        [[nodiscard]] kairo::renderer::TextureHandle ResolveTexture(
            kairo::assets::TextureAssetHandle asset) const
        {
            (void)m_Registry.Resolve(asset);
            const auto found = m_Textures.find(asset.ID);
            if (found == m_Textures.end())
                throw std::out_of_range(
                    "No renderer texture is bound for asset ID: " + asset.ID.ToString());
            return found->second;
        }

        void BindScene(kairo::assets::SceneAssetHandle asset,
            std::vector<ScenePrimitive> primitives)
        {
            (void)m_Registry.Resolve(asset);
            ValidateScenePrimitives(primitives);
            if (!m_Scenes.emplace(asset.ID,
                SceneBinding{ std::move(primitives), std::nullopt }).second)
                throw std::invalid_argument("A render scene asset is already bound.");
        }

        /// Bind the renderer handles produced from one exact glTF source while
        /// retaining the node/skin identities needed by animation extraction.
        void BindGltfScene(kairo::assets::SceneAssetHandle asset,
            kairo::assets::GltfSceneArtifactData source,
            const kairo::renderer::GltfRenderAsset& renderAsset,
            std::span<const kairo::renderer::MeshHandle> meshHandles)
        {
            (void)m_Registry.Resolve(asset);
            kairo::assets::ValidateGltfSceneArtifactData(source);
            if (renderAsset.Primitives.size() != meshHandles.size())
                throw std::invalid_argument(
                    "glTF scene binding requires one GPU mesh handle per render primitive.");
            std::vector<ScenePrimitive> primitives;
            primitives.reserve(renderAsset.Primitives.size());
            for (std::size_t index = 0u; index < renderAsset.Primitives.size(); ++index)
            {
                const auto& render = renderAsset.Primitives[index];
                if (render.NodeIndex >= source.Nodes.size() ||
                    render.PrimitiveIndex >= source.Primitives.size())
                    throw std::invalid_argument(
                        "glTF render primitive references source metadata outside the bound artifact.");
                const auto& node = source.Nodes[render.NodeIndex];
                if (std::ranges::find(node.PrimitiveIndices, render.PrimitiveIndex) ==
                    node.PrimitiveIndices.end() || node.SkinIndex != render.SkinIndex)
                    throw std::invalid_argument(
                        "glTF render primitive metadata does not match the bound source artifact.");
                primitives.push_back({ meshHandles[index], render.Material,
                    render.LocalToAsset, render.NodeIndex, render.PrimitiveIndex,
                    render.SkinIndex });
            }
            ValidateScenePrimitives(primitives);
            if (!m_Scenes.emplace(asset.ID,
                SceneBinding{ std::move(primitives), std::move(source) }).second)
                throw std::invalid_argument("A render scene asset is already bound.");
        }

        [[nodiscard]] const std::vector<ScenePrimitive>& ResolveScene(
            kairo::assets::SceneAssetHandle asset) const
        {
            return ResolveSceneBinding(asset).Primitives;
        }

        [[nodiscard]] const kairo::assets::GltfSceneArtifactData* ResolveGltfSource(
            kairo::assets::SceneAssetHandle asset) const
        {
            const auto& binding = ResolveSceneBinding(asset);
            return binding.GltfSource.has_value() ? &*binding.GltfSource : nullptr;
        }

    private:
        const kairo::assets::AssetRegistry& m_Registry;
        std::unordered_map<kairo::assets::AssetID, kairo::renderer::MeshHandle,
            kairo::assets::AssetIDHash> m_Meshes;
        std::unordered_map<kairo::assets::AssetID, kairo::renderer::PBRMaterial,
            kairo::assets::AssetIDHash> m_Materials;
        std::unordered_map<kairo::assets::AssetID, kairo::renderer::TextureHandle,
            kairo::assets::AssetIDHash> m_Textures;
        std::unordered_map<kairo::assets::AssetID, SceneBinding,
            kairo::assets::AssetIDHash> m_Scenes;

        static void ValidateScenePrimitives(
            std::span<const ScenePrimitive> primitives)
        {
            if (primitives.empty())
                throw std::invalid_argument("A render scene binding requires primitives.");
            for (const auto& primitive : primitives)
            {
                if (primitive.Mesh == kairo::renderer::InvalidMeshHandle)
                    throw std::invalid_argument(
                        "A render scene primitive requires a valid mesh handle.");
                primitive.Material.Validate();
            }
        }

        [[nodiscard]] const SceneBinding& ResolveSceneBinding(
            kairo::assets::SceneAssetHandle asset) const
        {
            (void)m_Registry.Resolve(asset);
            const auto found = m_Scenes.find(asset.ID);
            if (found == m_Scenes.end())
                throw std::out_of_range(
                    "No renderer scene is bound for asset ID: " + asset.ID.ToString());
            return found->second;
        }
    };

    struct SceneAnimationPlayback final
    {
        std::uint32_t ClipIndex = 0u;
        float TimeSeconds = 0.0f;
        kairo::engine::AnimationTimeMode TimeMode =
            kairo::engine::AnimationTimeMode::Loop;

        void Validate() const
        {
            if (!std::isfinite(TimeSeconds))
                throw std::invalid_argument(
                    "Scene animation playback time must be finite.");
        }
    };

    /// Per-instance playback overrides. Animation state is intentionally kept
    /// outside serialized ECS in this first runtime slice so Editor/Player can
    /// drive preview/playback without changing the Scene schema prematurely.
    class SceneAnimationOverrides final
    {
        std::unordered_map<std::uint32_t, SceneAnimationPlayback> m_Playback;

    public:
        void Set(kairo::engine::Entity entity, SceneAnimationPlayback playback)
        {
            playback.Validate();
            m_Playback.insert_or_assign(entity.Value, playback);
        }

        void Clear(kairo::engine::Entity entity) noexcept
        {
            m_Playback.erase(entity.Value);
        }

        [[nodiscard]] const SceneAnimationPlayback* Find(
            kairo::engine::Entity entity) const noexcept
        {
            const auto found = m_Playback.find(entity.Value);
            return found == m_Playback.end() ? nullptr : &found->second;
        }

        [[nodiscard]] bool Empty() const noexcept { return m_Playback.empty(); }
    };

    /// Input: one validated EngineCore light and its world transform.
    /// Output: backend-neutral real-time light data.
    /// Task: apply the single Editor/Player photometric calibration: 10 klux
    /// for directional lights or 100 candela for local lights maps to one
    /// scene-radiance unit. Coordinates stay right-handed with no axis swap.
    [[nodiscard]] inline kairo::renderer::RenderLight MakeRenderLight(
        const kairo::engine::LightComponent& source,
        const kairo::foundation::math::Transformf& world)
    {
        source.Validate();
        kairo::renderer::RenderLight result;
        result.Position = world.Translation;
        result.Color = source.Color;
        result.Range = source.Range;
        result.InnerConeRadians = source.InnerConeRadians;
        result.OuterConeRadians = source.OuterConeRadians;
        result.CastShadows = source.Shadows != kairo::engine::ShadowPolicy::Disabled;
        switch (source.Type)
        {
            case kairo::engine::LightType::Directional:
                result.Type = kairo::renderer::RenderLightType::Directional;
                result.Direction = -world.Forward();
                result.Intensity = source.Intensity / 10'000.0f;
                break;
            case kairo::engine::LightType::Point:
                result.Type = kairo::renderer::RenderLightType::Point;
                result.Direction = world.Forward();
                result.Intensity = source.Intensity / 100.0f;
                break;
            case kairo::engine::LightType::Spot:
                result.Type = kairo::renderer::RenderLightType::Spot;
                result.Direction = world.Forward();
                result.Intensity = source.Intensity / 100.0f;
                break;
            case kairo::engine::LightType::RectangleArea:
                result.Type = kairo::renderer::RenderLightType::RectangleArea;
                result.Direction = world.Forward();
                result.Intensity = source.Intensity / 100.0f;
                result.AreaWidth = source.AreaWidth;
                result.AreaHeight = source.AreaHeight;
                break;
        }
        result.Validate();
        return result;
    }

    /// Input: one EngineCore scene, complete process-local asset bindings, and
    /// a non-zero camera/viewport layer mask.
    /// Output: stable renderer draws, lights, and environment shared by Editor
    /// and Player regardless of the selected graphics API.
    /// Degeneracy: missing bindings, invalid material state, singular transforms,
    /// and an empty layer mask fail before GPU command recording.
    [[nodiscard]] inline kairo::renderer::RenderScene BuildRenderScene(
        const kairo::engine::Scene& scene,
        const RenderAssetBindings& assets,
        const SceneAnimationOverrides& animations,
        std::uint64_t renderLayers = kairo::engine::AllRenderLayers)
    {
        if (renderLayers == 0u)
            throw std::invalid_argument("Render extraction requires a non-empty layer mask.");
        kairo::renderer::RenderScene result;
        for (const kairo::engine::Entity entity : scene.RenderableEntities())
        {
            const auto& source = scene.MeshRenderer(entity);
            if ((source.RenderLayers & renderLayers) == 0u) continue;
            result.Add({ .Mesh = assets.ResolveMesh(source.MeshAsset),
                .Model = kairo::foundation::math::ToMatrix4(scene.WorldTransform(entity)),
                .Material = assets.ResolveMaterial(source.MaterialForSlot(0u)),
                .ObjectID = entity.Value,
                .CastShadows = source.CastShadows,
                .ReceiveShadows = source.ReceiveShadows });
        }
        for (const kairo::engine::Entity entity : scene.SceneInstanceEntities())
        {
            const auto& source = scene.SceneInstance(entity);
            if ((source.RenderLayers & renderLayers) == 0u) continue;
            const auto world = kairo::foundation::math::ToMatrix4(scene.WorldTransform(entity));
            const auto& primitives = assets.ResolveScene(source.SceneAsset);
            const auto* gltf = assets.ResolveGltfSource(source.SceneAsset);
            const auto* playback = animations.Find(entity);
            if (gltf == nullptr)
            {
                if (playback != nullptr)
                    throw std::invalid_argument(
                        "Animation playback requires a glTF-aware scene binding.");
                for (const auto& primitive : primitives)
                    result.Add({ .Mesh = primitive.Mesh,
                        .Model = world * primitive.LocalToAsset,
                        .Material = primitive.Material,
                        .ObjectID = entity.Value,
                        .CastShadows = source.CastShadows,
                        .ReceiveShadows = source.ReceiveShadows });
                continue;
            }

            const auto pose = playback != nullptr
                ? kairo::engine::SampleGltfAnimation(*gltf, playback->ClipIndex,
                    playback->TimeSeconds, playback->TimeMode)
                : kairo::engine::BuildGltfRestPose(*gltf);
            const auto poseWorld = kairo::engine::ResolveGltfWorldMatrices(*gltf, pose);
            std::unordered_map<std::uint32_t, kairo::renderer::SkinPalette> palettes;
            for (const auto& primitive : primitives)
            {
                if (primitive.NodeIndex >= poseWorld.size())
                    throw std::logic_error(
                        "Bound glTF primitive node index is outside evaluated pose.");
                kairo::renderer::MeshDraw draw;
                draw.Mesh = primitive.Mesh;
                draw.Material = primitive.Material;
                draw.ObjectID = entity.Value;
                draw.CastShadows = source.CastShadows;
                draw.ReceiveShadows = source.ReceiveShadows;
                if (primitive.SkinIndex == kairo::assets::GltfMissingIndex)
                    draw.Model = world * poseWorld[primitive.NodeIndex];
                else
                {
                    // EngineCore returns jointWorld * inverseBind in imported-
                    // asset space. The outer entity transform belongs on Model;
                    // applying the glTF mesh-node world here would double it.
                    draw.Model = world;
                    auto found = palettes.find(primitive.SkinIndex);
                    if (found == palettes.end())
                    {
                        const auto evaluated =
                            kairo::engine::BuildGltfAssetSpaceSkinPalette(
                                *gltf, pose, primitive.SkinIndex);
                        kairo::renderer::SkinPalette converted;
                        converted.JointMatrices = evaluated.JointMatrices;
                        found = palettes.emplace(primitive.SkinIndex,
                            std::move(converted)).first;
                    }
                    draw.Skinning = found->second;
                }
                result.Add(std::move(draw));
            }
        }
        for (const kairo::engine::Entity entity : scene.LightEntities())
            if ((scene.Light(entity).RenderLayers & renderLayers) != 0u)
                result.AddLight(MakeRenderLight(scene.Light(entity), scene.WorldTransform(entity)));
        if (const auto active = scene.ActiveEnvironment(); active.has_value())
        {
            const auto& source = scene.Environment(*active);
            kairo::renderer::RenderEnvironment environment;
            environment.BackgroundColor = source.BackgroundColor;
            environment.AmbientIntensity = source.AmbientIntensity * source.EnvironmentIntensity;
            environment.EnvironmentIntensity = source.EnvironmentIntensity;
            environment.ExposureEV100 = source.ExposureEV100;
            if (source.EnvironmentTexture.has_value())
                environment.EnvironmentTexture = assets.ResolveTexture(*source.EnvironmentTexture);
            result.SetEnvironment(environment);
        }
        return result;
    }

    [[nodiscard]] inline kairo::renderer::RenderScene BuildRenderScene(
        const kairo::engine::Scene& scene,
        const RenderAssetBindings& assets,
        std::uint64_t renderLayers = kairo::engine::AllRenderLayers)
    {
        const SceneAnimationOverrides none;
        return BuildRenderScene(scene, assets, none, renderLayers);
    }
}
