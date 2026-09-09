module;

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module Kairo.Player.RuntimeSaveGameBridge;

import Kairo.EngineCore;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeProject;

export namespace kairo::player
{
    inline constexpr std::string_view RuntimePhysicsSaveChunkName = "kairo.physics";
    inline constexpr std::uint32_t RuntimePhysicsSaveChunkSchema =
        kairo::foundation::physics::PhysicsSnapshotFileVersion;

    /// Player-level composition boundary for the subsystem-owned save formats.
    /// EngineCore owns the KSAVE001 container and scene snapshot. PhysicsEngine
    /// owns the deterministic PhysicsWorld payload. This bridge only validates
    /// that the running project can safely accept both snapshots together.
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
            archive.SetChunk(MakePhysicsChunk(m_Physics.CaptureSnapshot()));
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
            if (!archive.ContainsChunk(RuntimePhysicsSaveChunkName))
                throw std::invalid_argument(
                    "Runtime save-game is missing its physics snapshot chunk.");

            // Parse and semantically validate every subsystem before mutating the
            // running project. A malformed physics payload therefore cannot leave
            // the Scene restored while PhysicsWorld remains on the previous state.
            kairo::engine::Scene savedScene = kairo::engine::ParseSceneSaveChunk(
                archive.Chunk(kairo::engine::SceneSaveChunkName),
                m_Project.Assets());
            const auto physicsSnapshot = ParsePhysicsChunk(
                archive.Chunk(RuntimePhysicsSaveChunkName));
            kairo::foundation::physics::PhysicsWorld physicsValidation;
            physicsValidation.RestoreSnapshot(physicsSnapshot);

            ValidateSceneTopology(m_Project.Scene(), savedScene);
            ValidatePhysicsTopology(physicsSnapshot);

            // RuntimeProject owns the Scene object by value. Move-assignment
            // preserves that object's address, so renderer/physics/logic bridges
            // that hold Scene& remain valid. RuntimePhysicsBridge then restores
            // exact body state and collapses interpolation history to the loaded
            // pose without waking sleeping bodies.
            m_Project.Scene() = std::move(savedScene);
            m_Physics.RestoreSnapshot(physicsSnapshot);
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
