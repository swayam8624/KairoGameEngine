module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

export module Kairo.Player.RuntimeCrowdNavigationBridge;

import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.Spatial.CrowdAvoidance;
import Kairo.Player.RuntimeNavigationAgentBridge;

export namespace kairo::player
{
    struct RuntimeCrowdAgentSettings final
    {
        float Radius = 0.35f;
        float NeighborDistance = 6.0f;
        float TimeHorizon = 2.5f;
        float MaximumAcceleration = 12.0f;
        float StuckSpeedThreshold = 0.08f;
        float StuckReplanSeconds = 1.25f;

        void Validate() const
        {
            if (!std::isfinite(Radius) || Radius <= 0.0f || Radius > 10.0f)
                throw std::invalid_argument(
                    "Runtime crowd radius must be finite within (0, 10].");
            if (!std::isfinite(NeighborDistance) || NeighborDistance < Radius ||
                NeighborDistance > 1000.0f)
                throw std::invalid_argument(
                    "Runtime crowd neighbor distance must be finite and at least the radius.");
            if (!std::isfinite(TimeHorizon) || TimeHorizon <= 0.0f ||
                TimeHorizon > 60.0f)
                throw std::invalid_argument(
                    "Runtime crowd time horizon must be finite within (0, 60].");
            if (!std::isfinite(MaximumAcceleration) || MaximumAcceleration <= 0.0f ||
                MaximumAcceleration > 1000.0f)
                throw std::invalid_argument(
                    "Runtime crowd acceleration must be finite within (0, 1000].");
            if (!std::isfinite(StuckSpeedThreshold) || StuckSpeedThreshold < 0.0f ||
                StuckSpeedThreshold > 100.0f)
                throw std::invalid_argument(
                    "Runtime crowd stuck-speed threshold is invalid.");
            if (!std::isfinite(StuckReplanSeconds) || StuckReplanSeconds <= 0.0f ||
                StuckReplanSeconds > 60.0f)
                throw std::invalid_argument(
                    "Runtime crowd stuck replan interval must be finite within (0, 60].");
        }
    };

    struct RuntimeCrowdNavigationStep final
    {
        kairo::engine::Entity Entity{};
        RuntimeNavigationAgentState Navigation{};
        kairo::foundation::math::Vec3f PreferredVelocity =
            kairo::foundation::math::Vec3f::Zero();
        kairo::foundation::math::Vec3f AvoidanceVelocity =
            kairo::foundation::math::Vec3f::Zero();
        kairo::foundation::math::Vec3f AppliedVelocity =
            kairo::foundation::math::Vec3f::Zero();
        std::size_t NeighborCount = 0u;
        std::size_t ConstraintCount = 0u;
        bool ReplannedAfterStall = false;
    };

    /// Batch steering layer between NavMesh path following and the physical
    /// character motor. Every frame is solved from one immutable crowd snapshot;
    /// individual motor moves are then resolved through PhysicsEngine and their
    /// actual displacement becomes the next frame's crowd velocity.
    class RuntimeCrowdNavigationBridge final
    {
        struct AgentRecord final
        {
            kairo::engine::Entity Entity{};
            RuntimeCrowdAgentSettings Settings{};
            kairo::foundation::math::Vec3f Velocity =
                kairo::foundation::math::Vec3f::Zero();
            float StuckSeconds = 0.0f;
        };

    public:
        RuntimeCrowdNavigationBridge(kairo::engine::Scene& scene,
            RuntimeNavigationAgentBridge& navigation) noexcept
            : m_Scene(scene), m_Navigation(navigation) {}

        void Register(kairo::engine::Entity entity,
            RuntimeCrowdAgentSettings settings = {})
        {
            settings.Validate();
            if (!m_Scene.Contains(entity))
                throw std::out_of_range(
                    "Cannot register crowd state for an unknown runtime entity.");
            if (!m_Navigation.IsRegistered(entity))
                throw std::invalid_argument(
                    "Runtime crowd agent requires a registered navigation agent.");
            if (!m_Agents.emplace(entity.Value,
                    AgentRecord{ entity, settings, {}, 0.0f }).second)
                throw std::invalid_argument(
                    "Runtime crowd agent is already registered.");
        }

        bool Unregister(kairo::engine::Entity entity) noexcept
        {
            return m_Agents.erase(entity.Value) != 0u;
        }

        [[nodiscard]] bool IsRegistered(kairo::engine::Entity entity) const noexcept
        {
            return m_Agents.contains(entity.Value);
        }

        void SetObstacle(kairo::foundation::spatial::CrowdObstacle obstacle)
        {
            ValidateObstacle(obstacle);
            m_Obstacles.insert_or_assign(obstacle.ID, std::move(obstacle));
        }

        bool RemoveObstacle(kairo::foundation::spatial::CrowdObstacleID id) noexcept
        {
            return m_Obstacles.erase(id) != 0u;
        }

        [[nodiscard]] std::vector<RuntimeCrowdNavigationStep> StepAll(
            float deltaSeconds)
        {
            if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f ||
                deltaSeconds > 0.25f)
                throw std::invalid_argument(
                    "Runtime crowd delta must be finite within (0, 0.25].");

            std::vector<std::uint32_t> orderedIDs;
            orderedIDs.reserve(m_Agents.size());
            for (const auto& [id, record] : m_Agents)
            {
                (void)record;
                orderedIDs.push_back(id);
            }
            std::sort(orderedIDs.begin(), orderedIDs.end());

            struct Prepared final
            {
                AgentRecord* Record = nullptr;
                RuntimeNavigationSteeringRequest Steering{};
            };
            std::vector<Prepared> prepared;
            std::vector<kairo::foundation::spatial::CrowdAgent> crowdAgents;
            prepared.reserve(orderedIDs.size());
            crowdAgents.reserve(orderedIDs.size());

            for (const auto id : orderedIDs)
            {
                auto& record = m_Agents.at(id);
                auto steering = m_Navigation.PrepareSteering(
                    record.Entity, deltaSeconds);
                const auto position =
                    m_Scene.WorldTransform(record.Entity).Translation;
                const auto& navigationSettings =
                    m_Navigation.Settings(record.Entity);
                crowdAgents.push_back({
                    .ID = id,
                    .Position = position,
                    .Velocity = record.Velocity,
                    .PreferredVelocity = steering.WantsMovement
                        ? steering.PreferredVelocity
                        : kairo::foundation::math::Vec3f::Zero(),
                    .Radius = record.Settings.Radius,
                    .MaxSpeed = navigationSettings.MaximumSpeed,
                    .NeighborDistance = record.Settings.NeighborDistance,
                    .TimeHorizon = record.Settings.TimeHorizon
                });
                prepared.push_back({ &record, std::move(steering) });
            }

            std::vector<kairo::foundation::spatial::CrowdObstacle> obstacles;
            obstacles.reserve(m_Obstacles.size());
            for (const auto& [id, obstacle] : m_Obstacles)
            {
                (void)id;
                obstacles.push_back(obstacle);
            }
            std::sort(obstacles.begin(), obstacles.end(),
                [](const auto& lhs, const auto& rhs) { return lhs.ID < rhs.ID; });

            const auto commands =
                kairo::foundation::spatial::CrowdAvoidanceSolver::Solve(
                    crowdAgents, obstacles, { deltaSeconds });
            if (commands.size() != prepared.size())
                throw std::logic_error(
                    "Crowd solver returned a command count inconsistent with its snapshot.");

            std::vector<RuntimeCrowdNavigationStep> results;
            results.reserve(prepared.size());
            for (std::size_t index = 0u; index < prepared.size(); ++index)
            {
                auto& item = prepared[index];
                auto& record = *item.Record;
                const auto& command = commands[index];
                RuntimeCrowdNavigationStep result;
                result.Entity = record.Entity;
                result.Navigation = item.Steering.Navigation;
                result.PreferredVelocity = item.Steering.PreferredVelocity;
                result.AvoidanceVelocity = command.Velocity;
                result.NeighborCount = command.NeighborCount;
                result.ConstraintCount = command.ConstraintCount;

                if (!item.Steering.WantsMovement)
                {
                    record.Velocity = kairo::foundation::math::Vec3f::Zero();
                    record.StuckSeconds = 0.0f;
                    results.push_back(result);
                    continue;
                }

                const auto accelerated = AccelerateTowards(
                    record.Velocity,
                    command.Velocity,
                    record.Settings.MaximumAcceleration * deltaSeconds);
                const auto navigationStep = m_Navigation.ApplySteering(
                    record.Entity, accelerated, deltaSeconds);
                result.Navigation = navigationStep.Navigation;
                if (navigationStep.Motor.has_value())
                {
                    const auto& displacement =
                        navigationStep.Motor->Move.AppliedDisplacement;
                    record.Velocity = {
                        displacement.x / deltaSeconds,
                        0.0f,
                        displacement.z / deltaSeconds
                    };
                }
                else
                    record.Velocity = kairo::foundation::math::Vec3f::Zero();
                result.AppliedVelocity = record.Velocity;

                const float preferredSpeed = PlanarLength(
                    item.Steering.PreferredVelocity);
                const float appliedSpeed = PlanarLength(record.Velocity);
                if (preferredSpeed > record.Settings.StuckSpeedThreshold * 2.0f &&
                    appliedSpeed < record.Settings.StuckSpeedThreshold &&
                    result.Navigation.Status ==
                        RuntimeNavigationAgentStatus::FollowingPath)
                    record.StuckSeconds += deltaSeconds;
                else
                    record.StuckSeconds = 0.0f;

                if (record.StuckSeconds >= record.Settings.StuckReplanSeconds &&
                    m_Navigation.Intent(record.Entity).has_value())
                {
                    result.ReplannedAfterStall = true;
                    (void)m_Navigation.Replan(record.Entity);
                    result.Navigation = m_Navigation.State(record.Entity);
                    record.StuckSeconds = 0.0f;
                }
                results.push_back(std::move(result));
            }
            return results;
        }

    private:
        kairo::engine::Scene& m_Scene;
        RuntimeNavigationAgentBridge& m_Navigation;
        std::unordered_map<std::uint32_t, AgentRecord> m_Agents;
        std::unordered_map<kairo::foundation::spatial::CrowdObstacleID,
            kairo::foundation::spatial::CrowdObstacle> m_Obstacles;

        [[nodiscard]] static float PlanarLength(
            const kairo::foundation::math::Vec3f& value) noexcept
        {
            return std::sqrt(value.x * value.x + value.z * value.z);
        }

        [[nodiscard]] static kairo::foundation::math::Vec3f AccelerateTowards(
            const kairo::foundation::math::Vec3f& current,
            const kairo::foundation::math::Vec3f& target,
            float maximumDelta) noexcept
        {
            kairo::foundation::math::Vec3f delta{
                target.x - current.x, 0.0f, target.z - current.z };
            const float length = PlanarLength(delta);
            if (length <= maximumDelta || length <= 1.0e-6f)
                return { target.x, 0.0f, target.z };
            const float scale = maximumDelta / length;
            return {
                current.x + delta.x * scale,
                0.0f,
                current.z + delta.z * scale
            };
        }

        static void ValidateObstacle(
            const kairo::foundation::spatial::CrowdObstacle& obstacle)
        {
            if (obstacle.ID ==
                kairo::foundation::spatial::InvalidCrowdObstacleID)
                throw std::invalid_argument(
                    "Runtime crowd obstacle requires a valid ID.");
            if (!std::isfinite(obstacle.Position.x) ||
                !std::isfinite(obstacle.Position.y) ||
                !std::isfinite(obstacle.Position.z) ||
                !std::isfinite(obstacle.Radius) || obstacle.Radius <= 0.0f)
                throw std::invalid_argument(
                    "Runtime crowd obstacle position/radius is invalid.");
        }
    };
}
