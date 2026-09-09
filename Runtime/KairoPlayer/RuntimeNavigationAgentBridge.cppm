module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

export module Kairo.Player.RuntimeNavigationAgentBridge;

import Kairo.AI.Gameplay;
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.Spatial.NavMesh;
import Kairo.Player.RuntimeCharacterMotorBridge;

export namespace kairo::player
{
    enum class RuntimeNavigationAgentStatus : std::uint8_t
    {
        Idle,
        FollowingPath,
        Arrived,
        PathUnavailable
    };

    struct RuntimeNavigationAgentSettings final
    {
        float MaximumSpeed = 3.5f;
        float WaypointRadius = 0.20f;
        float PathSnapDistance = 1.5f;
        float VerticalTolerance = 2.0f;

        void Validate() const
        {
            if (!std::isfinite(MaximumSpeed) || MaximumSpeed <= 0.0f ||
                MaximumSpeed > 100.0f)
                throw std::invalid_argument(
                    "Runtime navigation maximum speed must be finite within (0, 100].");
            if (!std::isfinite(WaypointRadius) || WaypointRadius < 0.0f ||
                WaypointRadius > 10.0f)
                throw std::invalid_argument(
                    "Runtime navigation waypoint radius must be finite within [0, 10].");
            if (!std::isfinite(PathSnapDistance) || PathSnapDistance < 0.0f ||
                PathSnapDistance > 1000.0f)
                throw std::invalid_argument(
                    "Runtime navigation snap distance must be finite within [0, 1000].");
            if (!std::isfinite(VerticalTolerance) || VerticalTolerance < 0.0f ||
                VerticalTolerance > 1000.0f)
                throw std::invalid_argument(
                    "Runtime navigation vertical tolerance must be finite within [0, 1000].");
        }
    };

    struct RuntimeNavigationAgentState final
    {
        RuntimeNavigationAgentStatus Status = RuntimeNavigationAgentStatus::Idle;
        std::size_t WaypointIndex = 0u;
        std::size_t WaypointCount = 0u;
        float RemainingPlanarDistance = 0.0f;
    };

    struct RuntimeNavigationSteeringRequest final
    {
        RuntimeNavigationAgentState Navigation;
        kairo::foundation::math::Vec3f PreferredVelocity =
            kairo::foundation::math::Vec3f::Zero();
        bool WantsMovement = false;
    };

    struct RuntimeNavigationAgentStep final
    {
        RuntimeNavigationAgentState Navigation;
        std::optional<RuntimeCharacterMotorStep> Motor;
    };

    /// Runtime composition boundary between cognition, navigation and movement.
    /// KairoAI owns intent, KairoSpatial owns pathfinding, PhysicsEngine owns
    /// collision resolution through RuntimeCharacterMotorBridge, and Scene stays
    /// the authoritative gameplay transform source.
    ///
    /// Path preparation and physical resolution are deliberately separate. This
    /// allows deterministic crowd avoidance, gameplay steering, networking
    /// prediction and animation-facing locomotion layers to modify the preferred
    /// planar velocity without duplicating path-following state.
    class RuntimeNavigationAgentBridge final
    {
        struct AgentRecord final
        {
            RuntimeNavigationAgentSettings Settings;
            std::optional<kairo::ai::gameplay::NavigationIntent> Intent;
            kairo::foundation::spatial::NavMeshPath Path;
            RuntimeNavigationAgentState State;
        };

    public:
        RuntimeNavigationAgentBridge(kairo::engine::Scene& scene,
            RuntimeCharacterMotorBridge& motor,
            const kairo::foundation::spatial::NavMesh& navMesh) noexcept
            : m_Scene(scene), m_Motor(motor), m_NavMesh(navMesh) {}

        void Register(kairo::engine::Entity entity,
            RuntimeNavigationAgentSettings settings = {})
        {
            settings.Validate();
            if (!m_Scene.Contains(entity))
                throw std::out_of_range(
                    "Cannot register navigation for an unknown runtime entity.");
            if (!m_Motor.IsRegistered(entity))
                throw std::invalid_argument(
                    "Runtime navigation agent requires a registered character motor.");
            if (!m_Agents.emplace(entity.Value,
                    AgentRecord{ settings, std::nullopt, {}, {} }).second)
                throw std::invalid_argument(
                    "Runtime navigation agent is already registered.");
        }

        bool Unregister(kairo::engine::Entity entity) noexcept
        {
            return m_Agents.erase(entity.Value) != 0u;
        }

        [[nodiscard]] bool IsRegistered(kairo::engine::Entity entity) const noexcept
        {
            return m_Agents.contains(entity.Value);
        }

        [[nodiscard]] const RuntimeNavigationAgentSettings& Settings(
            kairo::engine::Entity entity) const
        {
            return Require(entity).Settings;
        }

        [[nodiscard]] const RuntimeNavigationAgentState& State(
            kairo::engine::Entity entity) const
        {
            return Require(entity).State;
        }

        [[nodiscard]] const kairo::foundation::spatial::NavMeshPath& Path(
            kairo::engine::Entity entity) const
        {
            return Require(entity).Path;
        }

        [[nodiscard]] std::optional<kairo::ai::gameplay::NavigationIntent> Intent(
            kairo::engine::Entity entity) const
        {
            return Require(entity).Intent;
        }

        /// Replans immediately from the entity's current world transform. A false
        /// return is a normal gameplay result: cognition may select another goal.
        /// `AllowPartialPath` is preserved on the intent but a path is reported
        /// available only when KairoSpatial can currently produce a valid corridor;
        /// future tiled/streaming nav can enrich this with true partial corridors.
        [[nodiscard]] bool SetIntent(kairo::engine::Entity entity,
            kairo::ai::gameplay::NavigationIntent intent)
        {
            intent.Validate();
            auto& record = Require(entity);
            record.Intent = intent;
            record.Path = BuildPath(entity, intent);
            record.State = {};
            if (!record.Path.Reached || record.Path.Waypoints.empty())
            {
                record.State.Status = RuntimeNavigationAgentStatus::PathUnavailable;
                return false;
            }
            record.State.Status = RuntimeNavigationAgentStatus::FollowingPath;
            record.State.WaypointCount = record.Path.Waypoints.size();
            record.State.WaypointIndex = FirstUsefulWaypoint(entity, record.Path,
                record.Settings.WaypointRadius);
            UpdateRemainingDistance(entity, record);
            ResolveArrival(entity, record);
            return record.State.Status !=
                RuntimeNavigationAgentStatus::PathUnavailable;
        }

        void ClearIntent(kairo::engine::Entity entity)
        {
            auto& record = Require(entity);
            record.Intent.reset();
            record.Path = {};
            record.State = {};
        }

        /// Rebuilds the current path after streaming/navmesh changes while keeping
        /// the cognition-owned destination. Returns false when no route exists.
        [[nodiscard]] bool Replan(kairo::engine::Entity entity)
        {
            auto& record = Require(entity);
            if (!record.Intent.has_value())
                throw std::logic_error(
                    "Runtime navigation agent has no intent to replan.");
            const auto intent = *record.Intent;
            return SetIntent(entity, intent);
        }

        /// Advances path bookkeeping but does not move the character. The returned
        /// preferred velocity is the path follower's unconstrained steering request.
        [[nodiscard]] RuntimeNavigationSteeringRequest PrepareSteering(
            kairo::engine::Entity entity, float deltaSeconds)
        {
            ValidateDelta(deltaSeconds);
            auto& record = Require(entity);
            if (!record.Intent.has_value() ||
                record.State.Status == RuntimeNavigationAgentStatus::Idle ||
                record.State.Status == RuntimeNavigationAgentStatus::PathUnavailable ||
                record.State.Status == RuntimeNavigationAgentStatus::Arrived)
                return { record.State, {}, false };

            AdvanceReachedWaypoints(entity, record);
            if (ResolveArrival(entity, record))
                return { record.State, {}, false };

            if (record.State.WaypointIndex >= record.Path.Waypoints.size())
            {
                record.State.Status = RuntimeNavigationAgentStatus::PathUnavailable;
                return { record.State, {}, false };
            }

            const auto current = m_Scene.WorldTransform(entity).Translation;
            const auto waypoint = record.Path.Waypoints[record.State.WaypointIndex];
            kairo::foundation::math::Vec3f direction{
                waypoint.x - current.x, 0.0f, waypoint.z - current.z };
            const float distance = std::sqrt(direction.x * direction.x +
                direction.z * direction.z);
            if (distance > 1.0e-6f)
                direction *= record.Settings.MaximumSpeed / distance;
            else
                direction = kairo::foundation::math::Vec3f::Zero();
            return { record.State, direction, true };
        }

        /// Applies an externally resolved steering velocity through the real
        /// character motor, then advances path/arrival bookkeeping from the
        /// resulting physical Scene transform. Speed is deterministically clamped
        /// to the navigation agent's authored maximum.
        [[nodiscard]] RuntimeNavigationAgentStep ApplySteering(
            kairo::engine::Entity entity,
            const kairo::foundation::math::Vec3f& planarVelocity,
            float deltaSeconds)
        {
            ValidateDelta(deltaSeconds);
            if (!std::isfinite(planarVelocity.x) ||
                !std::isfinite(planarVelocity.y) ||
                !std::isfinite(planarVelocity.z) ||
                std::abs(planarVelocity.y) > 1.0e-5f)
                throw std::invalid_argument(
                    "Runtime navigation steering velocity must be finite and planar.");

            auto& record = Require(entity);
            if (record.State.Status != RuntimeNavigationAgentStatus::FollowingPath ||
                !record.Intent.has_value())
                return { record.State, std::nullopt };

            auto velocity = planarVelocity;
            const float speedSquared = velocity.x * velocity.x + velocity.z * velocity.z;
            const float maximum = record.Settings.MaximumSpeed;
            if (speedSquared > maximum * maximum)
            {
                const float scale = maximum / std::sqrt(speedSquared);
                velocity.x *= scale;
                velocity.z *= scale;
            }

            auto motorStep = m_Motor.Step(entity, velocity, false, deltaSeconds);
            AdvanceReachedWaypoints(entity, record);
            UpdateRemainingDistance(entity, record);
            ResolveArrival(entity, record);
            return { record.State, std::move(motorStep) };
        }

        [[nodiscard]] RuntimeNavigationAgentStep Step(
            kairo::engine::Entity entity, float deltaSeconds)
        {
            const auto steering = PrepareSteering(entity, deltaSeconds);
            if (!steering.WantsMovement)
                return { steering.Navigation, std::nullopt };
            return ApplySteering(entity, steering.PreferredVelocity, deltaSeconds);
        }

    private:
        kairo::engine::Scene& m_Scene;
        RuntimeCharacterMotorBridge& m_Motor;
        const kairo::foundation::spatial::NavMesh& m_NavMesh;
        std::unordered_map<std::uint32_t, AgentRecord> m_Agents;

        static void ValidateDelta(float deltaSeconds)
        {
            if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f ||
                deltaSeconds > 0.25f)
                throw std::invalid_argument(
                    "Runtime navigation delta must be finite within (0, 0.25].");
        }

        [[nodiscard]] AgentRecord& Require(kairo::engine::Entity entity)
        {
            const auto found = m_Agents.find(entity.Value);
            if (found == m_Agents.end())
                throw std::out_of_range(
                    "Runtime navigation agent is not registered.");
            return found->second;
        }

        [[nodiscard]] const AgentRecord& Require(
            kairo::engine::Entity entity) const
        {
            const auto found = m_Agents.find(entity.Value);
            if (found == m_Agents.end())
                throw std::out_of_range(
                    "Runtime navigation agent is not registered.");
            return found->second;
        }

        [[nodiscard]] kairo::foundation::spatial::NavMeshPath BuildPath(
            kairo::engine::Entity entity,
            const kairo::ai::gameplay::NavigationIntent& intent) const
        {
            const auto start = m_Scene.WorldTransform(entity).Translation;
            const kairo::foundation::math::Vec3f destination{
                static_cast<float>(intent.Destination.X),
                static_cast<float>(intent.Destination.Y),
                static_cast<float>(intent.Destination.Z) };
            const auto& settings = Require(entity).Settings;
            return m_NavMesh.FindPath(start, destination,
                settings.PathSnapDistance, settings.VerticalTolerance);
        }

        [[nodiscard]] std::size_t FirstUsefulWaypoint(
            kairo::engine::Entity entity,
            const kairo::foundation::spatial::NavMeshPath& path,
            float radius) const
        {
            const auto position = m_Scene.WorldTransform(entity).Translation;
            const float radiusSquared = radius * radius;
            std::size_t index = 0u;
            while (index + 1u < path.Waypoints.size())
            {
                const auto delta = path.Waypoints[index] - position;
                const float planar = delta.x * delta.x + delta.z * delta.z;
                if (planar > radiusSquared) break;
                ++index;
            }
            return index;
        }

        void AdvanceReachedWaypoints(kairo::engine::Entity entity,
            AgentRecord& record) const
        {
            const auto position = m_Scene.WorldTransform(entity).Translation;
            const float radiusSquared = record.Settings.WaypointRadius *
                record.Settings.WaypointRadius;
            while (record.State.WaypointIndex + 1u < record.Path.Waypoints.size())
            {
                const auto delta =
                    record.Path.Waypoints[record.State.WaypointIndex] - position;
                const float planar = delta.x * delta.x + delta.z * delta.z;
                if (planar > radiusSquared) break;
                ++record.State.WaypointIndex;
            }
        }

        [[nodiscard]] bool ResolveArrival(kairo::engine::Entity entity,
            AgentRecord& record) const
        {
            if (!record.Intent.has_value()) return false;
            const auto position = m_Scene.WorldTransform(entity).Translation;
            const double dx = record.Intent->Destination.X - position.x;
            const double dz = record.Intent->Destination.Z - position.z;
            const double distanceSquared = dx * dx + dz * dz;
            const double radius = record.Intent->AcceptanceRadius;
            if (distanceSquared > radius * radius) return false;
            record.State.Status = RuntimeNavigationAgentStatus::Arrived;
            record.State.RemainingPlanarDistance = 0.0f;
            record.State.WaypointIndex = record.Path.Waypoints.size();
            return true;
        }

        void UpdateRemainingDistance(kairo::engine::Entity entity,
            AgentRecord& record) const
        {
            if (record.Path.Waypoints.empty() ||
                record.State.WaypointIndex >= record.Path.Waypoints.size())
            {
                record.State.RemainingPlanarDistance = 0.0f;
                return;
            }
            auto previous = m_Scene.WorldTransform(entity).Translation;
            float total = 0.0f;
            for (std::size_t index = record.State.WaypointIndex;
                index < record.Path.Waypoints.size(); ++index)
            {
                const auto current = record.Path.Waypoints[index];
                const float dx = current.x - previous.x;
                const float dz = current.z - previous.z;
                total += std::sqrt(dx * dx + dz * dz);
                previous = current;
            }
            record.State.RemainingPlanarDistance = total;
            record.State.WaypointCount = record.Path.Waypoints.size();
        }
    };
}
