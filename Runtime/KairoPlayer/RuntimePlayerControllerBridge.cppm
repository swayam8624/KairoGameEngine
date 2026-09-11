module;

#include <algorithm>
#include <cmath>
#include <cstddef>
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

    struct RuntimePlayerControllerSettings final
    {
        float MaximumSpeed = 5.5f;
        bool InvertMoveYForWorldZ = true;
        RuntimeCharacterMotorSettings Motor{};

        void Validate() const
        {
            if (!std::isfinite(MaximumSpeed) || MaximumSpeed <= 0.0f)
                throw std::invalid_argument(
                    "Runtime player-controller speed must be finite and positive.");
            Motor.Validate();
        }
    };

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

        [[nodiscard]] std::size_t ControllerCount() const noexcept
        {
            return m_Controllers.size();
        }

        [[nodiscard]] bool Controls(kairo::engine::Entity entity) const noexcept
        {
            return std::ranges::find(m_Controllers, entity) != m_Controllers.end();
        }

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
            if (lengthSquared > 1.0f)
            {
                const float inverseLength = 1.0f / std::sqrt(lengthSquared);
                m_State.Move.X *= inverseLength;
                m_State.Move.Y *= inverseLength;
            }
            if (jump.Pressed) m_State.JumpQueued = true;
        }

        void ClearInput() noexcept
        {
            m_State = {};
        }

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
