module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

export module Kairo.Player.RuntimePhysicsBridge;

import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Foundation.PhysicsMath;

export namespace kairo::player
{
    struct RuntimePhysicsSettings final
    {
        float FixedDeltaSeconds = 1.0f / 60.0f;
        std::uint32_t MaximumSubsteps = 8u;
        float MaximumFrameDeltaSeconds = 0.25f;

        void Validate() const
        {
            if (!std::isfinite(FixedDeltaSeconds) || FixedDeltaSeconds <= 0.0f)
                throw std::invalid_argument("Runtime physics fixed delta must be finite and positive.");
            if (MaximumSubsteps == 0u || MaximumSubsteps > 64u)
                throw std::invalid_argument("Runtime physics maximum substeps must be in [1, 64].");
            if (!std::isfinite(MaximumFrameDeltaSeconds) ||
                MaximumFrameDeltaSeconds < FixedDeltaSeconds || MaximumFrameDeltaSeconds > 1.0f)
                throw std::invalid_argument(
                    "Runtime physics maximum frame delta must be finite, at least one fixed step, and at most one second.");
        }
    };

    struct RuntimePhysicsAdvance final
    {
        std::uint32_t Steps = 0u;
        float InterpolationAlpha = 0.0f;
        bool DroppedExcessTime = false;
    };

    /// Entity-facing contact event. Runtime body/collider IDs intentionally do
    /// not cross this boundary because they are valid only for one play session.
    struct RuntimeContactEvent final
    {
        kairo::engine::Entity EntityA;
        kairo::engine::Entity EntityB;
        bool IsTrigger = false;
        kairo::foundation::physics::CollisionResponse Response =
            kairo::foundation::physics::CollisionResponse::Block;
        kairo::foundation::physics::PhysicsContactEventType Type =
            kairo::foundation::physics::PhysicsContactEventType::Begin;
    };

    /// Runtime adapter from EngineCore's persistent authoring descriptors to a
    /// process-local KairoPhysicsEngine world.
    ///
    /// Coordinate convention: both systems use right-handed KairoMath world
    /// transforms, +Y up, radians, and SI-style seconds. Collider dimensions
    /// are authored in local space and multiplied by absolute world scale.
    /// Dynamic body poses are interpolated back into local scene transforms so
    /// rendering remains smooth without changing deterministic fixed stepping.
    class RuntimePhysicsBridge final
    {
    public:
        explicit RuntimePhysicsBridge(
            kairo::engine::Scene& scene, RuntimePhysicsSettings settings = {})
            : m_Scene(scene), m_Settings(settings)
        {
            m_Settings.Validate();
            BuildWorld();
            m_World.SetContactEventCallback([this](const auto& event) { CaptureContact(event); });
        }

        RuntimePhysicsBridge(const RuntimePhysicsBridge&) = delete;
        RuntimePhysicsBridge& operator=(const RuntimePhysicsBridge&) = delete;
        RuntimePhysicsBridge(RuntimePhysicsBridge&&) = delete;
        RuntimePhysicsBridge& operator=(RuntimePhysicsBridge&&) = delete;

        /// Input: non-negative elapsed wall time for one rendered frame.
        /// Output: fixed-step count, interpolation alpha, and overload signal.
        /// Task: bound debugger/window stalls, execute deterministic substeps,
        /// discard an unrecoverable backlog at the explicit substep cap, and
        /// publish interpolated body poses into the runtime scene.
        [[nodiscard]] RuntimePhysicsAdvance Advance(float elapsedSeconds)
        {
            if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0f)
                throw std::invalid_argument("Runtime physics elapsed time must be finite and non-negative.");
            m_Events.clear();
            m_Accumulator += std::min(elapsedSeconds, m_Settings.MaximumFrameDeltaSeconds);
            RuntimePhysicsAdvance result;
            while (m_Accumulator >= m_Settings.FixedDeltaSeconds &&
                result.Steps < m_Settings.MaximumSubsteps)
            {
                for (auto& [entity, pose] : m_Poses) pose.Previous = pose.Current;
                m_World.Step(m_Settings.FixedDeltaSeconds);
                SynchronizeCurrentPoses();
                m_Accumulator -= m_Settings.FixedDeltaSeconds;
                ++result.Steps;
            }
            if (m_Accumulator >= m_Settings.FixedDeltaSeconds)
            {
                m_Accumulator = std::fmod(m_Accumulator, m_Settings.FixedDeltaSeconds);
                result.DroppedExcessTime = true;
            }
            result.InterpolationAlpha = std::clamp(
                m_Accumulator / m_Settings.FixedDeltaSeconds, 0.0f, 1.0f);
            PublishInterpolatedPoses(result.InterpolationAlpha);
            return result;
        }

        [[nodiscard]] const RuntimePhysicsSettings& Settings() const noexcept { return m_Settings; }
        [[nodiscard]] const kairo::foundation::physics::PhysicsWorld& World() const noexcept { return m_World; }
        [[nodiscard]] kairo::foundation::physics::PhysicsWorld& World() noexcept { return m_World; }
        [[nodiscard]] const std::vector<RuntimeContactEvent>& ContactEvents() const noexcept { return m_Events; }

        [[nodiscard]] std::optional<kairo::foundation::physics::BodyID> BodyFor(
            kairo::engine::Entity entity) const noexcept
        {
            const auto found = m_BodiesByEntity.find(entity.Value);
            return found == m_BodiesByEntity.end() ? std::nullopt : std::optional(found->second);
        }

        [[nodiscard]] std::optional<kairo::engine::Entity> EntityFor(
            kairo::foundation::physics::BodyID body) const noexcept
        {
            const auto found = m_EntitiesByBody.find(body);
            return found == m_EntitiesByBody.end() ? std::nullopt : std::optional(found->second);
        }

        /// Entity-aware nearest ray query for gameplay, sensors, and future
        /// visual-logic nodes. Layer masks and ignored colliders retain the
        /// exact KairoPhysicsEngine semantics.
        [[nodiscard]] std::optional<std::pair<kairo::engine::Entity,
            kairo::foundation::physics::PhysicsRayHit>> Raycast(
            const kairo::foundation::math::Vec3f& origin,
            const kairo::foundation::math::Vec3f& direction,
            float maximumDistance = std::numeric_limits<float>::infinity(),
            std::uint32_t layerMask = 0xFFFF'FFFFu) const
        {
            const auto hit = m_World.Raycast(origin, direction, maximumDistance, layerMask);
            if (!hit.has_value()) return std::nullopt;
            const auto entity = EntityFor(hit->Body);
            if (!entity.has_value())
                throw std::logic_error("Physics ray hit a body without a runtime entity mapping.");
            return std::pair{ *entity, *hit };
        }

    private:
        struct PoseHistory final
        {
            kairo::foundation::math::Transformf Previous;
            kairo::foundation::math::Transformf Current;
        };

        kairo::engine::Scene& m_Scene;
        RuntimePhysicsSettings m_Settings;
        kairo::foundation::physics::PhysicsWorld m_World;
        std::unordered_map<std::uint32_t, kairo::foundation::physics::BodyID> m_BodiesByEntity;
        std::unordered_map<kairo::foundation::physics::BodyID, kairo::engine::Entity> m_EntitiesByBody;
        std::unordered_map<std::uint32_t, PoseHistory> m_Poses;
        std::vector<RuntimeContactEvent> m_Events;
        float m_Accumulator = 0.0f;

        void BuildWorld()
        {
            for (const auto entity : m_Scene.Entities())
            {
                if (!m_Scene.IsActiveInHierarchy(entity)) continue;
                const bool hasBody = m_Scene.HasRigidBody(entity);
                const bool hasCollider = m_Scene.HasCollider(entity);
                if (!hasBody && !hasCollider) continue;
                if (hasBody && !hasCollider)
                    throw std::invalid_argument("A runtime rigid body requires an authored collider.");

                const auto worldTransform = m_Scene.WorldTransform(entity);
                const auto collider = hasCollider
                    ? m_Scene.Collider(entity) : kairo::engine::ColliderComponent{};
                const auto shape = MakeShape(collider, worldTransform.Scale);
                const auto body = hasBody ? m_Scene.RigidBody(entity) : kairo::engine::RigidBodyComponent{
                    .Motion = kairo::engine::RigidBodyMotion::Static };

                kairo::foundation::physics::RigidBodyDesc descriptor;
                descriptor.Type = ToRuntimeMotion(body.Motion);
                descriptor.State.Position = worldTransform.Translation;
                descriptor.State.Rotation = worldTransform.Rotation;
                descriptor.GravityScale = body.GravityScale;
                descriptor.LinearDamping = body.LinearDamping;
                descriptor.AngularDamping = body.AngularDamping;
                if (descriptor.Type == kairo::foundation::physics::BodyType::Dynamic)
                    descriptor.Mass = MakeMass(shape, body.Density);

                const auto bodyID = m_World.CreateRigidBody(descriptor);
                const kairo::foundation::physics::PhysicsMaterial material{
                    collider.Restitution, collider.Friction, collider.Friction };
                const auto colliderID = m_World.AddCollider(bodyID, shape, material);
                m_World.SetCollisionFilter(colliderID, collider.BelongsTo, collider.CollidesWith);
                m_World.SetColliderTrigger(colliderID, collider.IsTrigger);
                m_BodiesByEntity.emplace(entity.Value, bodyID);
                m_EntitiesByBody.emplace(bodyID, entity);
                m_Poses.emplace(entity.Value, PoseHistory{ worldTransform, worldTransform });
            }
        }

        void SynchronizeCurrentPoses()
        {
            for (auto& [entityValue, pose] : m_Poses)
            {
                const auto body = m_BodiesByEntity.at(entityValue);
                const auto& state = m_World.Bodies().at(body).State;
                pose.Current.Translation = state.Position;
                pose.Current.Rotation = state.Rotation;
            }
        }

        void PublishInterpolatedPoses(float alpha)
        {
            // Parent-first order is required because ToLocal reads the current
            // parent world transform. Scene entity order is stable but not a
            // hierarchy traversal, so recurse from roots through sorted children.
            for (const auto root : m_Scene.RootEntities()) PublishSubtree(root, alpha);
        }

        void PublishSubtree(kairo::engine::Entity entity, float alpha)
        {
            const auto found = m_Poses.find(entity.Value);
            if (found != m_Poses.end())
            {
                const auto world = kairo::foundation::math::Interpolate(
                    found->second.Previous, found->second.Current, alpha);
                m_Scene.Transform(entity).Local = ToLocal(m_Scene, entity, world);
            }
            for (const auto child : m_Scene.Children(entity)) PublishSubtree(child, alpha);
        }

        void CaptureContact(const kairo::foundation::physics::PhysicsContactEvent& event)
        {
            const auto entityA = EntityFor(event.BodyA);
            const auto entityB = EntityFor(event.BodyB);
            if (!entityA.has_value() || !entityB.has_value())
                throw std::logic_error("Physics contact references an unmapped runtime body.");
            m_Events.push_back({ *entityA, *entityB, event.IsTrigger, event.Response, event.Type });
        }

        [[nodiscard]] static kairo::foundation::physics::BodyType ToRuntimeMotion(
            kairo::engine::RigidBodyMotion motion)
        {
            switch (motion)
            {
                case kairo::engine::RigidBodyMotion::Static: return kairo::foundation::physics::BodyType::Static;
                case kairo::engine::RigidBodyMotion::Dynamic: return kairo::foundation::physics::BodyType::Dynamic;
                case kairo::engine::RigidBodyMotion::Kinematic: return kairo::foundation::physics::BodyType::Kinematic;
            }
            throw std::invalid_argument("Authored rigid body motion enum is invalid.");
        }

        [[nodiscard]] static kairo::foundation::physics::ColliderShape MakeShape(
            const kairo::engine::ColliderComponent& collider,
            const kairo::foundation::math::Vec3f& scale)
        {
            const kairo::foundation::math::Vec3f absolute{
                std::abs(scale.x), std::abs(scale.y), std::abs(scale.z) };
            switch (collider.Shape)
            {
                case kairo::engine::ColliderShape::Box:
                    return kairo::foundation::physics::BoxCollider{ collider.HalfExtents * absolute };
                case kairo::engine::ColliderShape::Sphere:
                    return kairo::foundation::physics::SphereCollider{
                        collider.Radius * std::max({ absolute.x, absolute.y, absolute.z }) };
                case kairo::engine::ColliderShape::Capsule:
                    return kairo::foundation::physics::CapsuleCollider{
                        collider.Radius * std::max(absolute.x, absolute.z), collider.HalfHeight * absolute.y };
            }
            throw std::invalid_argument("Authored collider shape enum is invalid.");
        }

        [[nodiscard]] static kairo::foundation::physics::MassProperties MakeMass(
            const kairo::foundation::physics::ColliderShape& shape, float density)
        {
            return std::visit([density](const auto& value) -> kairo::foundation::physics::MassProperties
            {
                using Shape = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Shape, kairo::foundation::physics::BoxCollider>)
                    return kairo::foundation::physics::BoxMassProperties(value.HalfExtents, density);
                else if constexpr (std::is_same_v<Shape, kairo::foundation::physics::SphereCollider>)
                    return kairo::foundation::physics::SphereMassProperties(value.Radius, density);
                else if constexpr (std::is_same_v<Shape, kairo::foundation::physics::CapsuleCollider>)
                    return kairo::foundation::physics::CapsuleMassProperties(
                        value.Radius, value.HalfHeight * 2.0f, density);
                else
                    throw std::invalid_argument("Dynamic runtime bodies require a finite primitive collider.");
            }, shape);
        }

        [[nodiscard]] static kairo::foundation::math::Transformf ToLocal(
            const kairo::engine::Scene& scene, kairo::engine::Entity entity,
            const kairo::foundation::math::Transformf& world)
        {
            const auto parent = scene.Parent(entity);
            if (!parent.has_value()) return world;
            const auto parentWorld = scene.WorldTransform(*parent);
            constexpr float epsilon = std::numeric_limits<float>::epsilon() * 10.0f;
            if (std::abs(parentWorld.Scale.x) <= epsilon ||
                std::abs(parentWorld.Scale.y) <= epsilon ||
                std::abs(parentWorld.Scale.z) <= epsilon)
                throw std::invalid_argument("Cannot publish physics below a zero-scale parent.");
            auto local = world;
            local.Translation = kairo::foundation::math::WorldToLocal(parentWorld, world.Translation);
            local.Rotation = (kairo::foundation::math::Inverse(parentWorld.Rotation) *
                world.Rotation).Normalized();
            local.Scale = { world.Scale.x / parentWorld.Scale.x,
                world.Scale.y / parentWorld.Scale.y, world.Scale.z / parentWorld.Scale.z };
            return local;
        }
    };
}
