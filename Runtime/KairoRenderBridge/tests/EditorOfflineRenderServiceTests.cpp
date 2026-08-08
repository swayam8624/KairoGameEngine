#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <thread>

#include <catch2/catch_test_macros.hpp>

import Kairo.Editor.OfflineRenderAuthoring;
import Kairo.EngineCore;
import Kairo.Runtime.RenderBridge.EditorOfflineService;

TEST_CASE("editor offline service renders the current EngineCore scene and publishes project output")
{
    using namespace kairo::engine;
    using namespace kairo::editor;
    using namespace kairo::runtime::renderbridge;

    Scene scene;
    const Entity cameraEntity = scene.CreateEntity("Camera");
    CameraComponent camera;
    camera.Primary = true;
    scene.SetCamera(cameraEntity, camera);
    scene.Transform(cameraEntity).Local.Translation = { 0.0f, 0.0f, 4.0f };
    const Entity lightEntity = scene.CreateEntity("Light");
    LightComponent light;
    light.Type = LightType::Point;
    light.Intensity = 4.0f;
    scene.SetLight(lightEntity, light);

    const auto root = std::filesystem::temp_directory_path() / "kairo-editor-offline-render-service-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "Renders");

    EditorOfflineRenderService service([&]() -> const Scene& { return scene; }, {});
    OfflineRenderRequest request;
    request.JobID = 101u;
    request.Width = 4u;
    request.Height = 4u;
    request.Passes = 1u;
    request.ProjectRoot = root;
    request.RelativeOutput = "Renders/editor.ppm";
    service.Submit(request);

    OfflineRenderServiceProgress progress;
    for (unsigned attempt = 0u; attempt < 200u; ++attempt)
    {
        progress = service.Poll(request.JobID);
        if (progress.Status == OfflineRenderWorkspaceStatus::Completed ||
            progress.Status == OfflineRenderWorkspaceStatus::Failed) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(progress.Status == OfflineRenderWorkspaceStatus::Completed);
    REQUIRE(progress.Output.has_value());
    CHECK(std::filesystem::is_regular_file(*progress.Output));
    CHECK(std::filesystem::is_regular_file(progress.Output->string() + ".kairo-render"));
    service.Forget(request.JobID);
    std::filesystem::remove_all(root);
}

TEST_CASE("editor offline service reports unsupported scene semantics before starting worker")
{
    using namespace kairo::engine;
    using namespace kairo::editor;
    using namespace kairo::runtime::renderbridge;

    Scene scene;
    const Entity cameraEntity = scene.CreateEntity("Camera");
    CameraComponent camera;
    camera.Primary = true;
    scene.SetCamera(cameraEntity, camera);
    const Entity sun = scene.CreateEntity("Sun");
    LightComponent light;
    light.Type = LightType::Directional;
    light.Unit = PhotometricUnit::Lux;
    scene.SetLight(sun, light);

    EditorOfflineRenderService service([&]() -> const Scene& { return scene; }, {});
    OfflineRenderRequest request;
    request.JobID = 102u;
    request.Width = 4u;
    request.Height = 4u;
    request.Passes = 1u;
    request.ProjectRoot = std::filesystem::temp_directory_path();
    service.Submit(request);
    const auto progress = service.Poll(request.JobID);
    CHECK(progress.Status == OfflineRenderWorkspaceStatus::Failed);
    REQUIRE_FALSE(progress.Diagnostics.empty());
}