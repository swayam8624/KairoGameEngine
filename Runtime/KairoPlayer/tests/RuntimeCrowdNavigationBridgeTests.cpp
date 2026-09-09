#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
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

    [[nodiscard]] float PlanarDistance(
        const math::Vec3f& lhs, const math::Vec3f& rhs)
    {
        const float dx = lhs.x - rhs.x;
        const float dz = lhs.z - rhs.z;
        return std::sqrt(dx * dx + dz * dz);
    }

    [[nodiscard]] float PlanarSpeed(const math::Vec3f& velocity)
    {
        return std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
    }

    void AddFloor(engine::Scene& scene)
    {
        const auto floor = scene.CreateEntity("Floor");
        scene.Transform(floor).Local.Translation = { 0.0f, -0.5f, 0.0f };
        engine::ColliderComponent collider;
        collider.Shape = engine::ColliderShape::Box;
        collider.HalfExtents = { 8.0f, 0.5f, 8.0f };
        collider.BelongsTo = physics::CollisionLayer::StaticWorld;
        collider.CollidesWith = physics::CollisionLayer::All;
        scene.SetCollider(floor, collider);
    }

    [[nodiscard]] engine::Entity AddCharacter(engine::Scene& scene,
        const char* name, const math::Vec3f& position)
    {
        const auto character = scene.CreateEntity(name);
        scene.Transform(character).Local.Translation = position;
        engine::RigidBodyComponent body;
        body.Motion = engine::RigidBodyMotion::Kinematic;
        body.GravityScale = 0.0f;
        scene.SetRigidBody(character, body);
        engine::ColliderComponent collider;
        collider.Shape = engine::ColliderShape::Capsule;
        collider.Radius = 0.30f;
        collider.HalfHeight = 0.60f;
        collider.BelongsTo = physics::CollisionLayer::Player;
        // Inter-agent separation is intentionally left to the crowd solver in
        // this regression; the character motor still resolves the static world.
        collider.CollidesWith = physics::CollisionLayer::StaticWorld;
        scene.SetCollider(character, collider);
        return character;
    }

    [[nodiscard]] spatial::NavMesh MakeArenaNavMesh()
    {
        spatial::NavMesh nav;
        nav.AddPolygon(1u, {
            { -5.0f, 0.0f, -5.0f },
            {  5.0f, 0.0f, -5.0f },
            {  5.0f, 0.0f,  5.0f },
            { -5.0f, 0.0f,  5.0f }
        });
        nav.BuildAdjacency();
        return nav;
    }
}

int main()
{
    try
    {
        engine::Scene scene;
        AddFloor(scene);
        const auto eastbound = AddCharacter(
            scene, "Eastbound", { -2.0f, 0.92f, 0.0f });
        const auto northbound = AddCharacter(
            scene, "Northbound", { 0.0f, 0.92f, -2.0f });

        player::RuntimePhysicsBridge runtimePhysics(scene);
        player::RuntimeCharacterMotorBridge motor(scene, runtimePhysics);
        motor.Register(eastbound);
        motor.Register(northbound);

        const auto navMesh = MakeArenaNavMesh();
        player::RuntimeNavigationAgentBridge navigation(scene, motor, navMesh);
        player::RuntimeNavigationAgentSettings navigationSettings;
        navigationSettings.MaximumSpeed = 1.6f;
        navigationSettings.WaypointRadius = 0.10f;
        navigationSettings.PathSnapDistance = 1.0f;
        navigationSettings.VerticalTolerance = 2.0f;
        navigation.Register(eastbound, navigationSettings);
        navigation.Register(northbound, navigationSettings);

        ai::NavigationIntent eastIntent;
        eastIntent.Destination = { 2.0, 0.92, 0.0 };
        eastIntent.AcceptanceRadius = 0.15;
        ai::NavigationIntent northIntent;
        northIntent.Destination = { 0.0, 0.92, 2.0 };
        northIntent.AcceptanceRadius = 0.15;
        Require(navigation.SetIntent(eastbound, eastIntent),
            "Eastbound crowd agent did not receive a valid path.");
        Require(navigation.SetIntent(northbound, northIntent),
            "Northbound crowd agent did not receive a valid path.");

        player::RuntimeCrowdNavigationBridge crowd(scene, navigation);
        player::RuntimeCrowdAgentSettings crowdSettings;
        crowdSettings.Radius = 0.32f;
        crowdSettings.NeighborDistance = 4.0f;
        crowdSettings.TimeHorizon = 2.5f;
        crowdSettings.MaximumAcceleration = 5.0f;
        crowdSettings.StuckSpeedThreshold = 0.04f;
        crowdSettings.StuckReplanSeconds = 1.5f;
        crowd.Register(eastbound, crowdSettings);
        crowd.Register(northbound, crowdSettings);

        constexpr float dt = 1.0f / 60.0f;
        float minimumSeparation = std::numeric_limits<float>::max();
        bool sawCrowdConstraint = false;
        bool sawAvoidanceModification = false;
        bool eastArrived = false;
        bool northArrived = false;

        for (unsigned frame = 0u; frame < 600u; ++frame)
        {
            const auto steps = crowd.StepAll(dt);
            Require(steps.size() == 2u,
                "Crowd bridge did not emit one deterministic result per agent.");

            for (const auto& step : steps)
            {
                sawCrowdConstraint = sawCrowdConstraint ||
                    step.NeighborCount != 0u || step.ConstraintCount != 0u;
                const math::Vec3f steeringDelta =
                    step.AvoidanceVelocity - step.PreferredVelocity;
                sawAvoidanceModification = sawAvoidanceModification ||
                    PlanarSpeed(steeringDelta) > 1.0e-3f;
                Require(PlanarSpeed(step.AvoidanceVelocity) <=
                        navigationSettings.MaximumSpeed + 1.0e-4f,
                    "Crowd solver exceeded the navigation maximum speed.");
                if (step.Entity == eastbound)
                    eastArrived = step.Navigation.Status ==
                        player::RuntimeNavigationAgentStatus::Arrived;
                else if (step.Entity == northbound)
                    northArrived = step.Navigation.Status ==
                        player::RuntimeNavigationAgentStatus::Arrived;
            }

            minimumSeparation = std::min(minimumSeparation,
                PlanarDistance(scene.WorldTransform(eastbound).Translation,
                    scene.WorldTransform(northbound).Translation));
            if (eastArrived && northArrived) break;
        }

        Require(sawCrowdConstraint,
            "Crossing agents never entered one another's crowd neighborhood.");
        Require(sawAvoidanceModification,
            "Crowd solver never modified a collision-course preferred velocity.");
        Require(minimumSeparation >= 0.50f,
            "Crowd-steered kinematic agents overlapped beyond their authored radii.");
        Require(eastArrived && northArrived,
            "Crowd-aware physical agents failed to make progress to both goals.");

        const auto settled = crowd.StepAll(dt);
        for (const auto& step : settled)
        {
            Require(PlanarSpeed(step.AppliedVelocity) <= 1.0e-5f,
                "Arrived crowd agent retained physical planar motion.");
        }

        bool missingNavigationRejected = false;
        const auto unregistered = AddCharacter(
            scene, "Unregistered", { 3.0f, 0.92f, 3.0f });
        motor.Register(unregistered);
        try { crowd.Register(unregistered); }
        catch (const std::invalid_argument&) { missingNavigationRejected = true; }
        Require(missingNavigationRejected,
            "Crowd bridge accepted an entity without navigation registration.");

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "KairoPlayer crowd navigation test: " << error.what() << '\n';
        return 1;
    }
}
