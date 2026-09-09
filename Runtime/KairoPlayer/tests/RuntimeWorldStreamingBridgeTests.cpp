#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

import Kairo.EngineCore;
import Kairo.Player.RuntimeWorldStreamingBridge;

namespace engine = kairo::engine;
namespace player = kairo::player;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    void Write(const std::filesystem::path& path, const std::string& text)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << text;
        if (!output) throw std::runtime_error("World streaming fixture write failed.");
    }

    void WriteProjectFixture(const std::filesystem::path& root)
    {
        Write(root / "Project.kproject",
            "kairo-project 2\n"
            "name \"Streaming Test\"\n"
            "engine-version \"0.1.0\"\n"
            "assets \"Assets.kassets\"\n"
            "startup-scene \"Scenes/Main.kscene\"\n"
            "input-map \"Config/Input.kinput\"\n"
            "rendering-profile \"desktop\"\n"
            "build-profile \"Development\" development \"Build/Development\"\n");
        Write(root / "Assets.kassets", "kairo-assets 1\n");
        Write(root / "Config/Input.kinput", "kairo-input 1\n");
        Write(root / "Scenes/Main.kscene",
            "kairo-scene 4\n"
            "entity 1 \"PersistentRoot\"\n"
            "enabled true\n"
            "layer 0\n"
            "tag \"persistent\"\n"
            "transform 0 0 0 0 0 0 1 1 1 1\n"
            "end\n");
        Write(root / "Scenes/Cell00.kscene",
            "kairo-scene 4\n"
            "entity 100 \"Cell00Root\"\n"
            "enabled true\n"
            "layer 3\n"
            "tag \"streamed\"\n"
            "transform 10 0 20 0 0 0 1 1 1 1\n"
            "end\n"
            "entity 101 \"Cell00Child\"\n"
            "parent 100\n"
            "enabled true\n"
            "layer 3\n"
            "transform 0 2 0 0 0 0 1 1 1 1\n"
            "end\n");
        Write(root / "Scenes/Cell10.kscene",
            "kairo-scene 4\n"
            "entity 200 \"Cell10Root\"\n"
            "enabled true\n"
            "layer 4\n"
            "transform 150 0 0 0 0 0 1 1 1 1\n"
            "end\n");
        Write(root / "Scenes/Broken.kscene",
            "kairo-scene 4\nentity nope \"Broken\"\n");
    }

    engine::WorldStreamingConfig Config()
    {
        engine::WorldStreamingConfig config;
        config.CellSize = 100.0;
        config.LoadRadius = 20.0;
        config.KeepRadius = 40.0;
        config.MaximumLoadsPerUpdate = 4u;
        config.MaximumUnloadsPerUpdate = 4u;
        config.MaximumCommittedCells = 8u;
        config.MaximumCommittedBytes = 8'000u;
        return config;
    }

    engine::WorldStreamingObserver Observer(double x, double z)
    {
        return { { x, 0.0, z }, 1.0 };
    }
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() /
        "kairo-player-world-streaming-tests";
    std::filesystem::remove_all(root);
    try
    {
        WriteProjectFixture(root);
        player::RuntimeProject project(root / "Project.kproject");
        player::RuntimeWorldStreamingBridge streaming(project, Config());
        streaming.RegisterCell({ { 0, 0 }, "Scenes/Cell00.kscene", 200u, 10, false });
        streaming.RegisterCell({ { 1, 0 }, "Scenes/Cell10.kscene", 100u, 0, false });

        Require(project.Scene().Size() == 1u,
            "Streaming fixture startup scene is not isolated from cell content.");
        const engine::WorldStreamingObserver near00 = Observer(50.0, 50.0);
        const auto loaded00 = streaming.Update({ &near00, 1u });
        Require(loaded00.Succeeded() && loaded00.LoadedCells == 1u,
            "Near observer did not load the first world cell.");
        Require(project.Scene().Size() == 3u,
            "Loaded world cell was not composed into the live scene.");
        Require(streaming.Runtime().State({ 0, 0 }) == engine::WorldCellState::Resident,
            "Successful cell load did not become Resident.");
        Require(streaming.LoadedCellCount() == 1u,
            "Player did not retain the scene composition token for the loaded cell.");

        const auto* ownership = streaming.Ownership({ 0, 0 });
        Require(ownership != nullptr && ownership->Size() == 2u,
            "Loaded world cell lost its source-to-runtime ownership map.");
        const auto streamedRoot = ownership->Resolve({ 100u });
        Require(streamedRoot.has_value() && project.Scene().Contains(*streamedRoot),
            "Source cell root did not resolve to a live runtime entity.");
        Require(*streamedRoot != engine::Entity{ 100u },
            "Streamed scene incorrectly preserved source-local entity IDs.");

        const engine::WorldStreamingObserver farAway = Observer(500.0, 500.0);
        const auto unloaded00 = streaming.Update({ &farAway, 1u });
        Require(unloaded00.Succeeded() && unloaded00.UnloadedCells == 1u,
            "Far observer did not unload the resident world cell.");
        Require(project.Scene().Size() == 1u && project.Scene().Contains({ 1u }),
            "World cell unload removed or retained the wrong entities.");
        Require(project.Scene().HasTag({ 1u }, "persistent"),
            "World cell unload damaged the persistent startup scene.");
        Require(streaming.Runtime().State({ 0, 0 }) == engine::WorldCellState::Unloaded,
            "Successful world cell removal did not become Unloaded.");

        streaming.RegisterCell({ { 5, 0 }, "Scenes/Broken.kscene", 100u, 50, false });
        const engine::WorldStreamingObserver nearBroken = Observer(550.0, 50.0);
        const auto broken = streaming.Update({ &nearBroken, 1u });
        Require(!broken.Succeeded() && broken.Failures.size() == 1u,
            "Malformed streamed scene did not report a cell-local load failure.");
        Require(broken.Failures.front().Operation == player::WorldStreamingOperation::Load,
            "Malformed scene failure was attributed to the wrong operation.");
        Require(streaming.Runtime().State({ 5, 0 }) == engine::WorldCellState::Unloaded,
            "Failed cell load remained committed in the streaming policy.");
        Require(project.Scene().Size() == 1u,
            "Failed cell parse partially mutated the live scene.");

        streaming.RegisterCell({ { 6, 0 }, "../outside.kscene", 100u, 100, false });
        const engine::WorldStreamingObserver nearEscape = Observer(650.0, 50.0);
        const auto escaped = streaming.Update({ &nearEscape, 1u });
        Require(!escaped.Succeeded() && escaped.Failures.size() == 1u,
            "Project-root escape did not fail the streaming request.");
        Require(streaming.Runtime().State({ 6, 0 }) == engine::WorldCellState::Unloaded,
            "Rejected project-root escape remained committed.");

        const auto reload = streaming.Update({ &near00, 1u });
        Require(reload.Succeeded() && reload.LoadedCells == 1u,
            "World cell could not reload after a prior successful unload.");
        ownership = streaming.Ownership({ 0, 0 });
        Require(ownership != nullptr, "Reloaded cell has no ownership token.");
        const auto reloadedRoot = ownership->Resolve({ 100u });
        Require(reloadedRoot.has_value(), "Reloaded root could not be resolved.");
        const auto gameplaySpawn = project.Scene().CreateEntity("GameplaySpawn");
        project.Scene().SetParent(gameplaySpawn, *reloadedRoot);

        const auto blockedUnload = streaming.Update({ &farAway, 1u });
        Require(!blockedUnload.Succeeded() && blockedUnload.Failures.size() == 1u,
            "Persistent child dependency did not block streamed ownership unload.");
        Require(blockedUnload.Failures.front().Operation ==
                player::WorldStreamingOperation::Unload,
            "Blocked unload was attributed to the wrong operation.");
        Require(streaming.Runtime().State({ 0, 0 }) == engine::WorldCellState::Resident,
            "Failed unload did not return the cell to Resident state.");
        Require(project.Scene().Contains(gameplaySpawn) &&
                project.Scene().Contains(*reloadedRoot),
            "Failed unload mutated the live scene despite ownership conflict.");

        bool extensionRejected = false;
        try
        {
            streaming.RegisterCell({ { 9, 9 }, "Scenes/not-a-scene.txt", 1u, 0, false });
        }
        catch (const std::invalid_argument&) { extensionRejected = true; }
        Require(extensionRejected,
            "Runtime world streaming accepted a non-.kscene content key.");

        std::filesystem::remove_all(root);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::filesystem::remove_all(root);
        std::cerr << "KairoPlayer world streaming bridge test: " << error.what() << '\n';
        return 1;
    }
}
