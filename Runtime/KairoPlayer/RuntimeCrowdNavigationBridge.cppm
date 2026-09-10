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
        float StuckSpeedThreshold = 0.05f;
        float StuckTimeBeforeReplan = 1.5f;
        float ReplanCooldown = 0.5f;

        void Validate() const
        {
            if (!std::isfinite(Radius) || Radius <= 0.0f || Radius > 10.0f)
                throw std::invalid_argument(
                    "Runtime crowd radius must be finite within (0, 10].");
            if (!std::isfinite(NeighborDistance) || NeighborDistance < Radius ||
                NeighborDistance > 1000.0f)
                throw std::invalid_argument(
                    "Runtime crowd neighbor distance must be finite and at least Radius.");
            if (!std::isfinite(TimeHorizon) || TimeHorizon <= 0.0f || TimeHorizon > 60.0f)
                throw std::invalid_argument(
                    "Runtime crowd time horizon must be finite within (0, 60].");
            if (!std::isfinite(MaximumAcceleration) || MaximumAcceleration <= 0.0f ||
                MaximumAcceleration > 1000.0f)
                throw std::invalid_argument(
                    "Runtime crowd maximum acceleration must be finite within (0, 1000].");
            if (!std::isfinite(StuckSpeedThreshold) || StuckSpeedThreshold < 0.0f ||
                StuckSpeedThreshold > 100.0f)
                throw std::invalid_argument(
                    "Runtime crowd stuck speed threshold must be finite within [0, 100].");
            if (!std::isfinite(StuckTimeBeforeReplan) || StuckTimeBeforeReplan <= 0.0f ||
                StuckTimeBeforeReplan > 60.0f)
                throw std::invalid_argument(
                    "Runtime crowd stuck replan time must be finite within (0, 60].");
            if (!std::isfinite(ReplanCooldown) || ReplanCooldown < 0.0f ||
                ReplanCooldown > 60.0f)
                throw std::invalid_argument(
                    "Runtime crowd replan cooldown must be finite within [0, 60].");
        }
    };

    struct RuntimeCrowdAgentState final
    {
        kairo::foundation::math::Vec3f PreferredVelocity =
            kairo::foundation::math::Vec3f::Zero();
        kairo::foundation::math::Vec3f AvoidedVelocity =
            kairo::foundation::math::Vec3f::Zero();
        kairo::foundation::math::Vec3f ActualVelocity =
            kairo::foundation::math::Vec3f::Zero();
        std::size_t NeighborCount = 0u;
        std::size_t ConstraintCount = 0u;
        float StuckSeconds = 0.0f;
        std::uint32_t ReplanCount = 0u;
        bool ReplannedThisStep = false;
    };

    struct RuntimeCrowdAgentStep final
    {
        kairo::engine::Entity Entity{};
        RuntimeNavigationAgentStep Navigation;
        RuntimeCrowdAgentState Crowd;
    };

    /// Batch composition layer between global navigation and physical character
    /// movement. All local-avoidance commands are solved from one immutable
    /// pre-move snapshot, so movement is deterministic with respect to entity
    /// registration/update order instead of whichever NPC happens to step first.
    class RuntimeCrowdNavigationBridge final
    {
        struct AgentRecord final
        {
            RuntimeCrowdAgentSettings Settings;
            RuntimeCrowdAgentState State;
            float ReplanCooldownRemaining = 0.0f;
        };

    public:
        RuntimeCrowdNavigationBridge(kairo::engine::Scene& scene,
            RuntimeNavigationAgentBridge& navigation) noexcept
            : m_Scene(scene), m_Navigation(navigation) {}

        void Register(kairo::engine::Entity entity,
            RuntimeCrowdAgentSettings settings = {})
        {
            settings.Validate();
            if (!m_Navigation.IsRegistered(entity))
                throw std::invalid_argument(
                    "Runtime crowd agent requires a registered navigation agent.");
            if (!m_Agents.emplace(entity.Value,
                    AgentRecord{ settings, {}, 0.0f }).second)
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

        [[nodiscard]] const RuntimeCrowdAgentState& State(
            kairo::engine::Entity entity) const
        {
            return Require(entity).State;
        }

        void SetStaticObstacles(
            std::vector<kairo::foundation::spatial::CrowdObstacle> obstacles)
        {
            // The solver performs full validation and deterministic ID ordering on
            // the next StepAll. Keeping the authored list here allows moving/static
            // gameplay blockers to be replaced transactionally once per frame.
            m_Obstacles = std::move(obstacles);
        }

        [[nodiscard]] const std::vector<kairo::foundation::spatial::CrowdObstacle>&
        StaticObstacles() const noexcept
        {
            return m_Obstacles;
        }

        [[nodiscard]] std::vector<RuntimeCrowdAgentStep> StepAll(float deltaSeconds)
        {
            if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f ||
                deltaSeconds > 0.25f)
                throw std::invalid_argument(
                    "Runtime crowd delta must be finite within (0, 0.25].");

            std::vector<std::uint32_t> ordered;
            ordered.reserve(m_Agents.size());
            for (const auto& [entity, record] : m_Agents)
            {
                (void)record;
                ordered.push_back(entity);
            }
            std::sort(ordered.begin(), ordered.end());

            std::vector<kairo::foundation::spatial::CrowdAgent> solverAgents;
            solverAgents.reserve(ordered.size());
            for (const auto entityValue : ordered)
            {
                const kairo::engine::Entity entity{ entityValue };
                auto& record = Require(entity);
                record.State.ReplannedThisStep = false;
                record.ReplanCooldownRemaining = std::max(
                    0.0f, record.ReplanCooldownRemaining - deltaSeconds);

                const auto preferred =
                    m_Navigation.PreferredPlanarVelocity(entity);
                record.State.PreferredVelocity = preferred;
                const auto navState = m_Navigation.State(entity);
                if (navState.Status != RuntimeNavigationAgentStatus::FollowingPath)
                {
                    record.State.AvoidedVelocity =
                        kairo::foundation::math::Vec3f::Zero();
                    record.State.ActualVelocity =
                        kairo::foundation::math::Vec3f::Zero();
                    record.State.NeighborCount = 0u;
                    record.State.ConstraintCount = 0u;
                    record.State.StuckSeconds = 0.0f;
                    continue;
                }

                const auto& navSettings = m_Navigation.Settings(entity);
                solverAgents.push_back({
                    .ID = entityValue,
                    .Position = m_Scene.WorldTransform(entity).Translation,
                    .Velocity = record.State.ActualVelocity,
                    .PreferredVelocity = preferred,
                    .Radius = record.Settings.Radius,
                    .MaxSpeed = navSettings.MaximumSpeed,
                    .NeighborDistance = record.Settings.NeighborDistance,
                    .TimeHorizon = record.Settings.TimeHorizon
                });
            }

            const auto commands =
                kairo::foundation::spatial::CrowdAvoidanceSolver::Solve(
                    solverAgents, m_Obstacles,
                    { .TimeStep = deltaSeconds });
            std::unordered_map<std::uint32_t,
                kairo::foundation::spatial::CrowdVelocityCommand> commandByID;
            commandByID.reserve(commands.size());
            for (const auto& command : commands)
                commandByID.emplace(command.ID, command);

            std::vector<RuntimeCrowdAgentStep> results;
            results.reserve(ordered.size());
            for (const auto entityValue : ordered)
            {
                const kairo::engine::Entity entity{ entityValue };
                auto& record = Require(entity);
                const auto before = m_Scene.WorldTransform(entity).Translation;

                const auto found = commandByID.find(entityValue);
                kairo::foundation::math::Vec3f resolved =
                    kairo::foundation::math::Vec3f::Zero();
                if (found != commandByID.end())
                {
                    resolved = ClampAcceleration(record.State.ActualVelocity,
                        found->second.Velocity,
                        record.Settings.MaximumAcceleration,
                        deltaSeconds);
                    record.State.AvoidedVelocity = resolved;
                    record.State.NeighborCount = found->second.NeighborCount;
                    record.State.ConstraintCount = found->second.ConstraintCount;
                }

                RuntimeNavigationAgentStep navigationStep;
                if (m_Navigation.State(entity).Status ==
                    RuntimeNavigationAgentStatus::FollowingPath)
                {
                    navigationStep = m_Navigation.StepResolvedVelocity(
                        entity, resolved, deltaSeconds);
                }
                else
                {
                    navigationStep.Navigation = m_Navigation.State(entity);
                }

                const auto after = m_Scene.WorldTransform(entity).Translation;
                record.State.ActualVelocity = {
                    (after.x - before.x) / deltaSeconds,
                    0.0f,
                    (after.z - before.z) / deltaSeconds
                };
                UpdateStuckState(entity, record, deltaSeconds);
                results.push_back({ entity, std::move(navigationStep), record.State });
            }
            return results;
        }

    private:
        kairo::engine::Scene& m_Scene;
        RuntimeNavigationAgentBridge& m_Navigation;
        std::unordered_map<std::uint32_t, AgentRecord> m_Agents;
        std::vector<kairo::foundation::spatial::CrowdObstacle> m_Obstacles;

        [[nodiscard]] AgentRecord& Require(kairo::engine::Entity entity)
        {
            const auto found = m_Agents.find(entity.Value);
            if (found == m_Agents.end())
                throw std::out_of_range("Runtime crowd agent is not registered.");
            return found->second;
        }

        [[nodiscard]] const AgentRecord& Require(
            kairo::engine::Entity entity) const
        {
            const auto found = m_Agents.find(entity.Value);
            if (found == m_Agents.end())
                throw std::out_of_range("Runtime crowd agent is not registered.");
            return found->second;
        }

        [[nodiscard]] static kairo::foundation::math::Vec3f ClampAcceleration(
            const kairo::foundation::math::Vec3f& current,
            const kairo::foundation::math::Vec3f& target,
            float maximumAcceleration,
            float deltaSeconds) noexcept
        {
            const auto delta = target - current;
            const float planarLengthSquared =
                delta.x * delta.x + delta.z * delta.z;
            const float maximumDelta = maximumAcceleration * deltaSeconds;
            if (planarLengthSquared <= maximumDelta * maximumDelta ||
                planarLengthSquared <= 1.0e-12f)
                return { target.x, 0.0f, target.z };
            const float scale = maximumDelta / std::sqrt(planarLengthSquared);
            return {
                current.x + delta.x * scale,
                0.0f,
                current.z + delta.z * scale
            };
        }

        void UpdateStuckState(kairo::engine::Entity entity,
            AgentRecord& record, float deltaSeconds)
        {
            const auto status = m_Navigation.State(entity).Status;
            const float desiredSpeedSquared =
                record.State.PreferredVelocity.x * record.State.PreferredVelocity.x +
                record.State.PreferredVelocity.z * record.State.PreferredVelocity.z;
            const float actualSpeedSquared =
                record.State.ActualVelocity.x * record.State.ActualVelocity.x +
                record.State.ActualVelocity.z * record.State.ActualVelocity.z;
            const float stuckThresholdSquared =
                record.Settings.StuckSpeedThreshold *
                record.Settings.StuckSpeedThreshold;

            if (status != RuntimeNavigationAgentStatus::FollowingPath ||
                desiredSpeedSquared <= stuckThresholdSquared ||
                actualSpeedSquared > stuckThresholdSquared)
            {
                record.State.StuckSeconds = 0.0f;
                return;
            }

            record.State.StuckSeconds += deltaSeconds;
            if (record.State.StuckSeconds < record.Settings.StuckTimeBeforeReplan ||
                record.ReplanCooldownRemaining > 0.0f)
                return;

            (void)m_Navigation.Replan(entity);
            record.State.StuckSeconds = 0.0f;
            record.ReplanCooldownRemaining = record.Settings.ReplanCooldown;
            ++record.State.ReplanCount;
            record.State.ReplannedThisStep = true;
        }
    };
}
