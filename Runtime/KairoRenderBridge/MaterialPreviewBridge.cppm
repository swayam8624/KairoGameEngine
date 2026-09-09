export module Kairo.Runtime.RenderBridge.MaterialPreview;

import Kairo.Editor.MaterialPreviewAuthoring;
import Kairo.Foundation.Math;
import Kairo.Renderer;

export namespace kairo::runtime::renderbridge
{
    struct MaterialPreviewScene final
    {
        MaterialPreviewScene();
        MaterialPreviewScene(const MaterialPreviewScene&);
        MaterialPreviewScene(MaterialPreviewScene&&);
        MaterialPreviewScene& operator=(const MaterialPreviewScene&);
        MaterialPreviewScene& operator=(MaterialPreviewScene&&);
        ~MaterialPreviewScene();

        kairo::renderer::RenderScene Scene;
        kairo::renderer::CameraPose Camera;
    };

    [[nodiscard]] kairo::renderer::Mesh MakeMaterialPreviewMesh(
        kairo::editor::MaterialPreviewShape shape);

    [[nodiscard]] MaterialPreviewScene BuildMaterialPreviewScene(
        kairo::renderer::MeshHandle mesh,
        const kairo::editor::MaterialPreviewRequest& request,
        const kairo::renderer::MaterialTextureResolver& resolveTexture = {});

    /// Owning convenience wrapper for a live RendererRuntime. Allocation-heavy
    /// construction and teardown live in the ordinary implementation unit so
    /// the module interface remains a declaration-only ABI boundary.
    class LiveMaterialPreview final
    {
    public:
        LiveMaterialPreview(kairo::renderer::RendererRuntime& renderer,
            const kairo::editor::MaterialPreviewRequest& request,
            kairo::renderer::MaterialTextureResolver resolveTexture = {});

        LiveMaterialPreview(const LiveMaterialPreview&) = delete;
        LiveMaterialPreview& operator=(const LiveMaterialPreview&) = delete;
        ~LiveMaterialPreview();

        [[nodiscard]] const MaterialPreviewScene& Preview() const noexcept;

    private:
        kairo::renderer::RendererRuntime& m_Renderer;
        kairo::renderer::MeshHandle m_Mesh = kairo::renderer::InvalidMeshHandle;
        MaterialPreviewScene m_Preview;
    };
}
