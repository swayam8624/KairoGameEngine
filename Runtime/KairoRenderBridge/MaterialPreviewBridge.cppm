module;

#include <stdexcept>

export module Kairo.Runtime.RenderBridge.MaterialPreview;

import Kairo.Editor.MaterialPreviewAuthoring;
import Kairo.Foundation.Math;
import Kairo.Renderer;

export namespace kairo::runtime::renderbridge
{
    struct MaterialPreviewScene final
    {
        kairo::renderer::RenderScene Scene;
        kairo::renderer::CameraPose Camera;
    };

    [[nodiscard]] inline kairo::renderer::Mesh MakeMaterialPreviewMesh(
        kairo::editor::MaterialPreviewShape shape)
    {
        switch (shape)
        {
            case kairo::editor::MaterialPreviewShape::Sphere:
                return kairo::renderer::Mesh::MakeUVSphere(24u, 32u);
            case kairo::editor::MaterialPreviewShape::Plane:
                return kairo::renderer::Mesh::MakePlane();
        }
        throw std::invalid_argument("Material preview shape is invalid.");
    }

    [[nodiscard]] inline MaterialPreviewScene BuildMaterialPreviewScene(
        kairo::renderer::MeshHandle mesh,
        const kairo::editor::MaterialPreviewRequest& request,
        const kairo::renderer::MaterialTextureResolver& resolveTexture = {})
    {
        request.Validate();
        if (mesh == kairo::renderer::InvalidMeshHandle)
            throw std::invalid_argument("Material preview requires a valid renderer mesh handle.");

        MaterialPreviewScene result;
        kairo::renderer::MeshDraw draw;
        draw.Mesh = mesh;
        draw.Material = kairo::renderer::MakePBRMaterial(request.Material, resolveTexture);
        draw.CastShadows = true;
        draw.ReceiveShadows = true;
        result.Scene.Add(draw);

        kairo::renderer::RenderEnvironment environment;
        environment.BackgroundColor = { 0.025f, 0.03f, 0.04f };
        environment.AmbientColor = { 1.0f, 1.0f, 1.0f };
        environment.AmbientIntensity = request.EnvironmentIntensity;
        result.Scene.SetEnvironment(environment);

        kairo::renderer::RenderLight key;
        key.Type = kairo::renderer::RenderLightType::Point;
        key.Position = { 2.5f, 3.0f, 3.0f };
        key.Direction = { -0.5f, -0.6f, -0.6f };
        key.Color = { 1.0f, 0.93f, 0.84f };
        key.Intensity = request.KeyLightIntensity;
        key.Range = 20.0f;
        result.Scene.AddLight(key);

        kairo::renderer::RenderLight fill;
        fill.Type = kairo::renderer::RenderLightType::Point;
        fill.Position = { -3.0f, 1.5f, 1.5f };
        fill.Direction = { 0.8f, -0.2f, -0.3f };
        fill.Color = { 0.65f, 0.78f, 1.0f };
        fill.Intensity = request.FillLightIntensity;
        fill.Range = 20.0f;
        result.Scene.AddLight(fill);

        result.Camera.Position = request.Shape == kairo::editor::MaterialPreviewShape::Plane
            ? kairo::foundation::math::Vec3f{ 0.0f, 2.4f, 2.8f }
            : kairo::foundation::math::Vec3f{ 0.0f, 0.0f, 3.4f };
        result.Camera.Target = { 0.0f, 0.0f, 0.0f };
        result.Camera.Up = { 0.0f, 1.0f, 0.0f };
        return result;
    }

    /// Owning convenience wrapper for a live RendererRuntime. Texture handles
    /// remain owned by the caller's asset/GPU cache; only preview geometry is
    /// allocated and released here.
    class LiveMaterialPreview final
    {
    public:
        LiveMaterialPreview(kairo::renderer::RendererRuntime& renderer,
            const kairo::editor::MaterialPreviewRequest& request,
            kairo::renderer::MaterialTextureResolver resolveTexture = {})
            : m_Renderer(renderer),
              m_Mesh(renderer.CreateMesh(MakeMaterialPreviewMesh(request.Shape)))
        {
            m_Preview = BuildMaterialPreviewScene(m_Mesh, request, resolveTexture);
            m_Renderer.SubmitRenderScene(m_Preview.Scene);
            m_Renderer.SetCameraPose(m_Preview.Camera);
        }

        LiveMaterialPreview(const LiveMaterialPreview&) = delete;
        LiveMaterialPreview& operator=(const LiveMaterialPreview&) = delete;
        ~LiveMaterialPreview()
        {
            try
            {
                m_Renderer.SubmitRenderScene({});
                m_Renderer.DestroyMesh(m_Mesh);
            }
            catch (...) {}
        }

        [[nodiscard]] const MaterialPreviewScene& Preview() const noexcept { return m_Preview; }

    private:
        kairo::renderer::RendererRuntime& m_Renderer;
        kairo::renderer::MeshHandle m_Mesh = kairo::renderer::InvalidMeshHandle;
        MaterialPreviewScene m_Preview;
    };
}
