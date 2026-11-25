module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

export module Kairo.Player.RuntimePlayerControllerBridge;

import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Player.RuntimeCharacterMotorBridge;
import Kairo.Player.RuntimePhysicsBridge;

export namespace kairo::player
{
    inline constexpr std::string_view RuntimePlayerControllerTag =
        "kairo.player-controller";

    // Authored controller settings in world units per second. Motor owns
    // contact geometry and physical integration, not the action map.
    struct RuntimePlayerControllerSettings final
    {
        float MaximumSpeed = 5.5f;
        bool InvertMoveYForWorldZ = true;
        RuntimeCharacterMotorSettings Motor{};

        // Reject invalid speed and delegate motor constraints to its own API.
        void Validate() const
        {
            if (!std::isfinite(MaximumSpeed) || MaximumSpeed <= 0.0f)
                throw std::invalid_argument(
                    "Runtime player-controller speed must be finite and positive.");
            Motor.Validate();
        }
    };

    // Frame input survives until fixed physics consumes it. Movement is
    // continuous state; JumpQueued is a one-shot pending event.
    struct RuntimePlayerControllerState final
    {
        kairo::engine::InputVector2 Move{};
        bool JumpQueued = false;
    };

    /// Small reusable bridge from project input semantics to Kairo's kinematic
    /// character motor. A Scene opts into this built-in controller explicitly by
    /// tagging a kinematic capsule entity `kairo.player-controller`; ordinary
    /// entities and projects with custom gameplay remain completely untouched.
    ///
    /// Input is latched once per rendered frame and consumed only at a fixed
    /// physics boundary. This preserves edge-triggered jump requests when a render
    /// frame produces zero simulation steps, while also ensuring one key press is
    /// never replayed across multiple catch-up steps.
    class RuntimePlayerControllerBridge final : public RuntimeFixedStepListener
    {
    public:
        // Scene and motor are borrowed; tag discovery is a startup snapshot.
        // Register only active opted-in entities. Later-created tagged entities
        // need explicit lifecycle integration rather than implicit discovery.
        RuntimePlayerControllerBridge(kairo::engine::Scene& scene,
            RuntimeCharacterMotorBridge& motor,
            RuntimePlayerControllerSettings settings = {})
            : m_Scene(scene), m_Motor(motor), m_Settings(settings)
        {
            m_Settings.Validate();
            for (const auto entity : m_Scene.Entities())
            {
                if (!m_Scene.IsActiveInHierarchy(entity) ||
                    !m_Scene.HasTag(entity, RuntimePlayerControllerTag))
                    continue;
                m_Motor.Register(entity, m_Settings.Motor);
                m_Controllers.push_back(entity);
            }
        }

        // Count startup registrations, not the current active hierarchy size.
        [[nodiscard]] std::size_t ControllerCount() const noexcept
        {
            return m_Controllers.size();
        }

        /// A fixed, deterministic camera-follow target. Other game projects
        /// remain free to provide their own camera systems.
        [[nodiscard]] std::optional<kairo::engine::Entity> PrimaryController() const noexcept
        {
            if (m_Controllers.empty()) return std::nullopt;
            return m_Controllers.front();
        }

        /// Measured world-space displacement since an authored spawn anchor.
        /// A game camera may translate by this amount while retaining the
        /// designer's original angle, distance, FOV and projection settings.
        [[nodiscard]] kairo::foundation::math::Vec3f CameraFollowDelta(
            const kairo::foundation::math::Vec3f& spawnAnchor) const
        {
            const auto entity = PrimaryController();
            if (!entity.has_value() || !m_Scene.Contains(*entity))
                return {};
            return m_Scene.WorldTransform(*entity).Translation - spawnAnchor;
        }

        // Membership test against the startup registration vector.
        [[nodiscard]] bool Controls(kairo::engine::Entity entity) const noexcept
        {
            return std::ranges::find(m_Controllers, entity) != m_Controllers.end();
        }

        // Borrow current frame-latched state; invalidated semantically by capture
        // or clear. This is input intent, not the actual motor result.
        [[nodiscard]] const RuntimePlayerControllerState& State() const noexcept
        {
            return m_State;
        }

        /// The exact world-space planar velocity command produced by the current
        /// normalized action state. Collision resolution belongs to
        /// RuntimeCharacterMotorBridge; exposing this boundary makes controller
        /// speed/axis semantics deterministic and directly testable without
        /// assuming the collision solver must integrate to an exact final pose.
        [[nodiscard]] kairo::foundation::math::Vec3f DesiredPlanarVelocity() const noexcept
        {
            // Action-map Y is mapped onto the horizontal world Z axis; the
            // vertical component is zero because jumping belongs to the motor.
            const float worldZ = (m_Settings.InvertMoveYForWorldZ ? -1.0f : 1.0f) *
                m_State.Move.Y * m_Settings.MaximumSpeed;
            return {
                m_State.Move.X * m_Settings.MaximumSpeed,
                0.0f,
                worldZ
            };
        }

        /// Latch platform-neutral action states after RuntimeInputBridge polls the
        /// native window. The axis is normalized so diagonal WASD/gamepad input
        /// cannot exceed MaximumSpeed.
        void CaptureInput(const kairo::engine::InputActionState& move,
            const kairo::engine::InputActionState& jump)
        {
            m_State.Move = move.Value;
            const float lengthSquared = m_State.Move.X * m_State.Move.X +
                m_State.Move.Y * m_State.Move.Y;
            // Normalize only outside the unit disk, preserving analog magnitude
            // for slow movement while limiting diagonal keyboard speed.
            if (lengthSquared > 1.0f)
            {
                const float inverseLength = 1.0f / std::sqrt(lengthSquared);
                m_State.Move.X *= inverseLength;
                m_State.Move.Y *= inverseLength;
            }
            if (jump.Pressed) m_State.JumpQueued = true;
        }

        // Explicit neutralization discards both held movement and queued jump.
        void ClearInput() noexcept
        {
            m_State = {};
        }

        // Physics invokes this once per fixed step, before integration. One
        // captured jump is delivered to the registered active motor set and
        // consumed; render frames with no fixed step retain that event.
        void BeforePhysicsStep(float fixedDeltaSeconds) override
        {
            const auto desiredVelocity = DesiredPlanarVelocity();
            const bool jump = m_State.JumpQueued;
            for (const auto entity : m_Controllers)
            {
                if (!m_Scene.Contains(entity) || !m_Scene.IsActiveInHierarchy(entity))
                    continue;
                (void)m_Motor.Step(entity, desiredVelocity, jump, fixedDeltaSeconds);
            }
            if (!m_Controllers.empty()) m_State.JumpQueued = false;
        }

    private:
        kairo::engine::Scene& m_Scene;
        RuntimeCharacterMotorBridge& m_Motor;
        RuntimePlayerControllerSettings m_Settings;
        std::vector<kairo::engine::Entity> m_Controllers;
        RuntimePlayerControllerState m_State;
    };
}
