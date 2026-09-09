module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <stdexcept>
#include <unordered_map>
#include <utility>

export module Kairo.Player.RuntimeCharacterMotorBridge;

import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.PhysicsMath.Types;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Player.RuntimePhysicsBridge;

export namespace kairo::player
{
    struct RuntimeCharacterMotorSettings final
    {
        float Gravity = -24.0f;
        float JumpSpeed = 8.0f;
        float MaximumFallSpeed = 40.0f;
        float SkinWidth = 0.02f;
        float GroundProbeDistance = 0.12f;
        float MaxSlopeAngleRadians = 0.872664626f;
        std::uint32_t MaxSlideIterations = 6u;
        std::uint32_t SweepSamples = 5u;

        void Validate() const
        {
            if (!std::isfinite(Gravity) || Gravity > 0.0f)
                throw std::invalid_argument(
                    "Runtime character gravity must be finite and non-positive.");
            if (!std::isfinite(JumpSpeed) || JumpSpeed < 0.0f)
                throw std::invalid_argument(
                    "Runtime character jump speed must be finite and non-negative.");
            if (!std::isfinite(MaximumFallSpeed) || MaximumFallSpeed <= 0.0f)
                throw std::invalid_argument(
                    "Runtime character maximum fall speed must be finite and positive.");
            if (!std::isfinite(SkinWidth) || SkinWidth < 0.0f)
                throw std::invalid_argument(
                    "Runtime character skin width must be finite and non-negative.");
            if (!std::isfinite(GroundProbeDistance) || GroundProbeDistance < 0.0f)
                throw std::invalid_argument(
                    "Runtime character ground probe distance must be finite and non-negative.");
            if (!std::isfinite(MaxSlopeAngleRadians) || MaxSlopeAngleRadians < 0.0f ||
                MaxSlopeAngleRadians >= std::numbers::pi_v<float> * 0.5f)
                throw std::invalid_argument(
                    "Runtime character maximum slope angle must be in [0, pi/2).");
            if (MaxSlideIterations == 0u || MaxSlideIterations > 16u)
                throw std::invalid_argument(
                    "Runtime character slide iterations must be within 1..16.");
            if (SweepSamples == 0u || SweepSamples > 17u)
                throw std::invalid_argument(
                    "Runtime character sweep samples must be within 1..17.");
        }
    };

    struct RuntimeCharacterMotorState final
    {
        float VerticalVelocity = 0.0f;
        bool Grounded = false;
        kairo::foundation::math::Vec3f GroundNormal =
            kairo::foundation::math::Vec3f::Up();
    };

    struct RuntimeCharacterMotorStep final
    {
        kairo::foundation::physics::CharacterMoveResult Move;
        RuntimeCharacterMotorState State;
        bool Jumped = false;
    };

    /// Gameplay-facing kinematic character layer over RuntimePhysicsBridge.
    /// The authored Scene remains the persistent source of shape/filter data;
    /// KairoPhysicsEngine owns collision queries; this bridge owns only the
    /// transient motor velocity and entity-to-controller mapping.
    class RuntimeCharacterMotorBridge final
    {
    public:
        RuntimeCharacterMotorBridge(kairo::engine::Scene& scene,
            RuntimePhysicsBridge& physics)
            : m_Scene(scene), m_Physics(physics) {}

        void Register(kairo::engine::Entity entity,
            RuntimeCharacterMotorSettings settings = {})
        {
            settings.Validate();
            if (!m_Scene.Contains(entity))
                throw std::out_of_range(
                    "Cannot register an unknown runtime character entity.");
            if (m_Characters.contains(entity.Value))
                throw std::invalid_argument(
                    "Runtime character entity is already registered.");
            if (!m_Scene.HasRigidBody(entity) || !m_Scene.HasCollider(entity))
                throw std::invalid_argument(
                    "Runtime character requires authored rigid-body and collider components.");

            const auto& body = m_Scene.RigidBody(entity);
            if (body.Motion != kairo::engine::RigidBodyMotion::Kinematic)
                throw std::invalid_argument(
                    "Runtime character rigid body must be authored as Kinematic.");
            const auto& collider = m_Scene.Collider(entity);
            if (collider.Shape != kairo::engine::ColliderShape::Capsule)
                throw std::invalid_argument(
                    "Runtime character requires an authored capsule collider.");

            const auto worldTransform = m_Scene.WorldTransform(entity);
            const kairo::foundation::math::Vec3f absoluteScale{
                std::abs(worldTransform.Scale.x),
                std::abs(worldTransform.Scale.y),
                std::abs(worldTransform.Scale.z) };
            const float radius = collider.Radius *
                std::max(absoluteScale.x, absoluteScale.z);
            const float halfSegment = collider.HalfHeight * absoluteScale.y;

            kairo::foundation::physics::CharacterControllerConfig config;
            config.Radius = radius;
            config.Height = 2.0f * (halfSegment + radius);
            config.SkinWidth = std::min(settings.SkinWidth, radius * 0.5f);
            config.GroundProbeDistance = settings.GroundProbeDistance;
            config.MaxSlopeAngleRadians = settings.MaxSlopeAngleRadians;
            config.MaxSlideIterations = settings.MaxSlideIterations;
            config.SweepSamples = settings.SweepSamples;
            config.CollisionMask = collider.CollidesWith;
            config.EnableGroundSnap = true;
            config.Validate();

            const auto bodyID = m_Physics.BodyFor(entity);
            if (!bodyID.has_value())
                throw std::logic_error(
                    "Runtime character has no PhysicsWorld body mapping.");
            const auto colliderID = FindOwnedCollider(*bodyID);

            CharacterRecord record{
                std::move(settings),
                kairo::foundation::physics::KinematicCharacterController(
                    config, worldTransform.Translation),
                colliderID,
                {}
            };
            const auto initialGround = record.Controller.ProbeGround(
                m_Physics.World(), record.OwnCollider);
            record.State.Grounded = initialGround.Grounded;
            if (initialGround.Grounded)
                record.State.GroundNormal = initialGround.Normal;
            m_Characters.emplace(entity.Value, std::move(record));
        }

        bool Unregister(kairo::engine::Entity entity) noexcept
        {
            return m_Characters.erase(entity.Value) != 0u;
        }

        [[nodiscard]] bool IsRegistered(kairo::engine::Entity entity) const noexcept
        {
            return m_Characters.contains(entity.Value);
        }

        [[nodiscard]] const RuntimeCharacterMotorState& State(
            kairo::engine::Entity entity) const
        {
            return Require(entity).State;
        }

        void Teleport(kairo::engine::Entity entity,
            const kairo::foundation::math::Vec3f& worldPosition)
        {
            auto& record = Require(entity);
            record.Controller.Teleport(worldPosition);
            record.State = {};
            m_Physics.SetEntityPosition(entity, {
                static_cast<double>(worldPosition.x),
                static_cast<double>(worldPosition.y),
                static_cast<double>(worldPosition.z) });
        }

        /// `desiredPlanarVelocity` is world-space units/second. Its Y component
        /// is rejected so gravity/jump remain single-owner deterministic state.
        [[nodiscard]] RuntimeCharacterMotorStep Step(
            kairo::engine::Entity entity,
            const kairo::foundation::math::Vec3f& desiredPlanarVelocity,
            bool jumpRequested,
            float deltaSeconds)
        {
            auto& record = Require(entity);
            if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f || deltaSeconds > 0.25f)
                throw std::invalid_argument(
                    "Runtime character delta must be finite and within (0, 0.25].");
            if (!std::isfinite(desiredPlanarVelocity.x) ||
                !std::isfinite(desiredPlanarVelocity.y) ||
                !std::isfinite(desiredPlanarVelocity.z))
                throw std::invalid_argument(
                    "Runtime character desired velocity must be finite.");
            if (std::abs(desiredPlanarVelocity.y) > 1.0e-5f)
                throw std::invalid_argument(
                    "Runtime character desired planar velocity must have zero Y.");

            // External gameplay/streaming teleports remain authoritative. Sync
            // the query controller to the Scene's current world pose before the
            // new motor step instead of retaining a stale private copy.
            const auto sceneWorld = m_Scene.WorldTransform(entity);
            const auto drift = sceneWorld.Translation - record.Controller.Position();
            if (drift.LengthSquared() > 1.0e-10f)
                record.Controller.Teleport(sceneWorld.Translation);

            const auto preGround = record.Controller.ProbeGround(
                m_Physics.World(), record.OwnCollider);
            record.State.Grounded = preGround.Grounded;
            if (preGround.Grounded)
                record.State.GroundNormal = preGround.Normal;

            bool jumped = false;
            if (jumpRequested && record.State.Grounded && record.Settings.JumpSpeed > 0.0f)
            {
                record.State.VerticalVelocity = record.Settings.JumpSpeed;
                record.State.Grounded = false;
                jumped = true;
            }
            else
            {
                record.State.VerticalVelocity = std::max(
                    -record.Settings.MaximumFallSpeed,
                    record.State.VerticalVelocity + record.Settings.Gravity * deltaSeconds);
            }

            const kairo::foundation::math::Vec3f displacement{
                desiredPlanarVelocity.x * deltaSeconds,
                record.State.VerticalVelocity * deltaSeconds,
                desiredPlanarVelocity.z * deltaSeconds };
            auto move = record.Controller.Move(
                m_Physics.World(), displacement, record.OwnCollider);

            if (move.HitCeiling && record.State.VerticalVelocity > 0.0f)
                record.State.VerticalVelocity = 0.0f;
            if (move.Ground.Grounded && record.State.VerticalVelocity <= 0.0f)
            {
                record.State.VerticalVelocity = 0.0f;
                record.State.Grounded = true;
                record.State.GroundNormal = move.Ground.Normal;
            }
            else if (!jumped)
            {
                record.State.Grounded = move.Ground.Grounded;
                if (move.Ground.Grounded)
                    record.State.GroundNormal = move.Ground.Normal;
            }

            const auto resolved = record.Controller.Position();
            m_Physics.SetEntityPosition(entity, {
                static_cast<double>(resolved.x),
                static_cast<double>(resolved.y),
                static_cast<double>(resolved.z) });
            return { std::move(move), record.State, jumped };
        }

    private:
        struct CharacterRecord final
        {
            RuntimeCharacterMotorSettings Settings;
            kairo::foundation::physics::KinematicCharacterController Controller;
            kairo::foundation::physics::ColliderID OwnCollider =
                kairo::foundation::physics::InvalidColliderID;
            RuntimeCharacterMotorState State;
        };

        kairo::engine::Scene& m_Scene;
        RuntimePhysicsBridge& m_Physics;
        std::unordered_map<std::uint32_t, CharacterRecord> m_Characters;

        [[nodiscard]] CharacterRecord& Require(kairo::engine::Entity entity)
        {
            const auto found = m_Characters.find(entity.Value);
            if (found == m_Characters.end())
                throw std::out_of_range(
                    "Runtime character entity is not registered.");
            return found->second;
        }

        [[nodiscard]] const CharacterRecord& Require(
            kairo::engine::Entity entity) const
        {
            const auto found = m_Characters.find(entity.Value);
            if (found == m_Characters.end())
                throw std::out_of_range(
                    "Runtime character entity is not registered.");
            return found->second;
        }

        [[nodiscard]] kairo::foundation::physics::ColliderID FindOwnedCollider(
            kairo::foundation::physics::BodyID body) const
        {
            kairo::foundation::physics::ColliderID found =
                kairo::foundation::physics::InvalidColliderID;
            for (const auto& collider : m_Physics.World().Colliders())
            {
                if (!collider.Active || collider.Body != body) continue;
                if (found != kairo::foundation::physics::InvalidColliderID)
                    throw std::invalid_argument(
                        "Runtime character currently requires exactly one collider on its body.");
                found = collider.ID;
            }
            if (found == kairo::foundation::physics::InvalidColliderID)
                throw std::logic_error(
                    "Runtime character body has no active collider.");
            return found;
        }
    };
}
