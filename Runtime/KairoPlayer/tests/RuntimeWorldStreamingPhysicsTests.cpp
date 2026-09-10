#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

import Kairo.EngineCore;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeWorldStreamingBridge;

namespace engine = kairo::engine;
namespace physics = kairo::foundation::physics;
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
        if (!output) throw std::runtime_error("Streaming-physics fixture write failed.");
    }

    void WriteFixture(const std::filesystem::path& root)
    {
        Write(root / "Project.kproject",
            "kairo-project 2\n"
            "name \"Streaming Physics Test\"\n"
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
            "entity 1 \"PersistentDynamic\"\n"
            "enabled true\nlayer 0\n"
            "transform -4 3 0 0 0 0 1 1 1 1\n"
            "rigid-body dynamic 1 1 0 0\n"
            "collider sphere 0.5 0.5 0.1 2 4294967295 false\n"
            "end\n"
            "entity 2 \"PersistentFloor\"\n"
            "enabled true\nlayer 0\n"
            "transform 0 -0.5 0 0 0 0 1 1 1 1\n"
            "collider box 20 0.5 20 0.5 0.1 1 4294967295 false\n"
            "end\n");
        Write(root / "Scenes/PhysicsCell.kscene",
            "kairo-scene 4\n"
            "entity 100 \"StreamedBlock\"\n"
            "enabled true\nlayer 0\ntag \"streamed-physics\"\n"
            "transform 10 1 0 0 0 0 1 1 1 1\n"
            "collider box 0.75 0.75 0.75 0.5 0.1 1 4294967295 false\n"
            "end\n");
        Write(root / "Scenes/MalformedPhysicsCell.kscene",
            "kairo-scene 4\n"
            "entity 200 \"MissingCollider\"\n"
            "enabled true\nlayer 0\n"
            "transform 0 2 0 0 0 0 1 1 1 1\n"
            "rigid-body dynamic 1 1 0 0\n"
            "end\n");
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
        config.MaximumCommittedBytes = 16'000u;
        return config;
    }

    engine::WorldStreamingObserver Observer(double x, double z)
    {
        return { { x, 0.0, z }, 1.0 };
    }

    void RequireSameBodyState(const physics::RigidBody& expected,
        const physics::RigidBody& actual)
    {
        Require(expected.Active == actual.Active,
            "Streaming changed persistent active state.");
        Require(expected.Sleeping == actual.Sleeping,
            "Streaming changed persistent sleep state.");
        Require(expected.State.Position == actual.State.Position,
            "Streaming changed persistent position.");
        Require(expected.State.Rotation == actual.State.Rotation,
            "Streaming changed persistent rotation.");
        Require(expected.State.LinearVelocity == actual.State.LinearVelocity,
            "Streaming changed persistent linear velocity.");
        Require(expected.State.AngularVelocity == actual.State.AngularVelocity,
            "Streaming changed persistent angular velocity.");
    }
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() /
        "kairo-player-world-streaming-physics-tests";
    std::filesystem::remove_all(root);
    try
    {
        WriteFixture(root);
        player::RuntimeProject project(root / "Project.kproject");
        player::RuntimePhysicsBridge runtimePhysics(project.Scene());
        const engine::Entity persistent{ 1u };
        const auto persistentBody = runtimePhysics.BodyFor(persistent);
        Require(persistentBody.has_value(),
            "Persistent startup body was not created before streaming.");
        runtimePhysics.ApplyEntityImpulse(persistent, { 3.0, 0.0, 1.0 });
        (void)runtimePhysics.Advance(1.0f / 60.0f);

        player::RuntimeWorldStreamingBridge streaming(
            project, runtimePhysics, Config());
        Require(streaming.SynchronizesPhysics(),
            "World streaming did not retain its physics lifecycle bridge.");
        streaming.RegisterCell({ { 0, 0 }, "Scenes/PhysicsCell.kscene",
            300u, 10, false });

        const physics::RigidBody survivorBeforeLoad =
            runtimePhysics.World().Bodies().at(*persistentBody);
        const auto near = Observer(50.0, 50.0);
        const auto loaded = streaming.Update({ &near, 1u });
        Require(loaded.Succeeded() && loaded.LoadedCells == 1u,
            "Physics-bearing world cell did not load successfully.");
        Require(loaded.ActivatedPhysicsBodies == 1u,
            "World-cell load did not report its activated physics body.");
        RequireSameBodyState(survivorBeforeLoad,
            runtimePhysics.World().Bodies().at(*persistentBody));

        const auto* ownership = streaming.Ownership({ 0, 0 });
        Require(ownership != nullptr,
            "Loaded physics cell lost its scene ownership token.");
        const auto streamed = ownership->Resolve({ 100u });
        Require(streamed.has_value(),
            "Streamed physics entity did not resolve through the remap token.");
        const auto streamedBody = runtimePhysics.BodyFor(*streamed);
        Require(streamedBody.has_value() &&
                runtimePhysics.World().IsValidBody(*streamedBody),
            "Streamed scene entity was not activated in PhysicsWorld.");
        const auto hit = runtimePhysics.Raycast(
            { 10.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 10.0f);
        Require(hit.has_value() && hit->first == *streamed,
            "Runtime raycast cannot see the loaded world-cell collider.");

        const auto far = Observer(500.0, 500.0);
        const physics::RigidBody survivorBeforeUnload =
            runtimePhysics.World().Bodies().at(*persistentBody);
        const auto unloaded = streaming.Update({ &far, 1u });
        Require(unloaded.Succeeded() && unloaded.UnloadedCells == 1u,
            "Physics-bearing world cell did not unload successfully.");
        Require(unloaded.DeactivatedPhysicsBodies == 1u,
            "World-cell unload did not report its deactivated physics body.");
        Require(!runtimePhysics.BodyFor(*streamed).has_value() &&
                !runtimePhysics.World().IsValidBody(*streamedBody),
            "Unloaded world-cell body remained live in PhysicsWorld.");
        RequireSameBodyState(survivorBeforeUnload,
            runtimePhysics.World().Bodies().at(*persistentBody));

        const auto reloaded = streaming.Update({ &near, 1u });
        Require(reloaded.Succeeded(), "Physics cell could not reload.");
        ownership = streaming.Ownership({ 0, 0 });
        Require(ownership != nullptr, "Reloaded physics cell has no ownership token.");
        const auto reloadedEntity = ownership->Resolve({ 100u });
        Require(reloadedEntity.has_value(), "Reloaded physics entity did not remap.");
        const auto reloadedBody = runtimePhysics.BodyFor(*reloadedEntity);
        Require(reloadedBody.has_value() && runtimePhysics.World().IsValidBody(*reloadedBody),
            "Reloaded physics entity has no active body.");

        const auto external = project.Scene().CreateEntity("PersistentGameplayChild");
        project.Scene().SetParent(external, *reloadedEntity);
        const auto blocked = streaming.Update({ &far, 1u });
        Require(!blocked.Succeeded() && blocked.Failures.size() == 1u,
            "External child did not block world-cell unload.");
        Require(blocked.DeactivatedPhysicsBodies == 0u,
            "Blocked scene unload deactivated physics before ownership validation.");
        Require(runtimePhysics.BodyFor(*reloadedEntity) == reloadedBody &&
                runtimePhysics.World().IsValidBody(*reloadedBody),
            "Blocked unload left the streamed scene alive but removed its physics.");

        project.Scene().DestroyEntity(external);
        const auto cleanup = streaming.Update({ &far, 1u });
        Require(cleanup.Succeeded(), "Physics cell cleanup after blocked unload failed.");
        streaming.RegisterCell({ { 2, 0 }, "Scenes/MalformedPhysicsCell.kscene",
            200u, 20, false });
        const auto bodyStorageBeforeFailure = runtimePhysics.CaptureSnapshot().Bodies.size();
        const auto sceneSizeBeforeFailure = project.Scene().Size();
        const physics::RigidBody survivorBeforeFailure =
            runtimePhysics.World().Bodies().at(*persistentBody);
        const auto nearMalformed = Observer(250.0, 50.0);
        const auto malformed = streaming.Update({ &nearMalformed, 1u });
        Require(!malformed.Succeeded() && malformed.Failures.size() == 1u,
            "Malformed streamed rigid body did not fail activation.");
        Require(project.Scene().Size() == sceneSizeBeforeFailure,
            "Failed physics activation leaked appended scene entities.");
        Require(runtimePhysics.CaptureSnapshot().Bodies.size() ==
                bodyStorageBeforeFailure,
            "Failed physics activation leaked body storage despite rollback.");
        RequireSameBodyState(survivorBeforeFailure,
            runtimePhysics.World().Bodies().at(*persistentBody));

        std::filesystem::remove_all(root);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::filesystem::remove_all(root);
        std::cerr << "KairoPlayer world streaming physics test: "
                  << error.what() << '\n';
        return 1;
    }
}
