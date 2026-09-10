#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>

import Kairo.AI.Gameplay;
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Foundation.Spatial.NavMesh;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeCharacterMotorBridge;
import Kairo.Player.RuntimeNavigationAgentBridge;
import Kairo.Player.RuntimeCrowdNavigationBridge;
import Kairo.Player.RuntimeEntityLifecycleBridge;

namespace ai = kairo::ai::gameplay;
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

    [[nodiscard]] engine::Entity AddFloor(engine::Scene& scene)
    {
        const auto floor = scene.CreateEntity("Floor");
        scene.Transform(floor).Local.Translation = { 0.0f, -0.5f, 0.0f };
        engine::ColliderComponent collider;
        collider.Shape = engine::ColliderShape::Box;
        collider.HalfExtents = { 12.0f, 0.5f, 12.0f };
        collider.BelongsTo = physics::CollisionLayer::StaticWorld;
        collider.CollidesWith = physics::CollisionLayer::All;
        scene.SetCollider(floor, collider);
        return floor;
    }

    [[nodiscard]] engine::Entity AddCharacter(engine::Scene& scene,
        const char* name, math::Vec3f position,
        engine::RigidBodyMotion motion = engine::RigidBodyMotion::Kinematic)
    {
        const auto entity = scene.CreateEntity(name);
        scene.Transform(entity).Local.Translation = position;
        engine::RigidBodyComponent body;
        body.Motion = motion;
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

    [[nodiscard]] std::size_t ActiveBodyCount(
        const player::RuntimePhysicsBridge& runtime)
    {
        return static_cast<std::size_t>(std::count_if(
            runtime.World().Bodies().begin(), runtime.World().Bodies().end(),
            [](const auto& body) { return body.Active; }));
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
    try
    {
        engine::Scene scene;
        (void)AddFloor(scene);
        player::RuntimePhysicsBridge runtimePhysics(scene);
        player::RuntimeCharacterMotorBridge motor(scene, runtimePhysics);
        const auto navMesh = MakeNavMesh();
        player::RuntimeNavigationAgentBridge navigation(scene, motor, navMesh);
        player::RuntimeCrowdNavigationBridge crowd(scene, navigation);
        player::RuntimeEntityLifecycleBridge lifecycle(
            scene, runtimePhysics, motor, navigation, crowd);

        // Late-authored entities no longer need a full PhysicsWorld rebuild.
        // Lifecycle activation owns the runtime body it creates and removes only
        // that body again when services are deactivated.
        const auto late = AddCharacter(scene, "LateSpawn", { -2.0f, 0.92f, 2.0f });
        Require(!runtimePhysics.BodyFor(late).has_value(),
            "Late entity unexpectedly existed in startup physics topology.");
        auto motorOnly = player::RuntimeEntityServiceProfile{};
        motorOnly.CharacterMotor = true;
        const auto activation = lifecycle.ActivateEntity(late, motorOnly);
        Require(activation.ActivatedPhysics && motor.IsRegistered(late),
            "Lifecycle did not activate late physics before registering the motor.");
        Require(runtimePhysics.BodyFor(late).has_value(),
            "Lifecycle-created runtime physics mapping is missing.");
        Require(lifecycle.DeactivateEntity(late),
            "Lifecycle failed to deactivate a managed late entity.");
        Require(!runtimePhysics.BodyFor(late).has_value() && !motor.IsRegistered(late),
            "Lifecycle deactivation leaked owned runtime services.");

        // A Scene fragment is the runtime prefab representation. Its authored
        // collider/body data and explicit gameplay service profile are remapped
        // together under one ownership token.
        engine::Scene prefab;
        const auto sourceNPC = AddCharacter(
            prefab, "PatrolNPC", { -4.0f, 0.92f, 0.0f });
        const std::size_t sceneBeforeSpawn = scene.Size();
        const std::size_t bodiesBeforeSpawn = ActiveBodyCount(runtimePhysics);
        const auto spawned = lifecycle.SpawnFragment(
            prefab, { { sourceNPC, AgentProfile() } });
        Require(spawned.IsValid() && lifecycle.SpawnedFragmentCount() == 1u,
            "Runtime prefab spawn did not retain an ownership token.");
        const auto runtimeNPC = spawned.Resolve(sourceNPC);
        Require(runtimeNPC.has_value() && scene.Contains(*runtimeNPC),
            "Runtime prefab source entity was not remapped into the live Scene.");
        Require(scene.Size() == sceneBeforeSpawn + 1u,
            "Runtime prefab spawn produced the wrong Scene entity count.");
        Require(ActiveBodyCount(runtimePhysics) == bodiesBeforeSpawn + 1u,
            "Runtime prefab physics was not activated exactly once.");
        Require(motor.IsRegistered(*runtimeNPC) &&
                navigation.IsRegistered(*runtimeNPC) &&
                crowd.IsRegistered(*runtimeNPC),
            "Runtime prefab did not activate the complete NPC service chain.");

        ai::NavigationIntent intent;
        intent.Destination = { 4.0, 0.92, 0.0 };
        intent.AcceptanceRadius = 0.20;
        Require(lifecycle.SetNavigationIntent(*runtimeNPC, intent),
            "Lifecycle-managed NPC failed to plan a navigation intent.");
        const float startX = scene.WorldTransform(*runtimeNPC).Translation.x;
        bool arrived = false;
        for (unsigned frame = 0u; frame < 420u; ++frame)
        {
            const auto step = lifecycle.StepAgents(1.0f / 60.0f);
            Require(step.CrowdSteps.size() == 1u,
                "Lifecycle NPC step did not return the managed crowd agent.");
            arrived = step.CrowdSteps.front().Navigation.Navigation.Status ==
                player::RuntimeNavigationAgentStatus::Arrived;
            if (arrived) break;
        }
        Require(scene.WorldTransform(*runtimeNPC).Translation.x > startX + 3.0f,
            "Lifecycle-managed NPC did not produce physical locomotion.");
        Require(arrived,
            "Lifecycle-managed NPC did not complete its navigation intent.");

        const auto runtimeNPCValue = *runtimeNPC;
        Require(lifecycle.DestroyFragment(spawned),
            "Runtime prefab ownership token could not be destroyed.");
        Require(!scene.Contains(runtimeNPCValue),
            "Runtime prefab teardown left its Scene entity alive.");
        Require(!runtimePhysics.BodyFor(runtimeNPCValue).has_value() &&
                !motor.IsRegistered(runtimeNPCValue) &&
                !navigation.IsRegistered(runtimeNPCValue) &&
                !crowd.IsRegistered(runtimeNPCValue),
            "Runtime prefab teardown leaked a runtime-only subsystem registration.");
        Require(ActiveBodyCount(runtimePhysics) == bodiesBeforeSpawn,
            "Runtime prefab teardown changed persistent active-body topology.");

        // Failure after Scene append + physics activation must still unwind the
        // externally visible runtime state. A dynamic body is intentionally an
        // invalid character-motor source and therefore fails after topology work.
        engine::Scene badPrefab;
        const auto badSource = AddCharacter(badPrefab, "BadNPC",
            { 0.0f, 0.92f, 3.0f }, engine::RigidBodyMotion::Dynamic);
        auto badProfile = player::RuntimeEntityServiceProfile{};
        badProfile.CharacterMotor = true;
        const std::size_t sceneBeforeFailure = scene.Size();
        const std::size_t activeBodiesBeforeFailure = ActiveBodyCount(runtimePhysics);
        bool rejected = false;
        try
        {
            (void)lifecycle.SpawnFragment(
                badPrefab, { { badSource, badProfile } });
        }
        catch (const std::invalid_argument&) { rejected = true; }
        Require(rejected,
            "Runtime prefab accepted an invalid dynamic character motor.");
        Require(scene.Size() == sceneBeforeFailure,
            "Failed runtime prefab spawn leaked Scene entities.");
        Require(ActiveBodyCount(runtimePhysics) == activeBodiesBeforeFailure,
            "Failed runtime prefab spawn leaked active PhysicsWorld bodies.");
        Require(lifecycle.SpawnedFragmentCount() == 0u,
            "Failed runtime prefab spawn leaked an ownership token.");

        // Dependency validation happens before any Scene mutation.
        engine::Scene invalidProfilePrefab;
        const auto invalidSource = AddCharacter(
            invalidProfilePrefab, "InvalidProfile", { 0.0f, 0.92f, -3.0f });
        auto invalidProfile = player::RuntimeEntityServiceProfile{};
        invalidProfile.Crowd = true;
        rejected = false;
        try
        {
            (void)lifecycle.SpawnFragment(
                invalidProfilePrefab, { { invalidSource, invalidProfile } });
        }
        catch (const std::invalid_argument&) { rejected = true; }
        Require(rejected && scene.Size() == sceneBeforeFailure,
            "Invalid runtime service dependency mutated the Scene before rejection.");

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "KairoPlayer entity lifecycle test: "
                  << error.what() << '\n';
        return 1;
    }
}
