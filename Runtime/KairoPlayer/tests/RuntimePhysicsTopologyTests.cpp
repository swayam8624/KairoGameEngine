#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Player.RuntimePhysicsBridge;

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

    engine::Entity AddFloor(engine::Scene& scene)
    {
        const auto floor = scene.CreateEntity("PersistentFloor");
        scene.Transform(floor).Local.Translation = { 0.0f, -0.5f, 0.0f };
        engine::ColliderComponent collider;
        collider.Shape = engine::ColliderShape::Box;
        collider.HalfExtents = { 20.0f, 0.5f, 20.0f };
        collider.BelongsTo = physics::CollisionLayer::StaticWorld;
        collider.CollidesWith = physics::CollisionLayer::All;
        scene.SetCollider(floor, collider);
        return floor;
    }

    engine::Entity AddDynamic(engine::Scene& scene)
    {
        const auto entity = scene.CreateEntity("PersistentDynamic");
        scene.Transform(entity).Local.Translation = { -4.0f, 3.0f, 0.0f };
        engine::RigidBodyComponent body;
        body.Motion = engine::RigidBodyMotion::Dynamic;
        body.Density = 1.0f;
        body.LinearDamping = 0.0f;
        body.AngularDamping = 0.0f;
        scene.SetRigidBody(entity, body);
        engine::ColliderComponent collider;
        collider.Shape = engine::ColliderShape::Sphere;
        collider.Radius = 0.5f;
        collider.BelongsTo = physics::CollisionLayer::DynamicWorld;
        collider.CollidesWith = physics::CollisionLayer::All;
        scene.SetCollider(entity, collider);
        return entity;
    }

    engine::Entity AddStreamedBlock(engine::Scene& scene, const char* name,
        float x, float y)
    {
        const auto entity = scene.CreateEntity(name);
        scene.Transform(entity).Local.Translation = { x, y, 0.0f };
        engine::ColliderComponent collider;
        collider.Shape = engine::ColliderShape::Box;
        collider.HalfExtents = { 0.75f, 0.75f, 0.75f };
        collider.BelongsTo = physics::CollisionLayer::StaticWorld;
        collider.CollidesWith = physics::CollisionLayer::All;
        scene.SetCollider(entity, collider);
        return entity;
    }

    void RequireSameSurvivorState(const physics::RigidBody& expected,
        const physics::RigidBody& actual)
    {
        Require(expected.Active == actual.Active,
            "Topology change altered the surviving body's active state.");
        Require(expected.Sleeping == actual.Sleeping,
            "Topology change altered the surviving body's sleep state.");
        Require(expected.State.Position == actual.State.Position,
            "Topology change altered the surviving body's position.");
        Require(expected.State.Rotation == actual.State.Rotation,
            "Topology change altered the surviving body's rotation.");
        Require(expected.State.LinearVelocity == actual.State.LinearVelocity,
            "Topology change altered the surviving body's linear velocity.");
        Require(expected.State.AngularVelocity == actual.State.AngularVelocity,
            "Topology change altered the surviving body's angular velocity.");
    }
}

int main()
{
    try
    {
        engine::Scene scene;
        (void)AddFloor(scene);
        const auto survivor = AddDynamic(scene);
        player::RuntimePhysicsBridge runtime(scene);
        const auto survivorBody = runtime.BodyFor(survivor);
        Require(survivorBody.has_value(),
            "Persistent dynamic body was not present in the initial runtime world.");

        runtime.ApplyEntityImpulse(survivor, { 3.0, 0.0, 1.5 });
        (void)runtime.Advance(1.0f / 60.0f);
        const physics::RigidBody beforeActivation =
            runtime.World().Bodies().at(*survivorBody);

        const auto streamed = AddStreamedBlock(scene, "StreamedBlock", 4.0f, 1.0f);
        const std::array activate{ streamed };
        const auto activation = runtime.ActivateEntities(activate);
        Require(activation.ActivatedBodies == 1u,
            "Streamed collider did not activate exactly one runtime body.");
        const auto streamedBody = runtime.BodyFor(streamed);
        Require(streamedBody.has_value() && runtime.World().IsValidBody(*streamedBody),
            "Streamed scene entity has no active physics body.");
        RequireSameSurvivorState(beforeActivation,
            runtime.World().Bodies().at(*survivorBody));

        const auto streamedHit = runtime.Raycast(
            { 4.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 10.0f);
        Require(streamedHit.has_value() && streamedHit->first == streamed,
            "Runtime raycast could not see the newly activated streamed collider.");

        const physics::RigidBody beforeDeactivation =
            runtime.World().Bodies().at(*survivorBody);
        const auto deactivation = runtime.DeactivateEntities(activate);
        Require(deactivation.DeactivatedBodies == 1u,
            "Streamed collider did not deactivate exactly one runtime body.");
        Require(!runtime.BodyFor(streamed).has_value(),
            "Deactivated streamed entity retained a runtime body mapping.");
        Require(!runtime.World().IsValidBody(*streamedBody),
            "Deactivated streamed body remained active in PhysicsWorld.");
        RequireSameSurvivorState(beforeDeactivation,
            runtime.World().Bodies().at(*survivorBody));

        const auto afterUnloadHit = runtime.Raycast(
            { 4.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 10.0f);
        Require(afterUnloadHit.has_value() && afterUnloadHit->first != streamed,
            "Raycast still returned an unloaded streamed body.");

        const auto candidate = AddStreamedBlock(scene,
            "TransactionalCandidate", 8.0f, 1.0f);
        const auto malformed = scene.CreateEntity("MalformedStreamedBody");
        engine::RigidBodyComponent invalidDescriptor;
        invalidDescriptor.Motion = engine::RigidBodyMotion::Dynamic;
        scene.SetRigidBody(malformed, invalidDescriptor);

        const auto snapshotBeforeFailure = runtime.CaptureSnapshot();
        const physics::RigidBody survivorBeforeFailure =
            runtime.World().Bodies().at(*survivorBody);
        const std::array invalidBatch{ candidate, malformed };
        bool rejected = false;
        try { (void)runtime.ActivateEntities(invalidBatch); }
        catch (const std::invalid_argument&) { rejected = true; }
        Require(rejected,
            "Transactional topology activation accepted a rigid body without a collider.");
        Require(!runtime.BodyFor(candidate).has_value(),
            "Failed topology transaction leaked an earlier candidate body mapping.");
        Require(!runtime.BodyFor(malformed).has_value(),
            "Failed topology transaction leaked the malformed body mapping.");
        Require(runtime.CaptureSnapshot().Bodies.size() == snapshotBeforeFailure.Bodies.size(),
            "Failed topology activation changed PhysicsWorld body storage.");
        Require(runtime.CaptureSnapshot().Colliders.size() ==
                snapshotBeforeFailure.Colliders.size(),
            "Failed topology activation changed PhysicsWorld collider storage.");
        RequireSameSurvivorState(survivorBeforeFailure,
            runtime.World().Bodies().at(*survivorBody));

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "KairoPlayer physics topology test: " << error.what() << '\n';
        return 1;
    }
}
