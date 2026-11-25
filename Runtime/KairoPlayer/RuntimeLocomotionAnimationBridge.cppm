module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

export module Kairo.Player.RuntimeLocomotionAnimationBridge;

import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Player.RuntimeCrowdNavigationBridge;
import Kairo.Player.RuntimeRenderBridge;

export namespace kairo::player
{
    // Semantic locomotion states decouple gameplay from asset clip indices.
    enum class RuntimeLocomotionAnimationState : std::uint8_t
    {
        Idle,
        Walk,
        Run,
        Airborne
    };

    // Clip names are authored asset metadata. Speed thresholds and reference
    // speeds use world units/second; playback multipliers are dimensionless.
    struct RuntimeLocomotionAnimationSettings final
    {
        std::string IdleClip = "Idle";
        std::string WalkClip = "Walk";
        std::string RunClip = "Run";
        std::string AirborneClip = "Fall";
        float MovingSpeedThreshold = 0.10f;
        float RunSpeedThreshold = 3.0f;
        float WalkReferenceSpeed = 1.8f;
        float RunReferenceSpeed = 4.5f;
        float MinimumPlaybackRate = 0.55f;
        float MaximumPlaybackRate = 1.75f;
        bool FaceMovementDirection = true;

        // Check clip requirements, ordered thresholds/reference speeds and
        // bounded playback multipliers before registering any asset binding.
        void Validate() const
        {
            if (IdleClip.empty() || WalkClip.empty() || RunClip.empty())
                throw std::invalid_argument(
                    "Runtime locomotion animation requires idle, walk and run clip names.");
            if (!std::isfinite(MovingSpeedThreshold) || MovingSpeedThreshold < 0.0f ||
                !std::isfinite(RunSpeedThreshold) || RunSpeedThreshold <= MovingSpeedThreshold)
                throw std::invalid_argument(
                    "Runtime locomotion speed thresholds must be finite with run > moving >= 0.");
            if (!std::isfinite(WalkReferenceSpeed) || WalkReferenceSpeed <= 0.0f ||
                !std::isfinite(RunReferenceSpeed) || RunReferenceSpeed <= WalkReferenceSpeed)
                throw std::invalid_argument(
                    "Runtime locomotion reference speeds must be finite with run > walk > 0.");
            if (!std::isfinite(MinimumPlaybackRate) || MinimumPlaybackRate <= 0.0f ||
                !std::isfinite(MaximumPlaybackRate) ||
                MaximumPlaybackRate < MinimumPlaybackRate || MaximumPlaybackRate > 8.0f)
                throw std::invalid_argument(
                    "Runtime locomotion playback-rate bounds are invalid.");
        }
    };

    // Value-only result of animation policy, independent of entity/GPU handles.
    struct RuntimeLocomotionAnimationDecision final
    {
        RuntimeLocomotionAnimationState State = RuntimeLocomotionAnimationState::Idle;
        float PlanarSpeed = 0.0f;
        float PlaybackRate = 1.0f;
    };

    // Pure classifier: airborne takes priority when configured, then idle,
    // walk or run. Match animation cadence to measured speed/reference speed.
    [[nodiscard]] inline RuntimeLocomotionAnimationDecision
    SelectLocomotionAnimation(const RuntimeLocomotionAnimationSettings& settings,
        float planarSpeed, bool grounded)
    {
        settings.Validate();
        if (!std::isfinite(planarSpeed) || planarSpeed < 0.0f)
            throw std::invalid_argument(
                "Runtime locomotion planar speed must be finite and non-negative.");

        if (!grounded && !settings.AirborneClip.empty())
            return { RuntimeLocomotionAnimationState::Airborne, planarSpeed, 1.0f };
        if (planarSpeed < settings.MovingSpeedThreshold)
            return { RuntimeLocomotionAnimationState::Idle, planarSpeed, 1.0f };

        // At the run threshold select the faster clip. Clamp the multiplier
        // rather than permitting pathological fast/slow playback.
        const bool running = planarSpeed >= settings.RunSpeedThreshold;
        const float reference = running
            ? settings.RunReferenceSpeed
            : settings.WalkReferenceSpeed;
        const float rate = std::clamp(planarSpeed / reference,
            settings.MinimumPlaybackRate, settings.MaximumPlaybackRate);
        return {
            running ? RuntimeLocomotionAnimationState::Run
                    : RuntimeLocomotionAnimationState::Walk,
            planarSpeed,
            rate
        };
    }

    // Last published decision tracks whether a clip transition needs a restart.
    struct RuntimeLocomotionAnimationStateData final
    {
        RuntimeLocomotionAnimationState State = RuntimeLocomotionAnimationState::Idle;
        float PlanarSpeed = 0.0f;
        float PlaybackRate = 1.0f;
        std::uint32_t ClipIndex = 0u;
        bool Initialized = false;
    };

    /// Gameplay-facing binding from physically realized character movement to
    /// imported animation playback and visual facing. It intentionally consumes
    /// ActualVelocity, not desired path velocity, so a blocked NPC neither runs
    /// through a wall visually nor rotates toward a motion it failed to achieve.
    class RuntimeLocomotionAnimationBridge final
    {
        // Cache resolved clip indices per entity so per-frame Apply does not
        // repeatedly search names. Airborne clip is deliberately optional.
        struct Agent final
        {
            RuntimeLocomotionAnimationSettings Settings;
            std::uint32_t Idle = 0u;
            std::uint32_t Walk = 0u;
            std::uint32_t Run = 0u;
            std::optional<std::uint32_t> Airborne;
            RuntimeLocomotionAnimationStateData State;
        };

    public:
        // Borrow scene transform authority and render animation control; both
        // collaborators must outlive this bridge.
        RuntimeLocomotionAnimationBridge(kairo::engine::Scene& scene,
            RuntimeRenderBridge& render) noexcept
            : m_Scene(scene), m_Render(render) {}

        // Resolve required asset clips into a local candidate before inserting.
        // Unknown entities, missing required clips and duplicate registration fail.
        void Register(kairo::engine::Entity entity,
            RuntimeLocomotionAnimationSettings settings = {})
        {
            settings.Validate();
            if (!m_Scene.Contains(entity))
                throw std::out_of_range(
                    "Cannot register locomotion animation for an unknown entity.");
            Agent agent;
            agent.Settings = std::move(settings);
            agent.Idle = RequireClip(entity, agent.Settings.IdleClip);
            agent.Walk = RequireClip(entity, agent.Settings.WalkClip);
            agent.Run = RequireClip(entity, agent.Settings.RunClip);
            if (!agent.Settings.AirborneClip.empty())
                agent.Airborne = m_Render.FindAnimationClip(
                    entity, agent.Settings.AirborneClip);
            if (!m_Agents.emplace(entity.Value, std::move(agent)).second)
                throw std::invalid_argument(
                    "Runtime locomotion animation entity is already registered.");
        }

        // Remove cached binding and relinquish animation control to the render
        // bridge. Does not destroy scene entity or imported asset.
        bool Unregister(kairo::engine::Entity entity) noexcept
        {
            const bool removed = m_Agents.erase(entity.Value) != 0u;
            if (removed) m_Render.ClearAnimationControl(entity);
            return removed;
        }

        // Test stable entity identity, not mesh or clip ownership.
        [[nodiscard]] bool IsRegistered(kairo::engine::Entity entity) const noexcept
        {
            return m_Agents.contains(entity.Value);
        }

        // Borrow the last applied policy result for this registered entity;
        // missing registration is a configuration error.
        [[nodiscard]] const RuntimeLocomotionAnimationStateData& State(
            kairo::engine::Entity entity) const
        {
            return Require(entity).State;
        }

        // Called with the resolved crowd/navigation step, after actual motor
        // movement. X/Z magnitude excludes vertical jump/fall speed.
        void Apply(const RuntimeCrowdAgentStep& step)
        {
            auto& agent = Require(step.Entity);
            const float vx = step.Crowd.ActualVelocity.x;
            const float vz = step.Crowd.ActualVelocity.z;
            const float speed = std::sqrt(vx * vx + vz * vz);
            bool grounded = true;
            if (step.Navigation.Motor.has_value())
                grounded = step.Navigation.Motor->State.Grounded;

            auto decision = SelectLocomotionAnimation(
                agent.Settings, speed, grounded);
            if (decision.State == RuntimeLocomotionAnimationState::Airborne &&
                !agent.Airborne.has_value())
                decision.State = speed < agent.Settings.MovingSpeedThreshold
                    ? RuntimeLocomotionAnimationState::Idle
                    : speed >= agent.Settings.RunSpeedThreshold
                        ? RuntimeLocomotionAnimationState::Run
                        : RuntimeLocomotionAnimationState::Walk;

            const std::uint32_t clip = ClipFor(agent, decision.State);
            // Restart clip time only for the initial selection or a real state/
            // clip transition. Changing speed alone retains animation phase.
            const bool changed = !agent.State.Initialized ||
                agent.State.State != decision.State ||
                agent.State.ClipIndex != clip;
            m_Render.SetAnimation(step.Entity, clip,
                kairo::engine::AnimationTimeMode::Loop,
                decision.PlaybackRate, changed);

            // Face realized motion only above the moving threshold; this avoids
            // normalizing a zero vector and jittering idle orientation.
            if (agent.Settings.FaceMovementDirection &&
                speed >= agent.Settings.MovingSpeedThreshold)
                FaceResolvedMotion(step.Entity, { vx, 0.0f, vz });

            agent.State = {
                decision.State,
                decision.PlanarSpeed,
                decision.PlaybackRate,
                clip,
                true
            };
        }

    private:
        kairo::engine::Scene& m_Scene;
        RuntimeRenderBridge& m_Render;
        std::unordered_map<std::uint32_t, Agent> m_Agents;

        /// KairoMath uses -Z as local forward. LookRotation gives the desired
        /// world orientation; a parented entity must convert that world rotation
        /// back into local space so moving platforms/streamed hierarchies remain
        /// correct instead of accumulating their parent's orientation twice.
        void FaceResolvedMotion(kairo::engine::Entity entity,
            const kairo::foundation::math::Vec3f& planarVelocity)
        {
            const auto desiredWorld = kairo::foundation::math::LookRotation(
                planarVelocity, kairo::foundation::math::Vec3f::Up());
            if (const auto parent = m_Scene.Parent(entity); parent.has_value())
            {
                const auto parentWorld = m_Scene.WorldTransform(*parent);
                m_Scene.Transform(entity).Local.Rotation =
                    (kairo::foundation::math::Inverse(parentWorld.Rotation) *
                        desiredWorld).Normalized();
            }
            else
            {
                m_Scene.Transform(entity).Local.Rotation = desiredWorld;
            }
        }

        // Resolve by imported clip name through RuntimeRenderBridge. A missing
        // required clip fails registration; optional fall is handled separately.
        [[nodiscard]] std::uint32_t RequireClip(kairo::engine::Entity entity,
            const std::string& name) const
        {
            const auto clip = m_Render.FindAnimationClip(entity, name);
            if (!clip.has_value())
                throw std::invalid_argument(
                    "Required locomotion animation clip is missing: " + name);
            return *clip;
        }

        // Convert semantic state to cached asset index. The idle fallback
        // handles an unavailable airborne clip without inventing a new clip.
        [[nodiscard]] static std::uint32_t ClipFor(const Agent& agent,
            RuntimeLocomotionAnimationState state)
        {
            switch (state)
            {
                case RuntimeLocomotionAnimationState::Idle: return agent.Idle;
                case RuntimeLocomotionAnimationState::Walk: return agent.Walk;
                case RuntimeLocomotionAnimationState::Run: return agent.Run;
                case RuntimeLocomotionAnimationState::Airborne:
                    if (agent.Airborne.has_value()) return *agent.Airborne;
                    return agent.Idle;
            }
            return agent.Idle;
        }

        // Mutable lookup for Apply; fail rather than implicitly creating an
        // agent with invalid default animation indices.
        [[nodiscard]] Agent& Require(kairo::engine::Entity entity)
        {
            const auto found = m_Agents.find(entity.Value);
            if (found == m_Agents.end())
                throw std::out_of_range(
                    "Runtime locomotion animation entity is not registered.");
            return found->second;
        }

        // Const counterpart used by State; same identity and error contract.
        [[nodiscard]] const Agent& Require(kairo::engine::Entity entity) const
        {
            const auto found = m_Agents.find(entity.Value);
            if (found == m_Agents.end())
                throw std::out_of_range(
                    "Runtime locomotion animation entity is not registered.");
            return found->second;
        }
    };
}
