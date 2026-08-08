#include <catch2/catch_test_macros.hpp>

import Kairo.Editor.MaterialPreviewAuthoring;
import Kairo.Runtime.RenderBridge.MaterialPreview;

TEST_CASE("material preview scene uses saved PBR artifact values on a lit sphere")
{
    using namespace kairo::editor;
    using namespace kairo::runtime::renderbridge;
    MaterialPreviewRequest request;
    request.Shape = MaterialPreviewShape::Sphere;
    request.Material.BaseColorFactor = { 0.2f, 0.4f, 0.8f, 1.0f };
    request.Material.MetallicFactor = 0.75f;
    request.Material.RoughnessFactor = 0.2f;
    request.Material.EmissiveFactor = { 0.1f, 0.0f, 0.0f };

    const auto preview = BuildMaterialPreviewScene(77u, request);
    REQUIRE(preview.Scene.Draws().size() == 1u);
    CHECK(preview.Scene.Draws().front().Mesh == 77u);
    CHECK(preview.Scene.Draws().front().Material.BaseColor.x == 0.2f);
    CHECK(preview.Scene.Draws().front().Material.Metallic == 0.75f);
    CHECK(preview.Scene.Draws().front().Material.Roughness == 0.2f);
    REQUIRE(preview.Scene.Lights().size() == 2u);
    CHECK(preview.Scene.Environment().AmbientIntensity == request.EnvironmentIntensity);
}

TEST_CASE("material preview mesh selection uses renderer production primitives")
{
    using namespace kairo::editor;
    using namespace kairo::runtime::renderbridge;
    const auto sphere = MakeMaterialPreviewMesh(MaterialPreviewShape::Sphere);
    const auto plane = MakeMaterialPreviewMesh(MaterialPreviewShape::Plane);
    CHECK(sphere.Indices().size() > plane.Indices().size());
    CHECK(plane.Indices().size() == 6u);
}
