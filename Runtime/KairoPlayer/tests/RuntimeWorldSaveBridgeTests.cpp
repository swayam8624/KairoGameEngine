#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

import Kairo.AI.Gameplay;
import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Foundation.PhysicsMath.Types;
import Kairo.Foundation.Spatial.NavMesh;
import Kairo.Player.RuntimeCharacterMotorBridge;
import Kairo.Player.RuntimeCrowdNavigationBridge;
import Kairo.Player.RuntimeEntityLifecycleBridge;
import Kairo.Player.RuntimeNavigationAgentBridge;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimeSaveGameBridge;
import Kairo.Player.RuntimeWorldSaveBridge;

namespace ai = kairo::ai::gameplay;
namespace assets = kairo::assets;
namespace engine = kairo::engine;
namespace math = kairo::foundation::math;
namespace physics = kairo::foundation::physics;
namespace spatial = kairo::foundation::spatial;
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
        if (!output) throw std::runtime_error("World-save fixture write failed.");
    }

    [[nodiscard]] std::filesystem::path MakeProject(
        const std::filesystem::path& root)
    {
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "Scenes");
        std::filesystem::create_directories(root / "Config");
        Write(root / "PlayableWorld.kproject",
            "kairo-project 2\n"
            "name \"Playable World Save Test\"\n"
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
        floorCollider.HalfExtents = { 12.0f, 0.5f, 12.0f };
        floorCollider.BelongsTo = physics::CollisionLayer::StaticWorld;
        floorCollider.CollidesWith = physics::CollisionLayer::All;
        scene.SetCollider(floor, floorCollider);
        engine::SaveScene(root / "Scenes/Main.kscene", scene, registry);
        return root / "PlayableWorld.kproject";
    }

    [[nodiscard]] spatial::NavMesh MakeNavMesh()
    {
        spatial::NavMesh nav;
        nav.AddPolygon(1u, {
            { -10.0f, 0.0f, -10.0f },
            {  10.0f, 0.0f, -10.0f },
            {  10.0f, 0.0f,  10.0f },
            { -10.0f, 0.0f,  10.0f }
        });
        nav.BuildAdjacency();
        return nav;
    }

    [[nodiscard]] engine::Entity AddPhysicsGapEntity(engine::Scene& scene)
    {
        const auto entity = scene.CreateEntity("TemporaryGapBody");
        scene.Transform(entity).Local.Translation = { 0.0f, 4.0f, 4.0f };
        scene.SetRigidBody(entity, {});
        engine::ColliderComponent collider;
        collider.Shape = engine::ColliderShape::Sphere;
        collider.Radius = 0.25f;
        scene.SetCollider(entity, collider);
        return entity;
    }

    [[nodiscard]] engine::Entity AddNPC(engine::Scene& scene)
    {
        const auto entity = scene.CreateEntity("SavedPatrolNPC");
        scene.Transform(entity).Local.Translation = { -5.0f, 0.92f, 0.0f };
        engine::RigidBodyComponent body;
        body.Motion = engine::RigidBodyMotion::Kinematic;
        body.GravityScale = 0.0f;
        scene.SetRigidBody(entity, body);
        engine::ColliderComponent collider;
        collider.Shape = engine::ColliderShape::Capsule;
        collider.Radius = 0.30f;
        collider.HalfHeight = 0.60f;
        collider.BelongsTo = physics::CollisionLayer::Player;
        collider.CollidesWith = physics::CollisionLayer::StaticWorld;
        scene.SetCollider(entity, collider);
        return entity;
    }

    [[nodiscard]] player::RuntimeEntityServiceProfile AgentProfile()
    {
        player::RuntimeEntityServiceProfile profile;
        profile.CharacterMotor = true;
        profile.Navigation = true;
        profile.Crowd = true;
        profile.NavigationSettings.MaximumSpeed = 2.5f;
        profile.NavigationSettings.WaypointRadius = 0.15f;
        profile.NavigationSettings.PathSnapDistance = 1.0f;
        profile.CrowdSettings.Radius = 0.35f;
        profile.CrowdSettings.MaximumAcceleration = 8.0f;
        return profile;
    }
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() /
        "kairo-player-world-save-tests";
    std::filesystem::remove_all(root);
    try
    {
        const auto projectPath = MakeProject(root);
        const auto savePath = root / "Saves/runtime-world.ksave";
        engine::Entity savedNPC{};
        physics::BodyID savedNPCBody = physics::InvalidBodyID;
        std::uint64_t savedPhysicsHash = 0u;
        math::Vec3f savedPosition{};
        ai::NavigationIntent savedIntent;
        savedIntent.Destination = { 5.0, 0.92, 0.0 };
        savedIntent.AcceptanceRadius = 0.20;

        {
            player::RuntimeProject project(projectPath);
            player::RuntimePhysicsBridge runtimePhysics(project.Scene());
            player::RuntimeCharacterMotorBridge motor(project.Scene(), runtimePhysics);
            const auto navMesh = MakeNavMesh();
            player::RuntimeNavigationAgentBridge navigation(
                project.Scene(), motor, navMesh);
            player::RuntimeCrowdNavigationBridge crowd(project.Scene(), navigation);
            player::RuntimeEntityLifecycleBridge lifecycle(
                project.Scene(), runtimePhysics, motor, navigation, crowd);
            player::RuntimeWorldSaveBridge saves(project, runtimePhysics, lifecycle);

            // Manufacture a real inactive BodyID hole. Cold restore must reproduce
            // this identity layout rather than compacting bodies by Scene order.
            const auto gap = AddPhysicsGapEntity(project.Scene());
            const std::span<const engine::Entity> gapSpan(&gap, 1u);
            Require(runtimePhysics.ActivateEntities(gapSpan).ActivatedBodies == 1u,
                "Fixture failed to create temporary runtime physics body.");
            const auto gapBody = runtimePhysics.BodyFor(gap).value();
            Require(runtimePhysics.DeactivateEntities(gapSpan).DeactivatedBodies == 1u,
                "Fixture failed to create an inactive BodyID gap.");
            project.Scene().DestroyEntity(gap);

            engine::Scene prefab;
            const auto sourceNPC = AddNPC(prefab);
            const auto spawned = lifecycle.SpawnFragment(
                prefab, { { sourceNPC, AgentProfile() } });
            savedNPC = spawned.Resolve(sourceNPC).value();
            savedNPCBody = runtimePhysics.BodyFor(savedNPC).value();
            Require(savedNPCBody > gapBody,
                "Fixture did not place the live NPC beyond an inactive BodyID gap.");
            Require(lifecycle.SetNavigationIntent(savedNPC, savedIntent),
                "Fixture NPC failed to receive its navigation intent.");

            for (unsigned frame = 0u; frame < 35u; ++frame)
                (void)lifecycle.StepAgents(1.0f / 60.0f);
            savedPosition = project.Scene().WorldTransform(savedNPC).Translation;
            savedPhysicsHash = physics::PhysicsStateHash(runtimePhysics.World());

            const auto archive = saves.Capture("mid-patrol", 41u);
            Require(archive.ContainsChunk(player::RuntimePhysicsBindingsSaveChunkName),
                "World save omitted portable physics bindings.");
            Require(archive.ContainsChunk(player::RuntimeLifecycleSaveChunkName),
                "World save omitted runtime lifecycle state.");
            saves.Save(savePath, "mid-patrol", 41u);
        }

        // Entire runtime above is gone. A fresh process-equivalent construction
        // starts from the authored startup Scene, which does not contain savedNPC.
        {
            player::RuntimeProject project(projectPath);
            Require(!project.Scene().Contains(savedNPC),
                "Fresh startup Scene unexpectedly contains the runtime-spawned NPC.");
            player::RuntimePhysicsBridge runtimePhysics(project.Scene());
            player::RuntimeCharacterMotorBridge motor(project.Scene(), runtimePhysics);
            const auto navMesh = MakeNavMesh();
            player::RuntimeNavigationAgentBridge navigation(
                project.Scene(), motor, navMesh);
            player::RuntimeCrowdNavigationBridge crowd(project.Scene(), navigation);
            player::RuntimeEntityLifecycleBridge lifecycle(
                project.Scene(), runtimePhysics, motor, navigation, crowd);
            player::RuntimeWorldSaveBridge saves(project, runtimePhysics, lifecycle);

            const auto archive = saves.Load(savePath);
            Require(archive.Sequence == 41u && archive.Label == "mid-patrol",
                "Cold world load lost archive metadata.");
            Require(project.Scene().Contains(savedNPC),
                "Cold world load did not recreate runtime-spawned Scene entity.");
            Require(runtimePhysics.BodyFor(savedNPC).has_value() &&
                runtimePhysics.BodyFor(savedNPC).value() == savedNPCBody,
                "Cold world load did not preserve the saved BodyID across its inactive gap.");
            Require(physics::PhysicsStateHash(runtimePhysics.World()) == savedPhysicsHash,
                "Cold world load did not restore deterministic PhysicsWorld state exactly.");
            Require(math::NearlyEqual(
                    project.Scene().WorldTransform(savedNPC).Translation,
                    savedPosition, 1.0e-5f),
                "Cold world load did not republish the saved NPC physical pose.");
            Require(lifecycle.IsActive(savedNPC) && motor.IsRegistered(savedNPC) &&
                    navigation.IsRegistered(savedNPC) && crowd.IsRegistered(savedNPC),
                "Cold world load did not reconstruct the NPC runtime service chain.");
            const auto restoredIntent = navigation.Intent(savedNPC);
            Require(restoredIntent.has_value() &&
                    restoredIntent->Destination.X == savedIntent.Destination.X &&
                    restoredIntent->Destination.Y == savedIntent.Destination.Y &&
                    restoredIntent->Destination.Z == savedIntent.Destination.Z &&
                    restoredIntent->AcceptanceRadius == savedIntent.AcceptanceRadius,
                "Cold world load did not restore the NPC current navigation intent.");

            const float before = project.Scene().WorldTransform(savedNPC).Translation.x;
            for (unsigned frame = 0u; frame < 30u; ++frame)
                (void)lifecycle.StepAgents(1.0f / 60.0f);
            Require(project.Scene().WorldTransform(savedNPC).Translation.x > before,
                "Restored NPC could not resume simulation after cold load.");
        }

        std::filesystem::remove_all(root);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "KairoPlayer world-save test: " << error.what() << '\n';
        std::filesystem::remove_all(root);
        return 1;
    }
}
