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
            // Keep the opt-out sentinel physically independent from the controlled
            // capsule. It proves registration isolation without becoming an
            // accidental contact obstacle in the locomotion assertions below.
            scene.Transform(untouched).Local.Translation = { -4.0f, 0.92f, 0.0f };
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
            const auto diagonalVelocity = controller.DesiredPlanarVelocity();
            const float diagonalSpeed = std::sqrt(
                diagonalVelocity.x * diagonalVelocity.x +
                diagonalVelocity.z * diagonalVelocity.z);
            Require(std::abs(diagonalSpeed - 6.0f) < 1.0e-5f,
                "Normalized diagonal input did not preserve configured maximum speed.");

            // Verify the controller contract at its boundary. The downstream
            // character motor is a collision/grounding solver, so its resolved
            // world pose is intentionally not expected to equal velocity * time.
            move.Value = { 1.0f, 0.0f };
            controller.CaptureInput(move, {});
            const auto xVelocity = controller.DesiredPlanarVelocity();
            Require(std::abs(xVelocity.x - 6.0f) < 1.0e-5f &&
                    std::abs(xVelocity.y) < 1.0e-6f &&
                    std::abs(xVelocity.z) < 1.0e-6f,
                "Unit X input did not map to the configured planar speed.");
            const float startX = scene.WorldTransform(controlled).Translation.x;
            for (unsigned step = 0u; step < 10u; ++step)
                controller.BeforePhysicsStep(1.0f / 60.0f);
            Require(scene.WorldTransform(controlled).Translation.x > startX,
                "Positive player X input did not advance the character through the motor.");

            move.Value = { 0.0f, 1.0f };
            controller.CaptureInput(move, {});
            const auto zVelocity = controller.DesiredPlanarVelocity();
            Require(std::abs(zVelocity.x) < 1.0e-6f &&
                    std::abs(zVelocity.z + 6.0f) < 1.0e-5f,
                "Player-controller axis mapping did not map positive action Y to world -Z speed.");
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
