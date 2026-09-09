module;

#include <cmath>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
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
    /// Runtime-owned bridge from persistent project asset IDs to process-local
    /// renderer handles. It owns GPU allocations plus the transient animation
    /// playback state required to turn imported glTF clips into skinned draws.
    class RuntimeRenderBridge final
    {
    public:
        RuntimeRenderBridge(kairo::renderer::RendererRuntime& renderer, const RuntimeProject& project)
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

        /// Advance the default runtime animation policy. Every active imported
        /// glTF scene that contains at least one clip plays clip zero in a loop.
        /// The transient clock intentionally lives outside authored Scene data;
        /// editor/gameplay clip selection can override this policy in a later
        /// authoring layer without coupling Renderer to gameplay state.
        void StepAnimations(float deltaSeconds)
        {
            if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f)
                throw std::invalid_argument(
                    "Runtime animation delta must be finite and non-negative.");

            for (const auto entity : m_Project.Scene().SceneInstanceEntities())
            {
                const auto& instance = m_Project.Scene().SceneInstance(entity);
                const auto* source = m_Assets.ResolveGltfSource(instance.SceneAsset);
                if (source == nullptr || source->Animations.empty())
                {
                    m_Animations.Clear(entity);
                    m_AnimationTimes.erase(entity.Value);
                    continue;
                }

                float& time = m_AnimationTimes[entity.Value];
                time += deltaSeconds;
                const float duration = source->Animations.front().DurationSeconds();
                if (duration > 0.0f && time >= duration)
                    time = std::fmod(time, duration);
                if (!std::isfinite(time))
                    throw std::overflow_error("Runtime animation clock overflowed.");

                m_Animations.Set(entity, {
                    .ClipIndex = 0u,
                    .TimeSeconds = time,
                    .TimeMode = kairo::engine::AnimationTimeMode::Loop
                });
            }
        }

        [[nodiscard]] std::size_t AnimatedSceneInstanceCount() const noexcept
        {
            return m_AnimationTimes.size();
        }

        /// Output: all active visible mesh/scene entities in stable scene order.
        /// Imported animated scenes are sampled using the runtime-owned clocks;
        /// missing GPU bindings fail explicitly rather than disappearing.
        [[nodiscard]] kairo::renderer::RenderScene BuildScene() const
        {
            return kairo::runtime::renderbridge::BuildRenderScene(
                m_Project.Scene(), m_Assets, m_Animations);
        }

        /// Output: primary authored camera pose, or the renderer's documented
        /// default when the scene has no camera. Multiple primary cameras are
        /// rejected because frame ownership must remain deterministic.
        [[nodiscard]] kairo::renderer::CameraPose CameraPose() const
        {
            return kairo::runtime::renderbridge::SelectRenderCamera(
                m_Project.Scene());
        }

    private:
        /// Task: release successfully-created GPU resources in reverse
        /// dependency order. It is also used during constructor rollback, so
        /// every operation is non-throwing and vectors are cleared idempotently.
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
        std::unordered_map<std::uint32_t, float> m_AnimationTimes;
        kairo::assets::ImportDatabase m_Imports;
        kairo::assets::DerivedDataCache m_Cache;
        std::unordered_map<kairo::assets::AssetID,
            kairo::assets::TextureImportSettings, kairo::assets::AssetIDHash>
            m_TextureSettings;
        std::vector<kairo::renderer::MeshHandle> m_OwnedMeshes;
        std::vector<kairo::renderer::TextureHandle> m_OwnedTextures;
    };
}
