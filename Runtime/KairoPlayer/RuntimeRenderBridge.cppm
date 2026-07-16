module;

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

export module Kairo.Player.RuntimeRenderBridge;

import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Renderer;
import Kairo.Player.RuntimeProject;

export namespace kairo::player
{
    /// Runtime-owned bridge from persistent project mesh IDs to process-local
    /// renderer handles. It uses KairoAssets importers and derived artifacts;
    /// this layer never parses source geometry or owns Vulkan objects directly.
    class RuntimeRenderBridge final
    {
    public:
        RuntimeRenderBridge(kairo::renderer::RendererRuntime& renderer, const RuntimeProject& project)
            : m_Renderer(renderer), m_Project(project), m_Cache(project.Root() / ".kairo" / "derived")
        {
            std::unordered_set<kairo::assets::AssetID, kairo::assets::AssetIDHash> seen;
            std::vector<kairo::assets::AssetID> required;
            for (const auto entity : project.Scene().RenderableEntities())
            {
                const auto asset = project.Scene().MeshRenderer(entity).MeshAsset.ID;
                if (seen.insert(asset).second) required.push_back(asset);
            }
            for (const auto asset : required)
            {
                const auto metadata = project.Assets().Resolve(kairo::assets::MeshAssetHandle{ asset });
                std::optional<kairo::renderer::Mesh> mesh;
                if (metadata.Origin == kairo::assets::AssetOrigin::Builtin)
                    mesh = MakeBuiltinMesh(metadata);
                else if (metadata.Origin == kairo::assets::AssetOrigin::SourceFile)
                    mesh = ImportSourceMesh(metadata);
                if (!mesh.has_value()) continue;
                const auto handle = m_Renderer.CreateMesh(*mesh);
                m_Meshes.emplace(metadata.ID, handle);
                m_OwnedMeshes.push_back(handle);
            }
        }

        ~RuntimeRenderBridge() noexcept
        {
            for (auto iterator = m_OwnedMeshes.rbegin(); iterator != m_OwnedMeshes.rend(); ++iterator)
            {
                // RendererRuntime also owns every allocation. Explicit release
                // keeps normal lifetimes prompt; exceptional teardown remains
                // non-throwing and lets the renderer perform final cleanup.
                try { m_Renderer.DestroyMesh(*iterator); }
                catch (...) {}
            }
        }

        RuntimeRenderBridge(const RuntimeRenderBridge&) = delete;
        RuntimeRenderBridge& operator=(const RuntimeRenderBridge&) = delete;

        /// Output: all active visible mesh entities in stable scene order.
        /// World transforms include parent composition. Missing GPU bindings
        /// fail explicitly rather than silently producing an empty window.
        [[nodiscard]] kairo::renderer::RenderScene BuildScene() const
        {
            kairo::renderer::RenderScene output;
            for (const auto entity : m_Project.Scene().RenderableEntities())
            {
                const auto& component = m_Project.Scene().MeshRenderer(entity);
                (void)m_Project.Assets().Resolve(component.MaterialAsset);
                const auto found = m_Meshes.find(component.MeshAsset.ID);
                if (found == m_Meshes.end())
                    throw std::runtime_error("No runtime mesh binding exists for asset " + component.MeshAsset.ID.ToString());
                output.Add({ found->second,
                    kairo::foundation::math::ToMatrix4(m_Project.Scene().WorldTransform(entity)),
                    {}, entity.Value });
            }
            return output;
        }

        /// Output: primary authored camera pose, or the renderer's documented
        /// default when the scene has no camera. Multiple primary cameras are
        /// rejected because frame ownership must remain deterministic.
        [[nodiscard]] kairo::renderer::CameraPose CameraPose() const
        {
            std::optional<kairo::engine::Entity> selected;
            for (const auto entity : m_Project.Scene().Entities())
            {
                if (!m_Project.Scene().IsActiveInHierarchy(entity) || !m_Project.Scene().HasCamera(entity)) continue;
                const auto& camera = m_Project.Scene().Camera(entity);
                if (!camera.Primary && selected.has_value()) continue;
                if (camera.Primary && selected.has_value() && m_Project.Scene().Camera(*selected).Primary)
                    throw std::runtime_error("Scene contains more than one active primary camera.");
                if (camera.Primary || !selected.has_value()) selected = entity;
            }
            if (!selected.has_value()) return {};
            const auto transform = m_Project.Scene().WorldTransform(*selected);
            return { transform.Translation, transform.Translation + transform.Forward(), transform.Up() };
        }

    private:
        [[nodiscard]] static std::optional<kairo::renderer::Mesh> MakeBuiltinMesh(
            const kairo::assets::AssetMetadata& metadata)
        {
            if (metadata.Importer == "kairo.builtin.cube") return kairo::renderer::Mesh::MakeCube();
            if (metadata.Importer == "kairo.builtin.plane") return kairo::renderer::Mesh::MakePlane();
            if (metadata.Importer == "kairo.builtin.uv-sphere") return kairo::renderer::Mesh::MakeUVSphere();
            if (metadata.Importer == "kairo.builtin.cylinder") return kairo::renderer::Mesh::MakeCylinder();
            throw std::invalid_argument("Unsupported builtin mesh importer: " + metadata.Importer);
        }

        [[nodiscard]] kairo::renderer::Mesh ImportSourceMesh(
            const kairo::assets::AssetMetadata& metadata)
        {
            kairo::assets::OBJMeshImporter importer;
            if (metadata.Importer != importer.Identifier())
                throw std::invalid_argument("Unsupported source mesh importer: " + metadata.Importer);
            kairo::assets::ImportRecord record{ metadata.ID, metadata.Path,
                importer.Identifier(), importer.Version(), {}, {}, 1u };
            const auto outcome = kairo::assets::ImportSourceAsset(
                m_Project.Root(), std::move(record), importer,
                m_Project.Assets(), m_Imports, m_Cache);
            return kairo::renderer::Mesh::FromArtifact(
                kairo::assets::ParseMeshDerivedArtifact(outcome.Artifact));
        }

        kairo::renderer::RendererRuntime& m_Renderer;
        const RuntimeProject& m_Project;
        kairo::assets::ImportDatabase m_Imports;
        kairo::assets::DerivedDataCache m_Cache;
        std::unordered_map<kairo::assets::AssetID, kairo::renderer::MeshHandle,
            kairo::assets::AssetIDHash> m_Meshes;
        std::vector<kairo::renderer::MeshHandle> m_OwnedMeshes;
    };
}
