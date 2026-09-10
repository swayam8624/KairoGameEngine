module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

export module Kairo.Player.RuntimeEntityLifecycleBridge;

import Kairo.AI.Gameplay;
import Kairo.EngineCore;
import Kairo.EngineCore.SceneComposition;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeCharacterMotorBridge;
import Kairo.Player.RuntimeNavigationAgentBridge;
import Kairo.Player.RuntimeCrowdNavigationBridge;
import Kairo.Player.RuntimeLocomotionAnimationBridge;

export namespace kairo::player
{
    struct RuntimeEntityServiceProfile final
    {
        bool CharacterMotor = false;
        RuntimeCharacterMotorSettings CharacterMotorSettings{};
        bool Navigation = false;
        RuntimeNavigationAgentSettings NavigationSettings{};
        bool Crowd = false;
        RuntimeCrowdAgentSettings CrowdSettings{};
        bool LocomotionAnimation = false;
        RuntimeLocomotionAnimationSettings LocomotionAnimationSettings{};
        std::optional<kairo::ai::gameplay::NavigationIntent> InitialNavigationIntent;

        void Validate() const
        {
            if (Navigation && !CharacterMotor)
                throw std::invalid_argument(
                    "Runtime navigation service requires the character-motor service.");
            if (Crowd && !Navigation)
                throw std::invalid_argument(
                    "Runtime crowd service requires the navigation service.");
            if (LocomotionAnimation && !Crowd)
                throw std::invalid_argument(
                    "Runtime locomotion-animation service requires the crowd service.");
            if (InitialNavigationIntent.has_value() && !Navigation)
                throw std::invalid_argument(
                    "An initial navigation intent requires the navigation service.");

            if (CharacterMotor) CharacterMotorSettings.Validate();
            if (Navigation) NavigationSettings.Validate();
            if (Crowd) CrowdSettings.Validate();
            if (LocomotionAnimation) LocomotionAnimationSettings.Validate();
            if (InitialNavigationIntent.has_value())
                InitialNavigationIntent->Validate();
        }
    };

    struct RuntimeEntityActivation final
    {
        kairo::engine::Entity Entity{};
        bool ActivatedPhysics = false;
        bool RegisteredCharacterMotor = false;
        bool RegisteredNavigation = false;
        bool RegisteredCrowd = false;
        bool RegisteredLocomotionAnimation = false;
    };

    struct RuntimeSpawnedFragment final
    {
        std::uint64_t Token = 0u;
        kairo::engine::SceneAppendResult Ownership;

        [[nodiscard]] bool IsValid() const noexcept { return Token != 0u; }
        [[nodiscard]] std::optional<kairo::engine::Entity> Resolve(
            kairo::engine::Entity source) const noexcept
        {
            return Ownership.Resolve(source);
        }
        [[nodiscard]] std::vector<kairo::engine::Entity> Entities() const
        {
            return Ownership.DestinationEntities();
        }
    };

    struct RuntimeEntityLifecycleStep final
    {
        std::vector<RuntimeCrowdAgentStep> CrowdSteps;
        std::size_t AnimatedAgents = 0u;
    };

    inline constexpr std::uint32_t RuntimeEntityLifecycleSnapshotVersion = 1u;

    struct RuntimeEntityLifecycleSnapshotEntry final
    {
        kairo::engine::Entity Entity{};
        RuntimeEntityServiceProfile Profile{};
    };

    struct RuntimeEntityLifecycleSnapshot final
    {
        std::uint32_t Version = RuntimeEntityLifecycleSnapshotVersion;
        std::vector<RuntimeEntityLifecycleSnapshotEntry> Entities;
    };

    /// Transactional ownership boundary for dynamic runtime entities.
    ///
    /// EngineCore Scene owns persistent/authored component data. Physics, character
    /// controllers, navigation, crowd state and imported-animation playback are
    /// process-local services. This bridge makes their lifetime explicit so a game
    /// never has to remember six different registration orders for one spawned NPC.
    ///
    /// Registration order:
    ///   Scene -> Physics -> CharacterMotor -> Navigation -> Crowd -> Animation
    /// Teardown uses the exact reverse dependency order. Fragment spawning uses
    /// SceneComposition as the prefab representation, so there is no duplicate
    /// component schema or Player-only prefab format.
    class RuntimeEntityLifecycleBridge final
    {
        struct EntityRecord final
        {
            RuntimeEntityServiceProfile Profile;
            bool OwnsPhysics = false;
        };

        struct FragmentRecord final
        {
            kairo::engine::SceneAppendResult Ownership;
            std::vector<kairo::engine::Entity> Destinations;
            std::vector<kairo::engine::Entity> ManagedEntities;
        };

    public:
        RuntimeEntityLifecycleBridge(
            kairo::engine::Scene& scene,
            RuntimePhysicsBridge& physics,
            RuntimeCharacterMotorBridge& characterMotor,
            RuntimeNavigationAgentBridge& navigation,
            RuntimeCrowdNavigationBridge& crowd,
            RuntimeLocomotionAnimationBridge* locomotionAnimation = nullptr) noexcept
            : m_Scene(scene),
              m_Physics(physics),
              m_CharacterMotor(characterMotor),
              m_Navigation(navigation),
              m_Crowd(crowd),
              m_LocomotionAnimation(locomotionAnimation) {}

        [[nodiscard]] RuntimeEntityActivation ActivateEntity(
            kairo::engine::Entity entity,
            RuntimeEntityServiceProfile profile = {})
        {
            profile.Validate();
            if (!m_Scene.Contains(entity))
                throw std::out_of_range(
                    "Cannot activate runtime services for an unknown Scene entity.");
            if (m_Entities.contains(entity.Value))
                throw std::invalid_argument(
                    "Runtime entity services are already active for this entity.");
            if (profile.LocomotionAnimation && m_LocomotionAnimation == nullptr)
                throw std::logic_error(
                    "Locomotion-animation service was requested without a runtime animation bridge.");

            RuntimeEntityActivation result{ .Entity = entity };
            bool ownsPhysics = false;
            try
            {
                const bool authoredPhysics =
                    m_Scene.HasRigidBody(entity) || m_Scene.HasCollider(entity);
                if (authoredPhysics && !m_Physics.BodyFor(entity).has_value())
                {
                    const std::span<const kairo::engine::Entity> one(&entity, 1u);
                    const auto topology = m_Physics.ActivateEntities(one);
                    ownsPhysics = topology.ActivatedBodies != 0u;
                    result.ActivatedPhysics = ownsPhysics;
                }

                if (profile.CharacterMotor)
                {
                    m_CharacterMotor.Register(entity, profile.CharacterMotorSettings);
                    result.RegisteredCharacterMotor = true;
                }
                if (profile.Navigation)
                {
                    m_Navigation.Register(entity, profile.NavigationSettings);
                    result.RegisteredNavigation = true;
                }
                if (profile.Crowd)
                {
                    m_Crowd.Register(entity, profile.CrowdSettings);
                    result.RegisteredCrowd = true;
                }
                if (profile.LocomotionAnimation)
                {
                    m_LocomotionAnimation->Register(
                        entity, profile.LocomotionAnimationSettings);
                    result.RegisteredLocomotionAnimation = true;
                }
                if (profile.InitialNavigationIntent.has_value())
                    (void)m_Navigation.SetIntent(
                        entity, *profile.InitialNavigationIntent);

                m_Entities.emplace(entity.Value,
                    EntityRecord{ std::move(profile), ownsPhysics });
                return result;
            }
            catch (...)
            {
                const std::exception_ptr original = std::current_exception();
                RollbackServices(entity, result);
                if (ownsPhysics)
                {
                    try
                    {
                        const std::span<const kairo::engine::Entity> one(&entity, 1u);
                        (void)m_Physics.DeactivateEntities(one);
                    }
                    catch (...)
                    {
                        throw std::runtime_error(
                            "Runtime entity activation failed and physics rollback also failed.");
                    }
                }
                std::rethrow_exception(original);
            }
        }

        /// Deactivates runtime-only services while keeping the persistent Scene
        /// entity alive. Physics is removed only when this lifecycle bridge was the
        /// owner that activated it; startup/world-streaming physics remains owned by
        /// its original subsystem.
        bool DeactivateEntity(kairo::engine::Entity entity)
        {
            const auto found = m_Entities.find(entity.Value);
            if (found == m_Entities.end()) return false;

            if (found->second.OwnsPhysics)
            {
                const std::span<const kairo::engine::Entity> one(&entity, 1u);
                (void)m_Physics.DeactivateEntities(one);
            }
            UnregisterServicesNoexcept(entity);
            m_Entities.erase(found);
            return true;
        }

        [[nodiscard]] bool IsActive(kairo::engine::Entity entity) const noexcept
        {
            return m_Entities.contains(entity.Value);
        }

        [[nodiscard]] std::size_t ActiveEntityCount() const noexcept
        {
            return m_Entities.size();
        }

        [[nodiscard]] RuntimeEntityLifecycleSnapshot CaptureSnapshot() const
        {
            RuntimeEntityLifecycleSnapshot snapshot;
            snapshot.Entities.reserve(m_Entities.size());
            for (const auto& [entityValue, record] : m_Entities)
            {
                const kairo::engine::Entity entity{ entityValue };
                RuntimeEntityServiceProfile profile = record.Profile;
                if (profile.Navigation && m_Navigation.IsRegistered(entity))
                    profile.InitialNavigationIntent = m_Navigation.Intent(entity);
                snapshot.Entities.push_back({ entity, std::move(profile) });
            }
            std::ranges::sort(snapshot.Entities, {},
                [](const RuntimeEntityLifecycleSnapshotEntry& entry)
                {
                    return entry.Entity.Value;
                });
            return snapshot;
        }

        /// Rebuilds process-local services against a Scene/Physics topology that
        /// has already been restored by RuntimeSaveGameBridge. Existing service
        /// registrations are discarded without touching Scene or Physics; saved
        /// profiles are then reactivated in stable entity order. The current
        /// navigation intent is captured as part of each profile so an NPC resumes
        /// its destination rather than reverting to its registration-time target.
        void RestoreSnapshot(const RuntimeEntityLifecycleSnapshot& snapshot)
        {
            ValidateSnapshot(snapshot);
            const auto previous = CaptureSnapshot();
            ResetServicesForRestore();
            try
            {
                for (const auto& entry : snapshot.Entities)
                    (void)ActivateEntity(entry.Entity, entry.Profile);
            }
            catch (...)
            {
                const std::exception_ptr original = std::current_exception();
                ResetServicesForRestore();
                try
                {
                    for (const auto& entry : previous.Entities)
                        if (m_Scene.Contains(entry.Entity))
                            (void)ActivateEntity(entry.Entity, entry.Profile);
                }
                catch (...)
                {
                    throw std::runtime_error(
                        "Runtime lifecycle restore failed and previous service registrations could not be reconstructed.");
                }
                std::rethrow_exception(original);
            }
        }

        /// Save-game restore replaces Scene and Physics independently from these
        /// process-local services. This reset deliberately does not deactivate any
        /// physics bodies or destroy Scene entities. Fragment ownership tokens are
        /// session-local; the restored entities remain valid Scene entities and may
        /// be adopted into new gameplay/streaming ownership after the cold load.
        void ResetServicesForRestore() noexcept
        {
            std::vector<std::uint32_t> entities;
            entities.reserve(m_Entities.size());
            for (const auto& [entity, record] : m_Entities)
            {
                (void)record;
                entities.push_back(entity);
            }
            std::ranges::sort(entities, std::greater<>{});
            for (const auto entity : entities)
                UnregisterServicesNoexcept(kairo::engine::Entity{ entity });
            m_Entities.clear();
            m_Fragments.clear();
            m_NextFragmentToken = 1u;
        }

        /// Appends a complete Scene fragment and activates authored physics for all
        /// of its entities before registering optional gameplay services. The source
        /// entity IDs in profiles are resolved through SceneAppendResult, which is
        /// also retained as the destruction/ownership token.
        [[nodiscard]] RuntimeSpawnedFragment SpawnFragment(
            const kairo::engine::Scene& fragment,
            std::vector<std::pair<kairo::engine::Entity,
                RuntimeEntityServiceProfile>> profiles = {})
        {
            if (fragment.Size() == 0u)
                throw std::invalid_argument(
                    "Runtime fragment spawn requires at least one Scene entity.");
            ValidateSourceProfiles(fragment, profiles);
            if (m_NextFragmentToken == 0u)
                throw std::overflow_error(
                    "Runtime fragment ownership-token space is exhausted.");

            kairo::engine::SceneAppendResult appended;
            std::vector<kairo::engine::Entity> destinations;
            std::vector<kairo::engine::Entity> activatedServices;
            bool appendedToScene = false;
            bool activatedPhysics = false;
            try
            {
                appended = kairo::engine::AppendScene(m_Scene, fragment);
                appendedToScene = true;
                destinations = appended.DestinationEntities();
                (void)m_Physics.ActivateEntities(destinations);
                activatedPhysics = true;

                // Source-profile order is normalized during validation so service
                // registration is deterministic even if caller data came from a map.
                for (auto& [source, profile] : profiles)
                {
                    const auto destination = appended.Resolve(source);
                    if (!destination.has_value())
                        throw std::logic_error(
                            "Runtime fragment profile source was lost during Scene remapping.");
                    (void)ActivateEntity(*destination, std::move(profile));
                    activatedServices.push_back(*destination);
                }

                const std::uint64_t token = m_NextFragmentToken++;
                FragmentRecord record{
                    appended,
                    destinations,
                    activatedServices
                };
                if (!m_Fragments.emplace(token, std::move(record)).second)
                    throw std::logic_error(
                        "Runtime fragment ownership token collided unexpectedly.");
                return { token, std::move(appended) };
            }
            catch (...)
            {
                const std::exception_ptr original = std::current_exception();
                for (auto iterator = activatedServices.rbegin();
                    iterator != activatedServices.rend(); ++iterator)
                    (void)DeactivateEntity(*iterator);
                if (activatedPhysics)
                {
                    try { (void)m_Physics.DeactivateEntities(destinations); }
                    catch (...) {}
                }
                if (appendedToScene)
                {
                    try { (void)kairo::engine::RemoveAppendedScene(m_Scene, appended); }
                    catch (...) {}
                }
                std::rethrow_exception(original);
            }
        }

        /// Validates Scene removal against a copy before touching runtime services.
        /// Once validation succeeds, physics deactivation is transactional and every
        /// remaining teardown operation is noexcept; therefore a blocked hierarchy
        /// cannot leave an NPC half-unloaded.
        bool DestroyFragment(std::uint64_t token)
        {
            const auto found = m_Fragments.find(token);
            if (found == m_Fragments.end()) return false;

            kairo::engine::Scene candidate = m_Scene;
            (void)kairo::engine::RemoveAppendedScene(
                candidate, found->second.Ownership);

            (void)m_Physics.DeactivateEntities(found->second.Destinations);
            for (auto iterator = found->second.ManagedEntities.rbegin();
                iterator != found->second.ManagedEntities.rend(); ++iterator)
            {
                UnregisterServicesNoexcept(*iterator);
                m_Entities.erase(iterator->Value);
            }

            m_Scene = std::move(candidate);
            m_Fragments.erase(found);
            return true;
        }

        bool DestroyFragment(const RuntimeSpawnedFragment& fragment)
        {
            return fragment.IsValid() && DestroyFragment(fragment.Token);
        }

        [[nodiscard]] std::size_t SpawnedFragmentCount() const noexcept
        {
            return m_Fragments.size();
        }

        [[nodiscard]] bool SetNavigationIntent(
            kairo::engine::Entity entity,
            kairo::ai::gameplay::NavigationIntent intent)
        {
            if (!m_Entities.contains(entity.Value) ||
                !m_Navigation.IsRegistered(entity))
                throw std::invalid_argument(
                    "Runtime navigation intent requires an active lifecycle-managed navigation agent.");
            return m_Navigation.SetIntent(entity, std::move(intent));
        }

        void ClearNavigationIntent(kairo::engine::Entity entity)
        {
            if (!m_Entities.contains(entity.Value) ||
                !m_Navigation.IsRegistered(entity))
                throw std::invalid_argument(
                    "Runtime navigation clear requires an active lifecycle-managed navigation agent.");
            m_Navigation.ClearIntent(entity);
        }

        /// Executes the canonical crowd batch once and then binds physically
        /// realized motion to animation for every lifecycle-managed animated agent.
        /// This is the first single-call NPC frame boundary in KairoPlayer.
        [[nodiscard]] RuntimeEntityLifecycleStep StepAgents(float deltaSeconds)
        {
            RuntimeEntityLifecycleStep result;
            result.CrowdSteps = m_Crowd.StepAll(deltaSeconds);
            if (m_LocomotionAnimation == nullptr) return result;

            for (const auto& step : result.CrowdSteps)
            {
                if (!m_LocomotionAnimation->IsRegistered(step.Entity)) continue;
                m_LocomotionAnimation->Apply(step);
                ++result.AnimatedAgents;
            }
            return result;
        }

    private:
        kairo::engine::Scene& m_Scene;
        RuntimePhysicsBridge& m_Physics;
        RuntimeCharacterMotorBridge& m_CharacterMotor;
        RuntimeNavigationAgentBridge& m_Navigation;
        RuntimeCrowdNavigationBridge& m_Crowd;
        RuntimeLocomotionAnimationBridge* m_LocomotionAnimation = nullptr;
        std::unordered_map<std::uint32_t, EntityRecord> m_Entities;
        std::unordered_map<std::uint64_t, FragmentRecord> m_Fragments;
        std::uint64_t m_NextFragmentToken = 1u;

        static void ValidateSnapshot(const RuntimeEntityLifecycleSnapshot& snapshot)
        {
            if (snapshot.Version != RuntimeEntityLifecycleSnapshotVersion)
                throw std::invalid_argument(
                    "Runtime lifecycle snapshot version is unsupported.");
            std::uint32_t previous = 0u;
            bool havePrevious = false;
            for (const auto& entry : snapshot.Entities)
            {
                if (entry.Entity.Value == 0u)
                    throw std::invalid_argument(
                        "Runtime lifecycle snapshot contains an invalid entity ID.");
                if (havePrevious && entry.Entity.Value <= previous)
                    throw std::invalid_argument(
                        "Runtime lifecycle snapshot entities must be unique and sorted by stable ID.");
                entry.Profile.Validate();
                previous = entry.Entity.Value;
                havePrevious = true;
            }
        }

        static void ValidateSourceProfiles(
            const kairo::engine::Scene& fragment,
            std::vector<std::pair<kairo::engine::Entity,
                RuntimeEntityServiceProfile>>& profiles)
        {
            std::ranges::sort(profiles,
                [](const auto& left, const auto& right)
                {
                    return left.first.Value < right.first.Value;
                });
            std::uint32_t previous = 0u;
            bool havePrevious = false;
            for (const auto& [entity, profile] : profiles)
            {
                if (!fragment.Contains(entity))
                    throw std::invalid_argument(
                        "Runtime fragment service profile references an entity outside the source fragment.");
                if (havePrevious && entity.Value == previous)
                    throw std::invalid_argument(
                        "Runtime fragment contains duplicate service profiles for one source entity.");
                profile.Validate();
                previous = entity.Value;
                havePrevious = true;
            }
        }

        void RollbackServices(
            kairo::engine::Entity entity,
            const RuntimeEntityActivation& activation) noexcept
        {
            if (activation.RegisteredLocomotionAnimation &&
                m_LocomotionAnimation != nullptr)
                (void)m_LocomotionAnimation->Unregister(entity);
            if (activation.RegisteredCrowd)
                (void)m_Crowd.Unregister(entity);
            if (activation.RegisteredNavigation)
                (void)m_Navigation.Unregister(entity);
            if (activation.RegisteredCharacterMotor)
                (void)m_CharacterMotor.Unregister(entity);
        }

        void UnregisterServicesNoexcept(kairo::engine::Entity entity) noexcept
        {
            if (m_LocomotionAnimation != nullptr &&
                m_LocomotionAnimation->IsRegistered(entity))
                (void)m_LocomotionAnimation->Unregister(entity);
            if (m_Crowd.IsRegistered(entity))
                (void)m_Crowd.Unregister(entity);
            if (m_Navigation.IsRegistered(entity))
                (void)m_Navigation.Unregister(entity);
            if (m_CharacterMotor.IsRegistered(entity))
                (void)m_CharacterMotor.Unregister(entity);
        }
    };
}
