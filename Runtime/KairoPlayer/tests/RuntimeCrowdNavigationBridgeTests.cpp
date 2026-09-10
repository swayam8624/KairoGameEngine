#include <cmath>
#include <iostream>
#include <stdexcept>

import Kairo.AI.Gameplay;
import Kairo.EngineCore;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Foundation.Spatial.NavMesh;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeCharacterMotorBridge;
import Kairo.Player.RuntimeNavigationAgentBridge;
import Kairo.Player.RuntimeCrowdNavigationBridge;

namespace ai = kairo::ai::gameplay;
namespace engine = kairo::engine;
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
        collider.HalfExtents = { 8.0f, 0.5f, 5.0f };
        collider.BelongsTo = physics::CollisionLayer::StaticWorld;
        collider.CollidesWith = physics::CollisionLayer::All;
        scene.SetCollider(floor, collider);
        return floor;
    }

    [[nodiscard]] engine::Entity AddCharacter(engine::Scene& scene,
        const char* name, float x)
    {
        const auto character = scene.CreateEntity(name);
        scene.Transform(character).Local.Translation = { x, 0.92f, 0.0f };
        engine::RigidBodyComponent body;
        body.Motion = engine::RigidBodyMotion::Kinematic;
        body.GravityScale = 0.0f;
        scene.SetRigidBody(character, body);
        engine::ColliderComponent collider;
        collider.Shape = engine::ColliderShape::Capsule;
        collider.Radius = 0.30f;
        collider.HalfHeight = 0.60f;
        collider.BelongsTo = physics::CollisionLayer::Player;
        collider.CollidesWith = physics::CollisionLayer::StaticWorld;
        scene.SetCollider(character, collider);
        return character;
    }

    [[nodiscard]] spatial::NavMesh MakeNavMesh()
    {
        spatial::NavMesh nav;
        nav.AddPolygon(1u, {
            { -7.0f, 0.0f, -4.0f },
            {  7.0f, 0.0f, -4.0f },
            {  7.0f, 0.0f,  4.0f },
            { -7.0f, 0.0f,  4.0f }
        });
        nav.BuildAdjacency();
        return nav;
    }

    [[nodiscard]] float PlanarDistance(const engine::Scene& scene,
        engine::Entity a, engine::Entity b)
    {
        const auto pa = scene.WorldTransform(a).Translation;
        const auto pb = scene.WorldTransform(b).Translation;
        const float dx = pa.x - pb.x;
        const float dz = pa.z - pb.z;
        return std::sqrt(dx * dx + dz * dz);
    }
}

int main()
{
    try
    {
        engine::Scene scene;
        (void)AddFloor(scene);
        const auto left = AddCharacter(scene, "LeftNPC", -4.0f);
        const auto right = AddCharacter(scene, "RightNPC", 4.0f);
        // RuntimePhysicsBridge snapshots authored physics topology at bootstrap.
        // Keep the negative-test NPC in that authored topology, but intentionally
        // omit it from navigation registration below.
        const auto third = AddCharacter(scene, "UnregisteredNPC", 0.0f);

        player::RuntimePhysicsBridge runtimePhysics(scene);
        player::RuntimeCharacterMotorBridge motor(scene, runtimePhysics);
        motor.Register(left);
        motor.Register(right);

        const auto navMesh = MakeNavMesh();
        player::RuntimeNavigationAgentBridge navigation(scene, motor, navMesh);
        player::RuntimeNavigationAgentSettings navSettings;
        navSettings.MaximumSpeed = 2.5f;
        navSettings.WaypointRadius = 0.15f;
        navSettings.PathSnapDistance = 1.0f;
        navigation.Register(left, navSettings);
        navigation.Register(right, navSettings);

        ai::NavigationIntent leftIntent;
        leftIntent.Destination = { 4.0, 0.92, 0.0 };
        leftIntent.AcceptanceRadius = 0.20;
        ai::NavigationIntent rightIntent;
        rightIntent.Destination = { -4.0, 0.92, 0.0 };
        rightIntent.AcceptanceRadius = 0.20;
        Require(navigation.SetIntent(left, leftIntent),
            "Left crowd agent could not plan its crossing path.");
        Require(navigation.SetIntent(right, rightIntent),
            "Right crowd agent could not plan its crossing path.");

        player::RuntimeCrowdNavigationBridge crowd(scene, navigation);
        player::RuntimeCrowdAgentSettings crowdSettings;
        crowdSettings.Radius = 0.40f;
        crowdSettings.NeighborDistance = 5.0f;
        crowdSettings.TimeHorizon = 2.5f;
        crowdSettings.MaximumAcceleration = 6.0f;
        crowdSettings.StuckTimeBeforeReplan = 0.75f;
        crowd.Register(left, crowdSettings);
        crowd.Register(right, crowdSettings);

        constexpr float delta = 1.0f / 60.0f;
        const auto first = crowd.StepAll(delta);
        Require(first.size() == 2u,
            "Crowd batch did not return both registered navigation agents.");
        const float firstSpeed = std::sqrt(
            first[0].Crowd.AvoidedVelocity.x * first[0].Crowd.AvoidedVelocity.x +
            first[0].Crowd.AvoidedVelocity.z * first[0].Crowd.AvoidedVelocity.z);
        Require(firstSpeed <= crowdSettings.MaximumAcceleration * delta + 1.0e-4f,
            "Crowd command ignored the authored acceleration limit.");

        float minimumSeparation = PlanarDistance(scene, left, right);
        bool observedNeighborConstraint = false;
        bool leftArrived = false;
        bool rightArrived = false;
        for (unsigned frame = 0u; frame < 600u; ++frame)
        {
            const auto steps = crowd.StepAll(delta);
            for (const auto& step : steps)
            {
                observedNeighborConstraint = observedNeighborConstraint ||
                    step.Crowd.NeighborCount > 0u;
                if (step.Entity == left)
                    leftArrived = step.Navigation.Navigation.Status ==
                        player::RuntimeNavigationAgentStatus::Arrived;
                if (step.Entity == right)
                    rightArrived = step.Navigation.Navigation.Status ==
                        player::RuntimeNavigationAgentStatus::Arrived;
            }
            minimumSeparation = std::min(
                minimumSeparation, PlanarDistance(scene, left, right));
            if (leftArrived && rightArrived) break;
        }

        Require(observedNeighborConstraint,
            "Crossing NPCs never entered each other's crowd neighborhood.");
        Require(minimumSeparation > 0.55f,
            "Crowd-aware NPCs collapsed into an unsafe overlap while crossing.");
        Require(leftArrived && rightArrived,
            "Crowd-aware NPCs failed to complete opposing navigation intents.");
        Require(crowd.State(left).ReplanCount < 8u &&
                crowd.State(right).ReplanCount < 8u,
            "Crowd agents entered a replan storm instead of making progress.");

        crowd.SetStaticObstacles({ {
            .ID = 7u,
            .Position = { 0.0f, 0.0f, 0.0f },
            .Radius = 0.75f
        } });
        Require(crowd.StaticObstacles().size() == 1u,
            "Crowd static-obstacle replacement was not retained transactionally.");

        bool missingNavigationRejected = false;
        motor.Register(third);
        try { crowd.Register(third); }
        catch (const std::invalid_argument&) { missingNavigationRejected = true; }
        Require(missingNavigationRejected,
            "Crowd bridge accepted an entity without global navigation state.");

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "KairoPlayer crowd navigation test: " << error.what() << '\n';
        return 1;
    }
}
