#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

import Kairo.AI.Gameplay;
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Foundation.Spatial.NavMesh;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeCharacterMotorBridge;
import Kairo.Player.RuntimeNavigationAgentBridge;

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
        scene.Transform(floor).Local.Translation = { 2.0f, -0.5f, 0.0f };
        engine::ColliderComponent collider;
        collider.Shape = engine::ColliderShape::Box;
        collider.HalfExtents = { 8.0f, 0.5f, 4.0f };
        collider.BelongsTo = physics::CollisionLayer::StaticWorld;
        collider.CollidesWith = physics::CollisionLayer::All;
        scene.SetCollider(floor, collider);
        return floor;
    }

    [[nodiscard]] engine::Entity AddCharacter(engine::Scene& scene)
    {
        const auto character = scene.CreateEntity("NPC");
        scene.Transform(character).Local.Translation = { 0.0f, 0.92f, 0.0f };
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
            { -2.0f, 0.0f, -2.0f },
            {  2.0f, 0.0f, -2.0f },
            {  2.0f, 0.0f,  2.0f },
            { -2.0f, 0.0f,  2.0f }
        });
        nav.AddPolygon(2u, {
            { 2.0f, 0.0f, -2.0f },
            { 6.0f, 0.0f, -2.0f },
            { 6.0f, 0.0f,  2.0f },
            { 2.0f, 0.0f,  2.0f }
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
        (void)AddFloor(scene);
        const auto npc = AddCharacter(scene);
        player::RuntimePhysicsBridge runtimePhysics(scene);
        player::RuntimeCharacterMotorBridge motor(scene, runtimePhysics);
        motor.Register(npc);
        const auto navMesh = MakeNavMesh();
        player::RuntimeNavigationAgentBridge navigation(scene, motor, navMesh);
        player::RuntimeNavigationAgentSettings settings;
        settings.MaximumSpeed = 3.0f;
        settings.WaypointRadius = 0.15f;
        settings.PathSnapDistance = 1.0f;
        settings.VerticalTolerance = 2.0f;
        navigation.Register(npc, settings);

        ai::NavigationIntent intent;
        intent.Destination = { 5.0, 0.92, 0.0 };
        intent.AcceptanceRadius = 0.20;
        Require(navigation.SetIntent(npc, intent),
            "Navigation intent did not produce a path across adjacent polygons.");
        Require(navigation.State(npc).Status ==
                player::RuntimeNavigationAgentStatus::FollowingPath,
            "Navigation agent did not enter path-following state.");
        Require(navigation.Path(npc).Reached &&
                navigation.Path(npc).Corridor.size() == 2u,
            "Navigation agent did not preserve the NavMesh corridor.");

        bool arrived = false;
        for (unsigned frame = 0u; frame < 240u; ++frame)
        {
            const auto step = navigation.Step(npc, 1.0f / 60.0f);
            if (step.Navigation.Status ==
                player::RuntimeNavigationAgentStatus::Arrived)
            {
                arrived = true;
                break;
            }
            Require(step.Motor.has_value(),
                "Path-following frame failed to drive the character motor.");
        }
        Require(arrived,
            "AI navigation intent did not reach its authored destination.");
        const auto finalPosition = scene.WorldTransform(npc).Translation;
        Require(std::abs(finalPosition.x - 5.0f) <= 0.25f,
            "Navigation agent stopped outside its acceptance radius.");
        Require(navigation.State(npc).RemainingPlanarDistance == 0.0f,
            "Arrived navigation agent retained remaining path distance.");
        const auto afterArrival = navigation.Step(npc, 1.0f / 60.0f);
        Require(!afterArrival.Motor.has_value(),
            "Arrived navigation agent continued issuing locomotion commands.");

        ai::NavigationIntent unreachable;
        unreachable.Destination = { 100.0, 0.92, 100.0 };
        unreachable.AcceptanceRadius = 0.25;
        Require(!navigation.SetIntent(npc, unreachable),
            "Navigation agent reported a route to an unsnappable destination.");
        Require(navigation.State(npc).Status ==
                player::RuntimeNavigationAgentStatus::PathUnavailable,
            "Failed path did not publish PathUnavailable state.");
        const auto unavailable = navigation.Step(npc, 1.0f / 60.0f);
        Require(!unavailable.Motor.has_value(),
            "Path-unavailable navigation agent issued motor commands.");

        navigation.ClearIntent(npc);
        Require(navigation.State(npc).Status ==
                player::RuntimeNavigationAgentStatus::Idle,
            "Clearing navigation intent did not restore Idle state.");
        Require(!navigation.Intent(npc).has_value(),
            "Clearing navigation intent retained cognition state.");

        bool missingMotorRejected = false;
        const auto second = AddCharacter(scene);
        try { navigation.Register(second); }
        catch (const std::invalid_argument&) { missingMotorRejected = true; }
        Require(missingMotorRejected,
            "Navigation agent accepted an entity without a registered motor.");

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "KairoPlayer navigation agent test: " << error.what() << '\n';
        return 1;
    }
}
