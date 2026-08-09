module;

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
    /// Runtime-owned bridge from persistent project mesh IDs to process-local
    /// renderer handles. It uses KairoAssets importers and derived artifacts;
    /// this layer never parses source geometry or owns native API objects directly.
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

        /// Output: all active visible mesh entities in stable scene order.
        /// World transforms include parent composition. Missing GPU bindings
        /// fail explicitly rather than silently producing an empty window.
        [[nodiscard]] kairo::renderer::RenderScene BuildScene() const
        {
            return kairo::runtime::renderbridge::BuildRenderScene(
                m_Project.Scene(), m_Assets);
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
                // RendererRuntime also owns every allocation. Explicit release
                // keeps normal lifetimes prompt; exceptional teardown remains
                // non-throwing and lets the renderer perform final cleanup.
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
                const auto imported = kairo::runtime::renderbridge::ImportRenderGltfScene(
                    m_Project.Root(), { metadata.ID }, m_Project.Assets(),
                    m_Imports, m_Cache, resolveTexture);
                std::vector<kairo::runtime::renderbridge::RenderAssetBindings::ScenePrimitive>
                    primitives;
                primitives.reserve(imported.Primitives.size());
                for (const auto& primitive : imported.Primitives)
                {
                    const auto handle = m_Renderer.CreateMesh(primitive.Geometry);
                    m_OwnedMeshes.push_back(handle);
                    primitives.push_back(
                        { handle, primitive.Material, primitive.LocalToAsset });
                }
                m_Assets.BindScene({ metadata.ID }, std::move(primitives));
            }
        }

        kairo::renderer::RendererRuntime& m_Renderer;
        const RuntimeProject& m_Project;
        kairo::runtime::renderbridge::RenderAssetBindings m_Assets;
        kairo::assets::ImportDatabase m_Imports;
        kairo::assets::DerivedDataCache m_Cache;
        std::unordered_map<kairo::assets::AssetID,
            kairo::assets::TextureImportSettings, kairo::assets::AssetIDHash>
            m_TextureSettings;
        std::vector<kairo::renderer::MeshHandle> m_OwnedMeshes;
        std::vector<kairo::renderer::TextureHandle> m_OwnedTextures;
    };
}
