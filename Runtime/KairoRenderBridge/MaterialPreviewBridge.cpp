module;

#include <stdexcept>
#include <utility>

module Kairo.Runtime.RenderBridge.MaterialPreview;

import Kairo.Editor.MaterialPreviewAuthoring;
import Kairo.Foundation.Math;
import Kairo.Renderer;

namespace kairo::runtime::renderbridge
{
    MaterialPreviewScene::MaterialPreviewScene() = default;
    MaterialPreviewScene::MaterialPreviewScene(const MaterialPreviewScene&) = default;
    MaterialPreviewScene::MaterialPreviewScene(MaterialPreviewScene&&) = default;
    MaterialPreviewScene& MaterialPreviewScene::operator=(const MaterialPreviewScene&) = default;
    MaterialPreviewScene& MaterialPreviewScene::operator=(MaterialPreviewScene&&) = default;
    MaterialPreviewScene::~MaterialPreviewScene() = default;

    kairo::renderer::Mesh MakeMaterialPreviewMesh(
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

    MaterialPreviewScene BuildMaterialPreviewScene(
        kairo::renderer::MeshHandle mesh,
        const kairo::editor::MaterialPreviewRequest& request,
        const kairo::renderer::MaterialTextureResolver& resolveTexture)
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

    LiveMaterialPreview::LiveMaterialPreview(
        kairo::renderer::RendererRuntime& renderer,
        const kairo::editor::MaterialPreviewRequest& request,
        kairo::renderer::MaterialTextureResolver resolveTexture)
        : m_Renderer(renderer),
          m_Mesh(renderer.CreateMesh(MakeMaterialPreviewMesh(request.Shape)))
    {
        try
        {
            m_Preview = BuildMaterialPreviewScene(m_Mesh, request, resolveTexture);
            m_Renderer.SubmitRenderScene(m_Preview.Scene);
            m_Renderer.SetCameraPose(m_Preview.Camera);
        }
        catch (...)
        {
            m_Renderer.DestroyMesh(m_Mesh);
            m_Mesh = kairo::renderer::InvalidMeshHandle;
            throw;
        }
    }

    LiveMaterialPreview::~LiveMaterialPreview()
    {
        try
        {
            m_Renderer.SubmitRenderScene({});
            if (m_Mesh != kairo::renderer::InvalidMeshHandle)
                m_Renderer.DestroyMesh(m_Mesh);
        }
        catch (...) {}
    }

    const MaterialPreviewScene& LiveMaterialPreview::Preview() const noexcept
    {
        return m_Preview;
    }
}
