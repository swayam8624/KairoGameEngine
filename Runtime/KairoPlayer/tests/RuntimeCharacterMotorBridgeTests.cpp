#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeCharacterMotorBridge;

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

    [[nodiscard]] engine::Entity AddFloor(engine::Scene& scene)
    {
        const auto floor = scene.CreateEntity("Floor");
        scene.Transform(floor).Local.Translation = { 0.0f, -0.5f, 0.0f };
        engine::ColliderComponent collider;
        collider.Shape = engine::ColliderShape::Box;
        collider.HalfExtents = { 8.0f, 0.5f, 8.0f };
        collider.BelongsTo = physics::CollisionLayer::StaticWorld;
        collider.CollidesWith = physics::CollisionLayer::All;
        scene.SetCollider(floor, collider);
        return floor;
    }

    [[nodiscard]] engine::Entity AddCharacter(engine::Scene& scene,
        math::Vec3f position = { 0.0f, 0.92f, 0.0f })
    {
        const auto character = scene.CreateEntity("Character");
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
        collider.CollidesWith = physics::CollisionLayer::StaticWorld;
        scene.SetCollider(character, collider);
        return character;
    }

    [[nodiscard]] engine::Entity AddWall(engine::Scene& scene)
    {
        const auto wall = scene.CreateEntity("Wall");
        scene.Transform(wall).Local.Translation = { 2.0f, 1.0f, 0.0f };
        engine::ColliderComponent collider;
        collider.Shape = engine::ColliderShape::Box;
        collider.HalfExtents = { 0.25f, 1.0f, 4.0f };
        collider.BelongsTo = physics::CollisionLayer::StaticWorld;
        collider.CollidesWith = physics::CollisionLayer::All;
        scene.SetCollider(wall, collider);
        return wall;
    }

    [[nodiscard]] float WorldY(const engine::Scene& scene, engine::Entity entity)
    {
        return scene.WorldTransform(entity).Translation.y;
    }
}

int main()
{
    try
    {
        {
            engine::Scene scene;
            (void)AddFloor(scene);
            const auto character = AddCharacter(scene);
            player::RuntimePhysicsBridge runtimePhysics(scene);
            player::RuntimeCharacterMotorBridge motor(scene, runtimePhysics);
            motor.Register(character);

            Require(motor.IsRegistered(character),
                "Registered character is missing from the runtime motor.");
            Require(motor.State(character).Grounded,
                "Character standing at authored floor skin did not register grounded.");

            for (unsigned frame = 0u; frame < 60u; ++frame)
            {
                const auto step = motor.Step(character,
                    { 2.0f, 0.0f, 0.0f }, false, 1.0f / 60.0f);
                Require(step.State.Grounded,
                    "Planar locomotion unexpectedly lost stable floor grounding.");
            }
            const auto position = scene.WorldTransform(character).Translation;
            Require(std::abs(position.x - 2.0f) < 0.05f,
                "One second of 2 m/s character input did not travel approximately two metres.");
            Require(std::abs(position.y - 0.92f) < 0.03f,
                "Planar locomotion drifted vertically away from the floor skin.");

            const auto body = runtimePhysics.BodyFor(character);
            Require(body.has_value(),
                "Character lost its runtime physics body mapping.");
            Require((runtimePhysics.World().Bodies().at(*body).State.Position - position).Length() < 1.0e-4f,
                "Character motor did not synchronize resolved scene and PhysicsWorld positions.");
        }

        {
            engine::Scene scene;
            (void)AddFloor(scene);
            (void)AddWall(scene);
            const auto character = AddCharacter(scene);
            player::RuntimePhysicsBridge runtimePhysics(scene);
            player::RuntimeCharacterMotorBridge motor(scene, runtimePhysics);
            motor.Register(character);

            bool hitWall = false;
            for (unsigned frame = 0u; frame < 120u; ++frame)
            {
                const auto step = motor.Step(character,
                    { 4.0f, 0.0f, 1.0f }, false, 1.0f / 60.0f);
                hitWall = hitWall || step.Move.HitWall;
            }
            const auto position = scene.WorldTransform(character).Translation;
            Require(hitWall,
                "Character motor never reported the authored blocking wall.");
            Require(position.x < 1.50f,
                "Character motor penetrated through the blocking wall.");
            Require(position.z > 1.0f,
                "Character motor failed to retain tangential slide motion along the wall.");
        }

        {
            engine::Scene scene;
            (void)AddFloor(scene);
            const auto character = AddCharacter(scene);
            player::RuntimePhysicsBridge runtimePhysics(scene);
            player::RuntimeCharacterMotorBridge motor(scene, runtimePhysics);
            player::RuntimeCharacterMotorSettings settings;
            settings.Gravity = -20.0f;
            settings.JumpSpeed = 6.0f;
            motor.Register(character, settings);

            const float startY = WorldY(scene, character);
            const auto jump = motor.Step(character,
                math::Vec3f::Zero(), true, 1.0f / 60.0f);
            Require(jump.Jumped,
                "Grounded jump request did not start a jump.");
            Require(jump.State.VerticalVelocity > 0.0f,
                "Jump did not establish positive vertical velocity.");
            Require(WorldY(scene, character) > startY,
                "Jump did not move the character upward.");

            float apex = WorldY(scene, character);
            bool landed = false;
            for (unsigned frame = 0u; frame < 180u; ++frame)
            {
                const auto step = motor.Step(character,
                    math::Vec3f::Zero(), false, 1.0f / 60.0f);
                apex = std::max(apex, WorldY(scene, character));
                if (step.State.Grounded && frame > 5u)
                {
                    landed = true;
                    break;
                }
            }
            Require(apex > startY + 0.5f,
                "Character jump never developed a meaningful airborne arc.");
            Require(landed,
                "Character did not return to the walkable floor under motor gravity.");
            Require(std::abs(WorldY(scene, character) - 0.92f) < 0.04f,
                "Landed character did not settle back to floor skin height.");
            Require(std::abs(motor.State(character).VerticalVelocity) < 1.0e-5f,
                "Landed character retained downward motor velocity.");
        }

        {
            engine::Scene scene;
            (void)AddFloor(scene);
            const auto badBody = scene.CreateEntity("DynamicCharacter");
            scene.Transform(badBody).Local.Translation = { 0.0f, 0.92f, 0.0f };
            scene.SetRigidBody(badBody, {});
            engine::ColliderComponent capsule;
            capsule.Shape = engine::ColliderShape::Capsule;
            capsule.Radius = 0.30f;
            capsule.HalfHeight = 0.60f;
            scene.SetCollider(badBody, capsule);

            player::RuntimePhysicsBridge runtimePhysics(scene);
            player::RuntimeCharacterMotorBridge motor(scene, runtimePhysics);
            bool rejected = false;
            try { motor.Register(badBody); }
            catch (const std::invalid_argument&) { rejected = true; }
            Require(rejected,
                "Runtime character accepted a non-kinematic authored body.");
        }

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "KairoPlayer character motor test: " << error.what() << '\n';
        return 1;
    }
}
