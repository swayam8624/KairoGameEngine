#include <filesystem>

#include <catch2/catch_test_macros.hpp>

import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.RayTracer;
import Kairo.Runtime.RenderBridge.SceneSnapshot;
import Kairo.Runtime.RenderBridge.OfflineSession;

namespace
{
    kairo::engine::Scene MakeRenderableScene()
    {
        using namespace kairo::engine;
        Scene scene;
        const Entity cameraEntity = scene.CreateEntity("Offline Camera");
        CameraComponent camera;
        camera.Primary = true;
        camera.VerticalFovRadians = 0.9f;
        scene.SetCamera(cameraEntity, camera);
        scene.Transform(cameraEntity).Local.Translation = { 0.0f, 1.0f, 5.0f };

        const Entity lightEntity = scene.CreateEntity("Point Light");
        LightComponent light;
        light.Type = LightType::Point;
        light.Unit = PhotometricUnit::Candela;
        light.Intensity = 12.0f;
        light.Color = { 1.0f, 0.5f, 0.25f };
        scene.SetLight(lightEntity, light);
        scene.Transform(lightEntity).Local.Translation = { 2.0f, 3.0f, 1.0f };
        return scene;
    }
}

TEST_CASE("EngineCore camera and point light convert without a second scene file")
{
    using namespace kairo::runtime::renderbridge;
    const auto scene = MakeRenderableScene();
    OfflineSceneSettings settings;
    settings.Width = 8u;
    settings.Height = 4u;
    const SceneConversionResult converted = ConvertSceneSnapshot(scene, {}, settings);
    REQUIRE(converted.Supported());
    CHECK(converted.Snapshot.Settings.Width == 8u);
    CHECK(converted.Snapshot.Settings.Height == 4u);
    CHECK(converted.Snapshot.MainCamera.Position.x == 0.0f);
    CHECK(converted.Snapshot.MainCamera.Position.y == 1.0f);
    CHECK(converted.Snapshot.MainCamera.Position.z == 5.0f);
    REQUIRE(converted.Snapshot.Lights.size() == 1u);
    CHECK(converted.Snapshot.Lights.front().Intensity == 12.0f);
    CHECK(converted.Snapshot.Lights.front().Color.r == 1.0f);
    CHECK(converted.Snapshot.Lights.front().Color.g == 0.5f);
}

TEST_CASE("directional spot and fog semantics convert into the shared offline scene")
{
    using namespace kairo::engine;
    using namespace kairo::runtime::renderbridge;
    Scene scene = MakeRenderableScene();

    const Entity sun = scene.CreateEntity("Sun");
    LightComponent directional;
    directional.Type = LightType::Directional;
    directional.Unit = PhotometricUnit::Lux;
    directional.Intensity = 3.0f;
    scene.SetLight(sun, directional);

    const Entity spotEntity = scene.CreateEntity("Spot");
    LightComponent spot;
    spot.Type = LightType::Spot;
    spot.Unit = PhotometricUnit::Candela;
    spot.Intensity = 8.0f;
    spot.Range = 25.0f;
    spot.InnerConeRadians = 0.25f;
    spot.OuterConeRadians = 0.5f;
    scene.SetLight(spotEntity, spot);
    scene.Transform(spotEntity).Local.Translation = { 0.0f, 2.0f, 3.0f };

    const Entity environmentEntity = scene.CreateEntity("Environment");
    EnvironmentComponent environment;
    environment.Active = true;
    environment.Fog = FogMode::Exponential;
    environment.FogColor = { 0.1f, 0.2f, 0.3f };
    environment.FogDensity = 0.04f;
    scene.SetEnvironment(environmentEntity, environment);

    const auto converted = ConvertSceneSnapshot(scene, {});
    REQUIRE(converted.Supported());
    REQUIRE(converted.Snapshot.DirectionalLights.size() == 1u);
    REQUIRE(converted.Snapshot.SpotLights.size() == 1u);
    CHECK(converted.Snapshot.DirectionalLights.front().Illuminance == 3.0f);
    CHECK(converted.Snapshot.SpotLights.front().Intensity == 8.0f);
    CHECK(converted.Snapshot.SpotLights.front().Range == 25.0f);
    CHECK(converted.Snapshot.Settings.Fog == kairo::foundation::raytracer::FogMode::Exponential);
    CHECK(converted.Snapshot.Settings.FogDensity == 0.04f);
}

TEST_CASE("Offline render session writes project-owned result metadata")
{
    using namespace kairo::runtime::renderbridge;
    const auto scene = MakeRenderableScene();
    OfflineSceneSettings settings;
    settings.Width = 4u;
    settings.Height = 4u;
    settings.Mode = kairo::foundation::raytracer::RenderMode::PBR;
    OfflineRenderSession session(scene, {}, settings);
    REQUIRE(session.Supported());
    session.Start(1u);
    session.Wait();
    CHECK(session.Progress().State == kairo::foundation::raytracer::OfflineRenderJobState::Completed);
    REQUIRE(session.Snapshot().has_value());

    const auto root = std::filesystem::temp_directory_path() / "kairo-render-bridge-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "Renders");
    const auto output = session.SaveResult(root, "Renders/offline.ppm");
    CHECK(std::filesystem::exists(output.ImagePath));
    CHECK(std::filesystem::exists(output.MetadataPath));
    CHECK(output.Width == 4u);
    CHECK(output.Height == 4u);
    std::filesystem::remove_all(root);
}
