module;

#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module Kairo.Player.RuntimeWorldSaveBridge;

import Kairo.EngineCore;
import Kairo.Player.RuntimeEntityLifecycleBridge;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimeSaveGameBridge;

export namespace kairo::player
{
    inline constexpr std::string_view RuntimeLifecycleSaveChunkName =
        "kairo.runtime-lifecycle";
    inline constexpr std::uint32_t RuntimeLifecycleSaveChunkSchema = 1u;

    namespace runtime_world_save_detail
    {
        inline constexpr std::uint8_t CharacterMotorFlag = 1u << 0u;
        inline constexpr std::uint8_t NavigationFlag = 1u << 1u;
        inline constexpr std::uint8_t CrowdFlag = 1u << 2u;
        inline constexpr std::uint8_t LocomotionAnimationFlag = 1u << 3u;
        inline constexpr std::uint8_t NavigationIntentFlag = 1u << 4u;
        inline constexpr std::size_t MaximumStringBytes = 4096u;
        inline constexpr std::size_t MaximumLifecycleEntities = 1'000'000u;

        inline void AppendU8(std::vector<std::byte>& output, std::uint8_t value)
        {
            output.push_back(static_cast<std::byte>(value));
        }

        inline void AppendU32(std::vector<std::byte>& output, std::uint32_t value)
        {
            for (unsigned shift = 0u; shift < 32u; shift += 8u)
                output.push_back(static_cast<std::byte>((value >> shift) & 0xffu));
        }

        inline void AppendU64(std::vector<std::byte>& output, std::uint64_t value)
        {
            for (unsigned shift = 0u; shift < 64u; shift += 8u)
                output.push_back(static_cast<std::byte>((value >> shift) & 0xffu));
        }

        inline void AppendF32(std::vector<std::byte>& output, float value)
        {
            AppendU32(output, std::bit_cast<std::uint32_t>(value));
        }

        inline void AppendF64(std::vector<std::byte>& output, double value)
        {
            AppendU64(output, std::bit_cast<std::uint64_t>(value));
        }

        inline void AppendString(std::vector<std::byte>& output,
            std::string_view value)
        {
            if (value.size() > MaximumStringBytes)
                throw std::length_error(
                    "Runtime lifecycle save string exceeds 4096 bytes.");
            AppendU32(output, static_cast<std::uint32_t>(value.size()));
            for (const unsigned char character : value)
                output.push_back(static_cast<std::byte>(character));
        }

        [[nodiscard]] inline std::uint8_t ReadU8(
            std::span<const std::byte> input, std::size_t& offset)
        {
            if (offset >= input.size())
                throw std::invalid_argument("Runtime lifecycle save payload is truncated.");
            return std::to_integer<std::uint8_t>(input[offset++]);
        }

        [[nodiscard]] inline std::uint32_t ReadU32(
            std::span<const std::byte> input, std::size_t& offset)
        {
            if (offset > input.size() || input.size() - offset < 4u)
                throw std::invalid_argument("Runtime lifecycle save payload is truncated.");
            std::uint32_t value = 0u;
            for (unsigned index = 0u; index < 4u; ++index)
                value |= static_cast<std::uint32_t>(
                    std::to_integer<std::uint8_t>(input[offset + index])) << (index * 8u);
            offset += 4u;
            return value;
        }

        [[nodiscard]] inline std::uint64_t ReadU64(
            std::span<const std::byte> input, std::size_t& offset)
        {
            if (offset > input.size() || input.size() - offset < 8u)
                throw std::invalid_argument("Runtime lifecycle save payload is truncated.");
            std::uint64_t value = 0u;
            for (unsigned index = 0u; index < 8u; ++index)
                value |= static_cast<std::uint64_t>(
                    std::to_integer<std::uint8_t>(input[offset + index])) << (index * 8u);
            offset += 8u;
            return value;
        }

        [[nodiscard]] inline float ReadF32(
            std::span<const std::byte> input, std::size_t& offset)
        {
            return std::bit_cast<float>(ReadU32(input, offset));
        }

        [[nodiscard]] inline double ReadF64(
            std::span<const std::byte> input, std::size_t& offset)
        {
            return std::bit_cast<double>(ReadU64(input, offset));
        }

        [[nodiscard]] inline std::string ReadString(
            std::span<const std::byte> input, std::size_t& offset)
        {
            const std::uint32_t length = ReadU32(input, offset);
            if (length > MaximumStringBytes ||
                offset > input.size() || input.size() - offset < length)
                throw std::invalid_argument(
                    "Runtime lifecycle save string is invalid or truncated.");
            std::string value;
            value.resize(length);
            for (std::uint32_t index = 0u; index < length; ++index)
                value[index] = static_cast<char>(
                    std::to_integer<unsigned char>(input[offset + index]));
            offset += length;
            return value;
        }

        inline void AppendProfile(std::vector<std::byte>& output,
            const RuntimeEntityServiceProfile& profile)
        {
            profile.Validate();
            std::uint8_t flags = 0u;
            if (profile.CharacterMotor) flags |= CharacterMotorFlag;
            if (profile.Navigation) flags |= NavigationFlag;
            if (profile.Crowd) flags |= CrowdFlag;
            if (profile.LocomotionAnimation) flags |= LocomotionAnimationFlag;
            if (profile.InitialNavigationIntent.has_value()) flags |= NavigationIntentFlag;
            AppendU8(output, flags);

            if (profile.CharacterMotor)
            {
                const auto& value = profile.CharacterMotorSettings;
                AppendF32(output, value.Gravity);
                AppendF32(output, value.JumpSpeed);
                AppendF32(output, value.MaximumFallSpeed);
                AppendF32(output, value.SkinWidth);
                AppendF32(output, value.GroundProbeDistance);
                AppendF32(output, value.MaxSlopeAngleRadians);
                AppendU32(output, value.MaxSlideIterations);
                AppendU32(output, value.SweepSamples);
            }
            if (profile.Navigation)
            {
                const auto& value = profile.NavigationSettings;
                AppendF32(output, value.MaximumSpeed);
                AppendF32(output, value.WaypointRadius);
                AppendF32(output, value.PathSnapDistance);
                AppendF32(output, value.VerticalTolerance);
            }
            if (profile.Crowd)
            {
                const auto& value = profile.CrowdSettings;
                AppendF32(output, value.Radius);
                AppendF32(output, value.NeighborDistance);
                AppendF32(output, value.TimeHorizon);
                AppendF32(output, value.MaximumAcceleration);
                AppendF32(output, value.StuckSpeedThreshold);
                AppendF32(output, value.StuckTimeBeforeReplan);
                AppendF32(output, value.ReplanCooldown);
            }
            if (profile.LocomotionAnimation)
            {
                const auto& value = profile.LocomotionAnimationSettings;
                AppendString(output, value.IdleClip);
                AppendString(output, value.WalkClip);
                AppendString(output, value.RunClip);
                AppendString(output, value.AirborneClip);
                AppendF32(output, value.MovingSpeedThreshold);
                AppendF32(output, value.RunSpeedThreshold);
                AppendF32(output, value.WalkReferenceSpeed);
                AppendF32(output, value.RunReferenceSpeed);
                AppendF32(output, value.MinimumPlaybackRate);
                AppendF32(output, value.MaximumPlaybackRate);
                AppendU8(output, value.FaceMovementDirection ? 1u : 0u);
            }
            if (profile.InitialNavigationIntent.has_value())
            {
                const auto& value = *profile.InitialNavigationIntent;
                AppendF64(output, value.Destination.X);
                AppendF64(output, value.Destination.Y);
                AppendF64(output, value.Destination.Z);
                AppendF64(output, value.AcceptanceRadius);
                AppendU8(output, value.AllowPartialPath ? 1u : 0u);
            }
        }

        [[nodiscard]] inline RuntimeEntityServiceProfile ReadProfile(
            std::span<const std::byte> input, std::size_t& offset)
        {
            RuntimeEntityServiceProfile profile;
            const std::uint8_t flags = ReadU8(input, offset);
            constexpr std::uint8_t known = CharacterMotorFlag | NavigationFlag |
                CrowdFlag | LocomotionAnimationFlag | NavigationIntentFlag;
            if ((flags & ~known) != 0u)
                throw std::invalid_argument(
                    "Runtime lifecycle save contains unknown service flags.");

            profile.CharacterMotor = (flags & CharacterMotorFlag) != 0u;
            profile.Navigation = (flags & NavigationFlag) != 0u;
            profile.Crowd = (flags & CrowdFlag) != 0u;
            profile.LocomotionAnimation =
                (flags & LocomotionAnimationFlag) != 0u;

            if (profile.CharacterMotor)
            {
                auto& value = profile.CharacterMotorSettings;
                value.Gravity = ReadF32(input, offset);
                value.JumpSpeed = ReadF32(input, offset);
                value.MaximumFallSpeed = ReadF32(input, offset);
                value.SkinWidth = ReadF32(input, offset);
                value.GroundProbeDistance = ReadF32(input, offset);
                value.MaxSlopeAngleRadians = ReadF32(input, offset);
                value.MaxSlideIterations = ReadU32(input, offset);
                value.SweepSamples = ReadU32(input, offset);
            }
            if (profile.Navigation)
            {
                auto& value = profile.NavigationSettings;
                value.MaximumSpeed = ReadF32(input, offset);
                value.WaypointRadius = ReadF32(input, offset);
                value.PathSnapDistance = ReadF32(input, offset);
                value.VerticalTolerance = ReadF32(input, offset);
            }
            if (profile.Crowd)
            {
                auto& value = profile.CrowdSettings;
                value.Radius = ReadF32(input, offset);
                value.NeighborDistance = ReadF32(input, offset);
                value.TimeHorizon = ReadF32(input, offset);
                value.MaximumAcceleration = ReadF32(input, offset);
                value.StuckSpeedThreshold = ReadF32(input, offset);
                value.StuckTimeBeforeReplan = ReadF32(input, offset);
                value.ReplanCooldown = ReadF32(input, offset);
            }
            if (profile.LocomotionAnimation)
            {
                auto& value = profile.LocomotionAnimationSettings;
                value.IdleClip = ReadString(input, offset);
                value.WalkClip = ReadString(input, offset);
                value.RunClip = ReadString(input, offset);
                value.AirborneClip = ReadString(input, offset);
                value.MovingSpeedThreshold = ReadF32(input, offset);
                value.RunSpeedThreshold = ReadF32(input, offset);
                value.WalkReferenceSpeed = ReadF32(input, offset);
                value.RunReferenceSpeed = ReadF32(input, offset);
                value.MinimumPlaybackRate = ReadF32(input, offset);
                value.MaximumPlaybackRate = ReadF32(input, offset);
                const auto face = ReadU8(input, offset);
                if (face > 1u)
                    throw std::invalid_argument(
                        "Runtime lifecycle animation facing flag is invalid.");
                value.FaceMovementDirection = face != 0u;
            }
            if ((flags & NavigationIntentFlag) != 0u)
            {
                kairo::ai::gameplay::NavigationIntent intent;
                intent.Destination.X = ReadF64(input, offset);
                intent.Destination.Y = ReadF64(input, offset);
                intent.Destination.Z = ReadF64(input, offset);
                intent.AcceptanceRadius = ReadF64(input, offset);
                const auto partial = ReadU8(input, offset);
                if (partial > 1u)
                    throw std::invalid_argument(
                        "Runtime lifecycle navigation partial-path flag is invalid.");
                intent.AllowPartialPath = partial != 0u;
                profile.InitialNavigationIntent = intent;
            }
            profile.Validate();
            return profile;
        }
    }

    [[nodiscard]] inline kairo::engine::SaveGameChunk MakeRuntimeLifecycleSaveChunk(
        const RuntimeEntityLifecycleSnapshot& snapshot)
    {
        if (snapshot.Version != RuntimeEntityLifecycleSnapshotVersion)
            throw std::invalid_argument(
                "Cannot serialize an unsupported runtime lifecycle snapshot.");
        if (snapshot.Entities.size() >
            runtime_world_save_detail::MaximumLifecycleEntities)
            throw std::length_error(
                "Runtime lifecycle snapshot exceeds the entity safety limit.");

        std::vector<std::byte> payload;
        runtime_world_save_detail::AppendU32(payload, snapshot.Version);
        runtime_world_save_detail::AppendU32(payload,
            static_cast<std::uint32_t>(snapshot.Entities.size()));
        for (const auto& entry : snapshot.Entities)
        {
            runtime_world_save_detail::AppendU32(payload, entry.Entity.Value);
            runtime_world_save_detail::AppendProfile(payload, entry.Profile);
        }
        return { std::string(RuntimeLifecycleSaveChunkName),
            RuntimeLifecycleSaveChunkSchema, std::move(payload) };
    }

    [[nodiscard]] inline RuntimeEntityLifecycleSnapshot ParseRuntimeLifecycleSaveChunk(
        const kairo::engine::SaveGameChunk& chunk)
    {
        if (chunk.Name != RuntimeLifecycleSaveChunkName ||
            chunk.SchemaVersion != RuntimeLifecycleSaveChunkSchema)
            throw std::invalid_argument(
                "Save-game chunk is not a supported runtime lifecycle snapshot.");
        std::size_t offset = 0u;
        RuntimeEntityLifecycleSnapshot snapshot;
        snapshot.Version = runtime_world_save_detail::ReadU32(chunk.Payload, offset);
        const auto count = runtime_world_save_detail::ReadU32(chunk.Payload, offset);
        if (count > runtime_world_save_detail::MaximumLifecycleEntities)
            throw std::length_error(
                "Runtime lifecycle save exceeds the entity safety limit.");
        snapshot.Entities.reserve(count);
        for (std::uint32_t index = 0u; index < count; ++index)
        {
            RuntimeEntityLifecycleSnapshotEntry entry;
            entry.Entity = { runtime_world_save_detail::ReadU32(chunk.Payload, offset) };
            entry.Profile = runtime_world_save_detail::ReadProfile(chunk.Payload, offset);
            snapshot.Entities.push_back(std::move(entry));
        }
        if (offset != chunk.Payload.size())
            throw std::invalid_argument(
                "Runtime lifecycle save payload contains trailing bytes.");
        return snapshot;
    }

    /// All-or-nothing world save boundary for the Playable World pass. The lower
    /// RuntimeSaveGameBridge restores Scene + authored audio + exact PhysicsWorld
    /// topology; this layer adds process-local NPC service state and current
    /// navigation intent. A failure in either half reconstructs the pre-load world.
    class RuntimeWorldSaveBridge final
    {
    public:
        RuntimeWorldSaveBridge(RuntimeProject& project,
            RuntimePhysicsBridge& physics,
            RuntimeEntityLifecycleBridge& lifecycle)
            : m_Base(project, physics), m_Lifecycle(lifecycle) {}

        [[nodiscard]] kairo::engine::SaveGameArchive Capture(
            std::string label = {}, std::uint64_t sequence = 0u) const
        {
            auto archive = m_Base.Capture(std::move(label), sequence);
            archive.SetChunk(MakeRuntimeLifecycleSaveChunk(
                m_Lifecycle.CaptureSnapshot()));
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
            if (!archive.ContainsChunk(RuntimeLifecycleSaveChunkName))
                throw std::invalid_argument(
                    "Runtime world save is missing its lifecycle snapshot chunk.");
            const auto savedLifecycle = ParseRuntimeLifecycleSaveChunk(
                archive.Chunk(RuntimeLifecycleSaveChunkName));

            const auto previousArchive = m_Base.Capture(
                "runtime-world-rollback", 0u);
            const auto previousLifecycle = m_Lifecycle.CaptureSnapshot();
            m_Lifecycle.ResetServicesForRestore();
            try
            {
                m_Base.Restore(archive);
                m_Lifecycle.RestoreSnapshot(savedLifecycle);
            }
            catch (...)
            {
                const std::exception_ptr original = std::current_exception();
                try
                {
                    m_Lifecycle.ResetServicesForRestore();
                    m_Base.Restore(previousArchive);
                    m_Lifecycle.RestoreSnapshot(previousLifecycle);
                }
                catch (...)
                {
                    throw std::runtime_error(
                        "Runtime world restore failed and its cross-subsystem rollback also failed.");
                }
                std::rethrow_exception(original);
            }
        }

    private:
        RuntimeSaveGameBridge m_Base;
        RuntimeEntityLifecycleBridge& m_Lifecycle;
    };
}
