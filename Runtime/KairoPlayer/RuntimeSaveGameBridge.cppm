module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module Kairo.Player.RuntimeSaveGameBridge;

import Kairo.EngineCore;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Foundation.PhysicsMath.Types;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeProject;

export namespace kairo::player
{
    inline constexpr std::string_view RuntimePhysicsSaveChunkName = "kairo.physics";
    inline constexpr std::uint32_t RuntimePhysicsSaveChunkSchema =
        kairo::foundation::physics::PhysicsSnapshotFileVersion;
    inline constexpr std::string_view RuntimePhysicsBindingsSaveChunkName =
        "kairo.physics-bindings";
    inline constexpr std::uint32_t RuntimePhysicsBindingsSaveChunkSchema = 1u;

    struct RuntimePhysicsBinding final
    {
        kairo::engine::Entity Entity{};
        kairo::foundation::physics::BodyID Body =
            kairo::foundation::physics::InvalidBodyID;

        friend constexpr bool operator==(const RuntimePhysicsBinding&,
            const RuntimePhysicsBinding&) noexcept = default;
    };

    /// Player-level composition boundary for subsystem-owned save formats.
    /// EngineCore owns the KSAVE001 container, core scene snapshot, and authored
    /// audio snapshot. PhysicsEngine owns the deterministic PhysicsWorld payload.
    ///
    /// Schema-v1 physics bindings make the snapshot topology portable across a
    /// cold process. Body IDs are stable vector indices, including inactive gaps
    /// left by streaming/despawn. Persisting the exact Entity<->BodyID table means
    /// a loaded Scene can rebuild RuntimePhysicsBridge mappings before the full
    /// PhysicsWorld snapshot is restored instead of requiring the pre-load world
    /// to already contain every runtime-spawned entity.
    class RuntimeSaveGameBridge final
    {
    public:
        RuntimeSaveGameBridge(RuntimeProject& project, RuntimePhysicsBridge& physics)
            : m_Project(project), m_Physics(physics) {}

        [[nodiscard]] kairo::engine::SaveGameArchive Capture(
            std::string label = {}, std::uint64_t sequence = 0u) const
        {
            kairo::engine::SaveGameArchive archive;
            archive.ProjectName = m_Project.Descriptor().Name;
            archive.EngineVersion = m_Project.Descriptor().EngineVersion;
            archive.Label = std::move(label);
            archive.Sequence = sequence;
            archive.SetChunk(kairo::engine::MakeSceneSaveChunk(
                m_Project.Scene(), m_Project.Assets()));
            archive.SetChunk(kairo::engine::MakeAudioSceneSaveChunk(
                m_Project.Scene()));
            const auto physics = m_Physics.CaptureSnapshot();
            const auto bindings = CurrentBindings();
            ValidateSavedBindings(m_Project.Scene(), physics, bindings);
            archive.SetChunk(MakePhysicsChunk(physics));
            archive.SetChunk(MakePhysicsBindingsChunk(bindings));
            archive.Validate();
            return archive;
        }

        void Save(const std::filesystem::path& path,
            std::string label = {}, std::uint64_t sequence = 0u) const
        {
            kairo::engine::SaveGameToFile(path,
                Capture(std::move(label), sequence));
        }

        [[nodiscard]] kairo::engine::SaveGameArchive Load(
            const std::filesystem::path& path)
        {
            auto archive = kairo::engine::LoadGameFromFile(path);
            Restore(archive);
            return archive;
        }

        void Restore(const kairo::engine::SaveGameArchive& archive)
        {
            archive.Validate();
            ValidateProjectIdentity(archive);
            if (!archive.ContainsChunk(kairo::engine::SceneSaveChunkName))
                throw std::invalid_argument(
                    "Runtime save-game is missing its scene snapshot chunk.");
            if (!archive.ContainsChunk(kairo::engine::AudioSceneSaveChunkName))
                throw std::invalid_argument(
                    "Runtime save-game is missing its authored-audio snapshot chunk.");
            if (!archive.ContainsChunk(RuntimePhysicsSaveChunkName))
                throw std::invalid_argument(
                    "Runtime save-game is missing its physics snapshot chunk.");

            // Parse and semantically validate every subsystem before mutating the
            // running project. Audio is applied only to the temporary savedScene;
            // malformed audio/physics/bindings therefore cannot partially restore
            // the live Scene before validation is complete.
            kairo::engine::Scene savedScene = kairo::engine::ParseSceneSaveChunk(
                archive.Chunk(kairo::engine::SceneSaveChunkName),
                m_Project.Assets());
            kairo::engine::ApplyAudioSceneSaveChunk(
                archive.Chunk(kairo::engine::AudioSceneSaveChunkName),
                savedScene, m_Project.Assets());

            const auto physicsSnapshot = ParsePhysicsChunk(
                archive.Chunk(RuntimePhysicsSaveChunkName));
            kairo::foundation::physics::PhysicsWorld physicsValidation;
            physicsValidation.RestoreSnapshot(physicsSnapshot);

            // Legacy archives intentionally retain the old same-topology contract.
            // Every archive captured by this revision contains bindings and uses the
            // portable cold-restore path below.
            if (!archive.ContainsChunk(RuntimePhysicsBindingsSaveChunkName))
            {
                ValidateSceneTopology(m_Project.Scene(), savedScene);
                ValidatePhysicsTopology(physicsSnapshot);
                m_Project.Scene() = std::move(savedScene);
                m_Physics.RestoreSnapshot(physicsSnapshot);
                return;
            }

            const auto savedBindings = ParsePhysicsBindingsChunk(
                archive.Chunk(RuntimePhysicsBindingsSaveChunkName));
            ValidateSavedBindings(savedScene, physicsSnapshot, savedBindings);

            // Capture an exact rollback point. RestoreRuntimeTopology is itself
            // deterministic and starts by removing every currently mapped body,
            // so it can recover both from the normal running world and from a
            // partially rebuilt saved topology if an unexpected activation error
            // is encountered after pre-validation.
            const kairo::engine::Scene previousScene = m_Project.Scene();
            const auto previousPhysics = m_Physics.CaptureSnapshot();
            const auto previousBindings = CurrentBindings();

            try
            {
                RestoreRuntimeTopology(
                    std::move(savedScene), physicsSnapshot, savedBindings);
            }
            catch (...)
            {
                const std::exception_ptr original = std::current_exception();
                try
                {
                    RestoreRuntimeTopology(
                        previousScene, previousPhysics, previousBindings);
                }
                catch (...)
                {
                    throw std::runtime_error(
                        "Cold save-game restore failed and rollback could not reconstruct the previous runtime topology.");
                }
                std::rethrow_exception(original);
            }
        }

    private:
        RuntimeProject& m_Project;
        RuntimePhysicsBridge& m_Physics;

        [[nodiscard]] static kairo::engine::SaveGameChunk MakePhysicsChunk(
            const kairo::foundation::physics::PhysicsWorldSnapshot& snapshot)
        {
            const auto bytes =
                kairo::foundation::physics::SerializePhysicsWorldSnapshot(snapshot);
            std::vector<std::byte> payload;
            payload.reserve(bytes.size());
            for (const std::uint8_t value : bytes)
                payload.push_back(static_cast<std::byte>(value));
            return { std::string(RuntimePhysicsSaveChunkName),
                RuntimePhysicsSaveChunkSchema, std::move(payload) };
        }

        [[nodiscard]] static kairo::foundation::physics::PhysicsWorldSnapshot
        ParsePhysicsChunk(const kairo::engine::SaveGameChunk& chunk)
        {
            if (chunk.Name != RuntimePhysicsSaveChunkName ||
                chunk.SchemaVersion != RuntimePhysicsSaveChunkSchema)
                throw std::invalid_argument(
                    "Save-game chunk is not a supported Kairo physics snapshot.");
            std::vector<std::uint8_t> bytes;
            bytes.reserve(chunk.Payload.size());
            for (const std::byte value : chunk.Payload)
                bytes.push_back(std::to_integer<std::uint8_t>(value));
            return kairo::foundation::physics::DeserializePhysicsWorldSnapshot(bytes);
        }

        [[nodiscard]] std::vector<RuntimePhysicsBinding> CurrentBindings() const
        {
            std::vector<RuntimePhysicsBinding> result;
            result.reserve(m_Project.Scene().Size());
            for (const auto entity : m_Project.Scene().Entities())
            {
                const auto body = m_Physics.BodyFor(entity);
                if (body.has_value()) result.push_back({ entity, *body });
            }
            std::ranges::sort(result, {},
                [](const RuntimePhysicsBinding& value) { return value.Entity.Value; });
            return result;
        }

        [[nodiscard]] static kairo::engine::SaveGameChunk MakePhysicsBindingsChunk(
            std::span<const RuntimePhysicsBinding> bindings)
        {
            if (bindings.size() > std::numeric_limits<std::uint32_t>::max())
                throw std::length_error("Runtime physics binding count exceeds save format capacity.");
            std::vector<std::byte> payload;
            payload.reserve(4u + bindings.size() * 8u);
            AppendU32(payload, static_cast<std::uint32_t>(bindings.size()));
            for (const auto& binding : bindings)
            {
                AppendU32(payload, binding.Entity.Value);
                AppendU32(payload, binding.Body);
            }
            return { std::string(RuntimePhysicsBindingsSaveChunkName),
                RuntimePhysicsBindingsSaveChunkSchema, std::move(payload) };
        }

        [[nodiscard]] static std::vector<RuntimePhysicsBinding>
        ParsePhysicsBindingsChunk(const kairo::engine::SaveGameChunk& chunk)
        {
            if (chunk.Name != RuntimePhysicsBindingsSaveChunkName ||
                chunk.SchemaVersion != RuntimePhysicsBindingsSaveChunkSchema)
                throw std::invalid_argument(
                    "Save-game chunk is not a supported runtime physics binding table.");
            std::size_t offset = 0u;
            const std::uint32_t count = ReadU32(chunk.Payload, offset);
            if (count > 1'000'000u)
                throw std::length_error(
                    "Runtime physics binding table exceeds the entity safety limit.");
            const std::size_t required = 4u + static_cast<std::size_t>(count) * 8u;
            if (chunk.Payload.size() != required)
                throw std::invalid_argument(
                    "Runtime physics binding payload length does not match its record count.");

            std::vector<RuntimePhysicsBinding> result;
            result.reserve(count);
            for (std::uint32_t index = 0u; index < count; ++index)
                result.push_back({
                    kairo::engine::Entity{ ReadU32(chunk.Payload, offset) },
                    ReadU32(chunk.Payload, offset) });
            return result;
        }

        static void ValidateSavedBindings(
            const kairo::engine::Scene& scene,
            const kairo::foundation::physics::PhysicsWorldSnapshot& snapshot,
            std::span<const RuntimePhysicsBinding> bindings)
        {
            std::vector<bool> mappedBodies(snapshot.Bodies.size(), false);
            std::vector<std::uint32_t> entities;
            entities.reserve(bindings.size());
            for (const auto& binding : bindings)
            {
                if (!scene.Contains(binding.Entity))
                    throw std::invalid_argument(
                        "Runtime physics binding references an entity absent from the saved Scene.");
                if (!scene.HasCollider(binding.Entity) &&
                    !scene.HasRigidBody(binding.Entity))
                    throw std::invalid_argument(
                        "Runtime physics binding references an entity without authored physics.");
                if (binding.Body == kairo::foundation::physics::InvalidBodyID ||
                    binding.Body >= snapshot.Bodies.size() ||
                    !snapshot.Bodies[binding.Body].Active ||
                    snapshot.Bodies[binding.Body].ID != binding.Body)
                    throw std::invalid_argument(
                        "Runtime physics binding references an inactive or missing saved body.");
                if (mappedBodies[binding.Body])
                    throw std::invalid_argument(
                        "Runtime physics binding table maps one body more than once.");
                mappedBodies[binding.Body] = true;
                entities.push_back(binding.Entity.Value);
            }

            std::ranges::sort(entities);
            if (std::adjacent_find(entities.begin(), entities.end()) != entities.end())
                throw std::invalid_argument(
                    "Runtime physics binding table maps one entity more than once.");

            std::size_t activeBodies = 0u;
            for (std::size_t body = 0u; body < snapshot.Bodies.size(); ++body)
            {
                if (!snapshot.Bodies[body].Active) continue;
                ++activeBodies;
                if (!mappedBodies[body])
                    throw std::invalid_argument(
                        "Physics snapshot contains an active body with no persisted entity binding.");
            }
            if (activeBodies != bindings.size())
                throw std::invalid_argument(
                    "Physics snapshot active-body count differs from its persisted binding table.");
        }

        void RestoreRuntimeTopology(
            kairo::engine::Scene scene,
            const kairo::foundation::physics::PhysicsWorldSnapshot& snapshot,
            std::span<const RuntimePhysicsBinding> bindings)
        {
            ValidateSavedBindings(scene, snapshot, bindings);

            const auto currentBindings = CurrentBindings();
            if (!currentBindings.empty())
            {
                std::vector<kairo::engine::Entity> currentEntities;
                currentEntities.reserve(currentBindings.size());
                for (const auto& binding : currentBindings)
                    currentEntities.push_back(binding.Entity);
                (void)m_Physics.DeactivateEntities(currentEntities);
            }

            // Drop inactive storage left by the old session. Private bridge maps
            // are empty after DeactivateEntities; the authoritative snapshot will
            // be restored only after its exact active BodyID mapping is rebuilt.
            m_Physics.World() = kairo::foundation::physics::PhysicsWorld{};
            m_Project.Scene() = std::move(scene);

            std::vector<RuntimePhysicsBinding> ordered(bindings.begin(), bindings.end());
            std::ranges::sort(ordered, {},
                [](const RuntimePhysicsBinding& value) { return value.Body; });
            for (const auto& binding : ordered)
            {
                while (m_Physics.World().Bodies().size() < binding.Body)
                {
                    const auto filler = m_Physics.World().CreateRigidBody({});
                    m_Physics.World().DestroyRigidBody(filler);
                }
                const auto entity = binding.Entity;
                const std::span<const kairo::engine::Entity> one(&entity, 1u);
                const auto change = m_Physics.ActivateEntities(one);
                if (change.ActivatedBodies != 1u ||
                    m_Physics.BodyFor(entity) !=
                        std::optional<kairo::foundation::physics::BodyID>{ binding.Body })
                    throw std::logic_error(
                        "Cold restore could not reconstruct the persisted entity/body identity.");
            }

            // Existing RuntimePhysicsBridge validation now sees precisely the
            // mapping it expects. RestoreSnapshot replaces placeholder/gap storage,
            // reinstalls callbacks, resets interpolation time, and publishes the
            // saved physical poses back into Scene.
            m_Physics.RestoreSnapshot(snapshot);
        }

        static void AppendU32(std::vector<std::byte>& payload, std::uint32_t value)
        {
            payload.push_back(static_cast<std::byte>(value & 0xffu));
            payload.push_back(static_cast<std::byte>((value >> 8u) & 0xffu));
            payload.push_back(static_cast<std::byte>((value >> 16u) & 0xffu));
            payload.push_back(static_cast<std::byte>((value >> 24u) & 0xffu));
        }

        [[nodiscard]] static std::uint32_t ReadU32(
            const std::vector<std::byte>& payload, std::size_t& offset)
        {
            if (offset > payload.size() || payload.size() - offset < 4u)
                throw std::invalid_argument(
                    "Runtime physics binding payload is truncated.");
            const auto byte = [&](std::size_t index)
            {
                return static_cast<std::uint32_t>(
                    std::to_integer<std::uint8_t>(payload[offset + index]));
            };
            const std::uint32_t value = byte(0u) |
                (byte(1u) << 8u) | (byte(2u) << 16u) | (byte(3u) << 24u);
            offset += 4u;
            return value;
        }

        void ValidateProjectIdentity(
            const kairo::engine::SaveGameArchive& archive) const
        {
            if (archive.ProjectName != m_Project.Descriptor().Name)
                throw std::invalid_argument(
                    "Save-game belongs to a different Kairo project.");
            if (archive.EngineVersion != m_Project.Descriptor().EngineVersion)
                throw std::invalid_argument(
                    "Save-game engine version does not match the running project.");
        }

        static void ValidateSceneTopology(const kairo::engine::Scene& running,
            const kairo::engine::Scene& saved)
        {
            const auto currentEntities = running.Entities();
            const auto savedEntities = saved.Entities();
            if (currentEntities.size() != savedEntities.size())
                throw std::invalid_argument(
                    "Save-game scene entity topology does not match the running project.");
            for (std::size_t index = 0u; index < currentEntities.size(); ++index)
            {
                const auto current = currentEntities[index];
                const auto candidate = savedEntities[index];
                if (current != candidate)
                    throw std::invalid_argument(
                        "Save-game scene entity IDs do not match the running project.");
                if (running.HasRigidBody(current) != saved.HasRigidBody(candidate) ||
                    running.HasCollider(current) != saved.HasCollider(candidate))
                    throw std::invalid_argument(
                        "Save-game scene physics-component topology does not match the running project.");
            }
        }

        void ValidatePhysicsTopology(
            const kairo::foundation::physics::PhysicsWorldSnapshot& snapshot) const
        {
            std::size_t mappedBodies = 0u;
            for (const auto entity : m_Project.Scene().Entities())
                if (m_Physics.BodyFor(entity).has_value()) ++mappedBodies;

            std::size_t activeBodies = 0u;
            for (const auto& body : snapshot.Bodies)
            {
                if (!body.Active) continue;
                ++activeBodies;
                if (!m_Physics.EntityFor(body.ID).has_value())
                    throw std::invalid_argument(
                        "Save-game physics snapshot contains an unmapped active body.");
            }
            if (activeBodies != mappedBodies)
                throw std::invalid_argument(
                    "Save-game physics body topology does not match the running project.");
        }
    };
}
