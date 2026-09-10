#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimeSaveGameBridge;

namespace assets = kairo::assets;
namespace engine = kairo::engine;
namespace math = kairo::foundation::math;
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
        if (!output) throw std::runtime_error("Save-game fixture write failed.");
    }

    struct Fixture final
    {
        std::filesystem::path Root;
        engine::Entity Floor;
        engine::Entity Ball;
        engine::Entity Marker;
    };

    [[nodiscard]] Fixture MakeProject(const std::filesystem::path& root)
    {
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "Scenes");
        std::filesystem::create_directories(root / "Config");

        Write(root / "RuntimeSave.kproject",
            "kairo-project 2\n"
            "name \"Runtime Save Test\"\n"
            "engine-version \"0.1.0\"\n"
            "assets \"Assets.kassets\"\n"
            "startup-scene \"Scenes/Main.kscene\"\n"
            "input-map \"Config/Input.kinput\"\n"
            "rendering-profile \"desktop\"\n"
            "build-profile \"Development\" development \"Build/Development\"\n");
        Write(root / "Assets.kassets", "kairo-assets 1\n");
        Write(root / "Config/Input.kinput",
            "kairo-input 1\n"
            "action \"Jump\" button\n"
            "bind \"Jump\" key Space 1 0 0\n");

        assets::AssetRegistry registry;
        engine::Scene scene;

        const auto floor = scene.CreateEntity("Floor");
        scene.Transform(floor).Local.Translation = { 0.0f, -0.5f, 0.0f };
        engine::ColliderComponent floorCollider;
        floorCollider.Shape = engine::ColliderShape::Box;
        floorCollider.HalfExtents = { 5.0f, 0.5f, 5.0f };
        scene.SetCollider(floor, floorCollider);

        const auto ball = scene.CreateEntity("Ball");
        scene.Transform(ball).Local.Translation = { 0.0f, 3.0f, 0.0f };
        scene.SetRigidBody(ball, {});
        engine::ColliderComponent ballCollider;
        ballCollider.Shape = engine::ColliderShape::Sphere;
        ballCollider.Radius = 0.5f;
        scene.SetCollider(ball, ballCollider);

        const auto marker = scene.CreateEntity("Marker");
        scene.Transform(marker).Local.Translation = { 2.0f, 1.0f, -3.0f };
        scene.AddTag(marker, "checkpoint-marker");

        engine::SaveScene(root / "Scenes/Main.kscene", scene, registry);
        return { root, floor, ball, marker };
    }

    void AddPrimaryListener(engine::Scene& scene, engine::Entity entity)
    {
        engine::AudioListenerComponent listener;
        listener.Enabled = true;
        listener.Primary = true;
        scene.SetAudioListener(entity, listener);
    }

    void TestRoundTrip(const std::filesystem::path& testRoot)
    {
        const auto fixture = MakeProject(testRoot / "roundtrip");
        player::RuntimeProject project(fixture.Root / "RuntimeSave.kproject");
        AddPrimaryListener(project.Scene(), fixture.Marker);
        player::RuntimePhysicsBridge runtimePhysics(project.Scene());
        player::RuntimeSaveGameBridge saveGames(project, runtimePhysics);

        for (unsigned frame = 0u; frame < 45u; ++frame)
            (void)runtimePhysics.Advance(1.0f / 60.0f);
        runtimePhysics.ApplyEntityImpulse(fixture.Ball, { 1.5, 2.0, -0.5 });
        (void)runtimePhysics.Advance(1.0f / 60.0f);

        const auto expectedPhysicsHash = physics::PhysicsStateHash(runtimePhysics.World());
        const auto expectedMarker = project.Scene().Transform(fixture.Marker).Local;
        const auto expectedBallBody = runtimePhysics.BodyFor(fixture.Ball).value();
        const auto expectedBallState = runtimePhysics.World().Bodies().at(expectedBallBody).State;

        const auto archive = saveGames.Capture("checkpoint-one", 17u);
        Require(archive.ProjectName == "Runtime Save Test",
            "Captured save-game lost project identity.");
        Require(archive.EngineVersion == "0.1.0" && archive.Sequence == 17u &&
            archive.Label == "checkpoint-one",
            "Captured save-game metadata is incorrect.");
        Require(archive.ContainsChunk(engine::SceneSaveChunkName) &&
            archive.ContainsChunk(engine::AudioSceneSaveChunkName) &&
            archive.ContainsChunk(player::RuntimePhysicsSaveChunkName) &&
            archive.ContainsChunk(player::RuntimePhysicsBindingsSaveChunkName),
            "Captured save-game is missing a required runtime subsystem chunk.");

        const auto savePath = fixture.Root / "Saves/checkpoint.ksave";
        saveGames.Save(savePath, "checkpoint-one", 17u);
        Require(std::filesystem::is_regular_file(savePath),
            "Runtime save-game file was not written.");

        project.Scene().Transform(fixture.Marker).Local.Translation = { 99.0f, 88.0f, 77.0f };
        (void)project.Scene().RemoveAudioListener(fixture.Marker);
        runtimePhysics.SetEntityPosition(fixture.Ball, { 4.0, 6.0, 2.0 });
        runtimePhysics.ApplyEntityImpulse(fixture.Ball, { -8.0, 4.0, 3.0 });
        (void)runtimePhysics.Advance(1.0f / 30.0f);
        Require(physics::PhysicsStateHash(runtimePhysics.World()) != expectedPhysicsHash,
            "Fixture failed to move physics away from the saved state.");
        Require(!project.Scene().HasAudioListener(fixture.Marker),
            "Fixture failed to move authored audio away from the saved state.");

        const auto loaded = saveGames.Load(savePath);
        Require(loaded.Sequence == 17u && loaded.Label == "checkpoint-one",
            "Loaded archive metadata does not match the written checkpoint.");
        Require(physics::PhysicsStateHash(runtimePhysics.World()) == expectedPhysicsHash,
            "PhysicsWorld did not restore bit-stable deterministic state.");
        Require(project.Scene().Transform(fixture.Marker).Local.Translation ==
            expectedMarker.Translation,
            "Non-physics scene state did not restore from the scene chunk.");
        Require(project.Scene().HasAudioListener(fixture.Marker) &&
            project.Scene().AudioListenerComponentFor(fixture.Marker).Enabled &&
            project.Scene().AudioListenerComponentFor(fixture.Marker).Primary,
            "Authored primary audio listener did not restore from its save chunk.");

        const auto restoredBall = runtimePhysics.World().Bodies().at(expectedBallBody).State;
        Require(math::NearlyEqual(restoredBall.Position, expectedBallState.Position, 1.0e-6f) &&
            math::NearlyEqual(restoredBall.LinearVelocity,
                expectedBallState.LinearVelocity, 1.0e-6f),
            "Restored dynamic body pose/velocity does not match the saved snapshot.");
        const auto sceneBall = project.Scene().WorldTransform(fixture.Ball);
        Require(math::NearlyEqual(sceneBall.Translation, restoredBall.Position, 1.0e-6f),
            "Restored physics pose was not republished into the runtime scene.");

        (void)runtimePhysics.Advance(0.0f);
        Require(math::NearlyEqual(project.Scene().WorldTransform(fixture.Ball).Translation,
            restoredBall.Position, 1.0e-6f),
            "First frame after load interpolated from stale pre-load physics history.");
    }

    void TestValidationIsNonDestructive(const std::filesystem::path& testRoot)
    {
        const auto fixture = MakeProject(testRoot / "validation");
        player::RuntimeProject project(fixture.Root / "RuntimeSave.kproject");
        AddPrimaryListener(project.Scene(), fixture.Marker);
        player::RuntimePhysicsBridge runtimePhysics(project.Scene());
        player::RuntimeSaveGameBridge saveGames(project, runtimePhysics);
        (void)runtimePhysics.Advance(1.0f / 60.0f);

        const auto originalHash = physics::PhysicsStateHash(runtimePhysics.World());
        const auto originalMarker = project.Scene().Transform(fixture.Marker).Local.Translation;

        auto wrongProject = saveGames.Capture();
        wrongProject.ProjectName = "Another Game";
        bool wrongProjectRejected = false;
        try { saveGames.Restore(wrongProject); }
        catch (const std::invalid_argument&) { wrongProjectRejected = true; }
        Require(wrongProjectRejected,
            "Save-game from another project was accepted.");
        Require(physics::PhysicsStateHash(runtimePhysics.World()) == originalHash &&
            project.Scene().Transform(fixture.Marker).Local.Translation == originalMarker &&
            project.Scene().HasAudioListener(fixture.Marker),
            "Rejected project mismatch mutated runtime state.");

        auto missingAudio = saveGames.Capture();
        missingAudio.RemoveChunk(engine::AudioSceneSaveChunkName);
        bool missingAudioRejected = false;
        try { saveGames.Restore(missingAudio); }
        catch (const std::invalid_argument&) { missingAudioRejected = true; }
        Require(missingAudioRejected,
            "Save-game missing authored audio state was accepted.");
        Require(physics::PhysicsStateHash(runtimePhysics.World()) == originalHash &&
            project.Scene().HasAudioListener(fixture.Marker),
            "Rejected missing-audio save mutated runtime state.");

        auto missingPhysics = saveGames.Capture();
        missingPhysics.RemoveChunk(player::RuntimePhysicsSaveChunkName);
        bool missingRejected = false;
        try { saveGames.Restore(missingPhysics); }
        catch (const std::invalid_argument&) { missingRejected = true; }
        Require(missingRejected,
            "Save-game missing physics state was accepted.");
        Require(physics::PhysicsStateHash(runtimePhysics.World()) == originalHash,
            "Rejected missing-physics save mutated PhysicsWorld.");

        auto wrongSchema = saveGames.Capture();
        auto physicsChunk = wrongSchema.Chunk(player::RuntimePhysicsSaveChunkName);
        ++physicsChunk.SchemaVersion;
        wrongSchema.SetChunk(std::move(physicsChunk));
        bool schemaRejected = false;
        try { saveGames.Restore(wrongSchema); }
        catch (const std::invalid_argument&) { schemaRejected = true; }
        Require(schemaRejected,
            "Unsupported physics save chunk schema was accepted.");
        Require(physics::PhysicsStateHash(runtimePhysics.World()) == originalHash,
            "Rejected physics schema mutated PhysicsWorld.");

        // Explicitly remove the portable binding table to exercise the legacy
        // same-topology restore contract. New-format archives are allowed to carry
        // runtime-spawned Scene topology as long as their persisted body mapping is valid.
        auto changedTopology = saveGames.Capture();
        engine::Scene savedScene = engine::ParseSceneSaveChunk(
            changedTopology.Chunk(engine::SceneSaveChunkName), project.Assets());
        const auto extra = savedScene.CreateEntity("Runtime-only extra entity");
        savedScene.Transform(extra).Local.Translation = { 1.0f, 2.0f, 3.0f };
        changedTopology.SetChunk(engine::MakeSceneSaveChunk(savedScene, project.Assets()));
        changedTopology.RemoveChunk(player::RuntimePhysicsBindingsSaveChunkName);
        bool topologyRejected = false;
        try { saveGames.Restore(changedTopology); }
        catch (const std::invalid_argument&) { topologyRejected = true; }
        Require(topologyRejected,
            "Legacy save-game with incompatible scene topology was accepted.");
        Require(physics::PhysicsStateHash(runtimePhysics.World()) == originalHash &&
            project.Scene().Transform(fixture.Marker).Local.Translation == originalMarker &&
            project.Scene().HasAudioListener(fixture.Marker),
            "Rejected scene topology mutated runtime state.");
    }
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() /
        "kairo-player-save-game-tests";
    std::filesystem::remove_all(root);
    try
    {
        TestRoundTrip(root);
        TestValidationIsNonDestructive(root);
        std::filesystem::remove_all(root);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "KairoPlayer save-game test: " << error.what() << '\n';
        std::filesystem::remove_all(root);
        return 1;
    }
}
