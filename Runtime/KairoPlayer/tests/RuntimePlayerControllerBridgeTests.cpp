#include <cmath>
#include <iostream>
#include <stdexcept>

import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Player.RuntimeCharacterMotorBridge;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimePlayerControllerBridge;

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

    void AddFloor(engine::Scene& scene)
    {
        const auto floor = scene.CreateEntity("Floor");
        scene.Transform(floor).Local.Translation = { 0.0f, -0.5f, 0.0f };
        engine::ColliderComponent collider;
        collider.Shape = engine::ColliderShape::Box;
        collider.HalfExtents = { 12.0f, 0.5f, 12.0f };
        collider.BelongsTo = physics::CollisionLayer::StaticWorld;
        collider.CollidesWith = physics::CollisionLayer::All;
        scene.SetCollider(floor, collider);
    }

    [[nodiscard]] engine::Entity AddController(engine::Scene& scene,
        bool optedIn = true, engine::RigidBodyMotion motion = engine::RigidBodyMotion::Kinematic)
    {
        const auto entity = scene.CreateEntity("Player");
        scene.Transform(entity).Local.Translation = { 0.0f, 0.92f, 0.0f };
        if (optedIn) scene.AddTag(entity, std::string(player::RuntimePlayerControllerTag));
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
}

int main()
{
    try
    {
        {
            engine::Scene scene;
            AddFloor(scene);
            const auto controlled = AddController(scene);
            const auto untouched = AddController(scene, false);
            player::RuntimePhysicsBridge runtimePhysics(scene);
            player::RuntimeCharacterMotorBridge motor(scene, runtimePhysics);
            player::RuntimePlayerControllerSettings settings;
            settings.MaximumSpeed = 6.0f;
            player::RuntimePlayerControllerBridge controller(scene, motor, settings);

            Require(controller.ControllerCount() == 1u && controller.Controls(controlled),
                "Player-controller opt-in tag did not register exactly the intended entity.");
            Require(!controller.Controls(untouched) && !motor.IsRegistered(untouched),
                "Player controller touched an entity that did not explicitly opt in.");

            engine::InputActionState move;
            move.Value = { 1.0f, 1.0f };
            controller.CaptureInput(move, {});
            const float normalizedLength = std::sqrt(
                controller.State().Move.X * controller.State().Move.X +
                controller.State().Move.Y * controller.State().Move.Y);
            Require(std::abs(normalizedLength - 1.0f) < 1.0e-5f,
                "Diagonal player input was not normalized before locomotion.");

            // Keep speed verification independent from diagonal solver contact:
            // the normalization invariant is proven above, while this trajectory
            // verifies that the controller maps unit action magnitude to the exact
            // configured world-space speed.
            move.Value = { 1.0f, 0.0f };
            controller.CaptureInput(move, {});
            for (unsigned step = 0u; step < 60u; ++step)
                controller.BeforePhysicsStep(1.0f / 60.0f);
            auto position = scene.WorldTransform(controlled).Translation;
            Require(std::abs(position.x - 6.0f) < 0.12f && std::abs(position.z) < 0.05f,
                "One second of unit player input did not travel at configured maximum speed.");

            move.Value = { 0.0f, 1.0f };
            controller.CaptureInput(move, {});
            controller.BeforePhysicsStep(1.0f / 60.0f);
            position = scene.WorldTransform(controlled).Translation;
            Require(position.z < -0.05f,
                "Player-controller axis mapping did not map positive action Y toward world -Z.");
        }

        {
            engine::Scene scene;
            AddFloor(scene);
            const auto controlled = AddController(scene);
            player::RuntimePhysicsBridge runtimePhysics(scene);
            player::RuntimeCharacterMotorBridge motor(scene, runtimePhysics);
            player::RuntimePlayerControllerBridge controller(scene, motor);
            const float startY = scene.WorldTransform(controlled).Translation.y;

            engine::InputActionState jump;
            jump.Pressed = true;
            controller.CaptureInput({}, jump);
            Require(controller.State().JumpQueued,
                "Pressed jump action was not latched until the fixed step.");
            controller.BeforePhysicsStep(1.0f / 60.0f);
            Require(!controller.State().JumpQueued,
                "Jump edge was replayable after its fixed-step consumption.");
            Require(scene.WorldTransform(controlled).Translation.y > startY,
                "Latched jump did not move the grounded kinematic character upward.");
        }

        {
            engine::Scene scene;
            AddFloor(scene);
            (void)AddController(scene, true, engine::RigidBodyMotion::Dynamic);
            player::RuntimePhysicsBridge runtimePhysics(scene);
            player::RuntimeCharacterMotorBridge motor(scene, runtimePhysics);
            bool rejected = false;
            try { player::RuntimePlayerControllerBridge controller(scene, motor); }
            catch (const std::invalid_argument&) { rejected = true; }
            Require(rejected,
                "Player-controller accepted an opted-in dynamic body instead of a kinematic capsule.");
        }

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "KairoPlayer player-controller test: " << error.what() << '\n';
        return 1;
    }
}
