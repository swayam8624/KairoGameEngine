#include <catch2/catch_test_macros.hpp>

import Kairo.Runtime.RealtimeSceneBridge;
import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Renderer;

using namespace kairo::runtime::renderbridge;

namespace
{
    const auto MeshID = kairo::assets::AssetID::Parse(
        "00000000-0000-4000-8000-000000000501");
    const auto MaterialID = kairo::assets::AssetID::Parse(
        "00000000-0000-4000-8000-000000000502");
    const auto SceneID = kairo::assets::AssetID::Parse(
        "00000000-0000-4000-8000-000000000503");
    const auto TextureID = kairo::assets::AssetID::Parse(
        "00000000-0000-4000-8000-000000000504");

    void RegisterAssets(kairo::assets::AssetRegistry& registry)
    {
        registry.Insert({ MeshID, kairo::assets::AssetType::Mesh,
            kairo::assets::AssetOrigin::Builtin, "builtin/cube",
            "kairo.builtin.cube", 1u, {} });
        registry.Insert({ MaterialID, kairo::assets::AssetType::Material,
            kairo::assets::AssetOrigin::Builtin, "builtin/material",
            "kairo.builtin.material", 1u, {} });
        registry.Insert({ SceneID, kairo::assets::AssetType::Scene,
            kairo::assets::AssetOrigin::SourceFile, "Scenes/model.glb",
            "kairo.gltf.scene", 1u, {} });
        registry.Insert({ TextureID, kairo::assets::AssetType::Texture2D,
            kairo::assets::AssetOrigin::SourceFile, "Textures/environment.hdr",
            "kairo.texture.stb", 1u, {} });
    }
}

TEST_CASE("Shared real-time extraction preserves authored render semantics",
    "[KairoRealtimeRenderBridge][Scene]")
{
    kairo::assets::AssetRegistry registry;
    RegisterAssets(registry);
    RenderAssetBindings assets(registry);
    assets.BindMesh({ MeshID }, 7u);
    assets.BindTexture({ TextureID }, 11u);
    kairo::renderer::PBRMaterial material;
    material.BaseColor = { 0.2f, 0.4f, 0.8f };
    material.Metallic = 0.65f;
    assets.BindMaterial({ MaterialID }, material);

    kairo::engine::Scene scene;
    const auto mesh = scene.CreateEntity("Mesh");
    scene.SetMeshRenderer(mesh, { { MeshID }, { MaterialID }, true });
    scene.MeshRenderer(mesh).RenderLayers = 0x1u;
    scene.MeshRenderer(mesh).CastShadows = false;
    scene.Transform(mesh).Local.Translation = { 2.0f, 0.0f, 0.0f };

    const auto lightEntity = scene.CreateEntity("Light");
    kairo::engine::LightComponent light;
    light.Type = kairo::engine::LightType::Point;
    light.Unit = kairo::engine::PhotometricUnit::Candela;
    light.Intensity = 500.0f;
    light.RenderLayers = 0x1u;
    scene.SetLight(lightEntity, light);

    const auto environmentEntity = scene.CreateEntity("World");
    kairo::engine::EnvironmentComponent environment;
    environment.BackgroundColor = { 0.1f, 0.2f, 0.3f };
    environment.AmbientIntensity = 0.25f;
    environment.EnvironmentIntensity = 2.0f;
    environment.EnvironmentTexture = kairo::assets::TextureAssetHandle{ TextureID };
    scene.SetEnvironment(environmentEntity, environment);

    const auto output = BuildRenderScene(scene, assets, 0x1u);
    REQUIRE(output.Draws().size() == 1u);
    CHECK(output.Draws()[0].ObjectID == mesh.Value);
    CHECK(output.Draws()[0].Material.Metallic == 0.65f);
    CHECK_FALSE(output.Draws()[0].CastShadows);
    REQUIRE(output.Lights().size() == 1u);
    CHECK(output.Lights()[0].Intensity == 5.0f);
    CHECK(output.Environment().AmbientIntensity == 0.5f);
    CHECK(output.Environment().EnvironmentTexture == 11u);
    REQUIRE_THROWS_AS(BuildRenderScene(scene, assets, 0u), std::invalid_argument);
}

TEST_CASE("Shared extraction expands scene instances and honors layer masks",
    "[KairoRealtimeRenderBridge][SceneInstance]")
{
    kairo::assets::AssetRegistry registry;
    RegisterAssets(registry);
    RenderAssetBindings assets(registry);
    auto local = kairo::foundation::math::Mat4f::Identity();
    local(1u, 3u) = 2.0f;
    assets.BindScene({ SceneID }, { { 41u, {}, local },
        { 42u, {}, kairo::foundation::math::Mat4f::Identity() } });

    kairo::engine::Scene scene;
    const auto entity = scene.CreateEntity("Imported");
    scene.SetSceneInstance(entity, { { SceneID }, true, false, true, 0x4u });
    scene.Transform(entity).Local.Translation = { 3.0f, 4.0f, 5.0f };

    const auto output = BuildRenderScene(scene, assets, 0x4u);
    REQUIRE(output.Draws().size() == 2u);
    CHECK(output.Draws()[0].Model(0u, 3u) == 3.0f);
    CHECK(output.Draws()[0].Model(1u, 3u) == 6.0f);
    CHECK(output.Draws()[0].ObjectID == entity.Value);
    CHECK(BuildRenderScene(scene, assets, 0x2u).Draws().empty());
}
