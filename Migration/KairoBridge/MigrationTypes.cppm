module;

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

export module Kairo.Bridge.Types;

export namespace kairo::bridge
{
    enum class SourceEngine : std::uint8_t
    {
        Unknown,
        Unity,
        Unreal,
        Godot
    };

    [[nodiscard]] constexpr std::string_view NameOfSourceEngine(SourceEngine engine) noexcept
    {
        switch (engine)
        {
            case SourceEngine::Unity: return "unity";
            case SourceEngine::Unreal: return "unreal";
            case SourceEngine::Godot: return "godot";
            case SourceEngine::Unknown: return "unknown";
        }
        return "unknown";
    }

    enum class CanonicalKind : std::uint16_t
    {
        Project,
        World,
        WorldCell,
        Scene,
        Entity,
        Prefab,
        Mesh,
        Material,
        Texture,
        Shader,
        Skeleton,
        AnimationClip,
        AnimationGraph,
        Camera,
        Light,
        Collider,
        RigidBody,
        Joint,
        CharacterController,
        AudioClip,
        AudioEmitter,
        AudioListener,
        NavigationMesh,
        NavigationAgent,
        Terrain,
        ParticleSystem,
        Script,
        VisualScript,
        UserInterface,
        InputAction,
        DataTable,
        GameplayTag,
        Other
    };

    [[nodiscard]] constexpr std::string_view NameOfCanonicalKind(CanonicalKind kind) noexcept
    {
        switch (kind)
        {
            case CanonicalKind::Project: return "project";
            case CanonicalKind::World: return "world";
            case CanonicalKind::WorldCell: return "world-cell";
            case CanonicalKind::Scene: return "scene";
            case CanonicalKind::Entity: return "entity";
            case CanonicalKind::Prefab: return "prefab";
            case CanonicalKind::Mesh: return "mesh";
            case CanonicalKind::Material: return "material";
            case CanonicalKind::Texture: return "texture";
            case CanonicalKind::Shader: return "shader";
            case CanonicalKind::Skeleton: return "skeleton";
            case CanonicalKind::AnimationClip: return "animation-clip";
            case CanonicalKind::AnimationGraph: return "animation-graph";
            case CanonicalKind::Camera: return "camera";
            case CanonicalKind::Light: return "light";
            case CanonicalKind::Collider: return "collider";
            case CanonicalKind::RigidBody: return "rigid-body";
            case CanonicalKind::Joint: return "joint";
            case CanonicalKind::CharacterController: return "character-controller";
            case CanonicalKind::AudioClip: return "audio-clip";
            case CanonicalKind::AudioEmitter: return "audio-emitter";
            case CanonicalKind::AudioListener: return "audio-listener";
            case CanonicalKind::NavigationMesh: return "navigation-mesh";
            case CanonicalKind::NavigationAgent: return "navigation-agent";
            case CanonicalKind::Terrain: return "terrain";
            case CanonicalKind::ParticleSystem: return "particle-system";
            case CanonicalKind::Script: return "script";
            case CanonicalKind::VisualScript: return "visual-script";
            case CanonicalKind::UserInterface: return "ui";
            case CanonicalKind::InputAction: return "input-action";
            case CanonicalKind::DataTable: return "data-table";
            case CanonicalKind::GameplayTag: return "gameplay-tag";
            case CanonicalKind::Other: return "other";
        }
        return "other";
    }

    enum class MigrationDisposition : std::uint8_t
    {
        Native,
        Compatibility,
        NeedsAttention,
        Unsupported
    };

    [[nodiscard]] constexpr std::string_view NameOfMigrationDisposition(
        MigrationDisposition disposition) noexcept
    {
        switch (disposition)
        {
            case MigrationDisposition::Native: return "native";
            case MigrationDisposition::Compatibility: return "compatibility";
            case MigrationDisposition::NeedsAttention: return "needs-attention";
            case MigrationDisposition::Unsupported: return "unsupported";
        }
        return "unsupported";
    }

    struct SourceIdentity final
    {
        SourceEngine Engine = SourceEngine::Unknown;
        std::string StableID;
        std::filesystem::path SourcePath;

        void Validate() const
        {
            if (Engine == SourceEngine::Unknown)
                throw std::invalid_argument("Migration source identity requires a known engine.");
            if (StableID.empty() && SourcePath.empty())
                throw std::invalid_argument(
                    "Migration source identity requires a stable ID or source path.");
            if (SourcePath.is_absolute())
                throw std::invalid_argument(
                    "Migration source identity paths must be project-relative.");
            for (const auto& component : SourcePath.lexically_normal())
                if (component == "..")
                    throw std::invalid_argument(
                        "Migration source identity paths cannot escape the project root.");
        }
    };

    /// Durable engine IDs are the identity. SourcePath is provenance only when
    /// a GUID/UID/package identity exists, and becomes the key only as a fallback.
    [[nodiscard]] inline std::string SourceIdentityKey(const SourceIdentity& identity)
    {
        identity.Validate();
        const std::string engine = std::string(NameOfSourceEngine(identity.Engine));
        if (!identity.StableID.empty())
            return engine + "|id|" + identity.StableID;
        return engine + "|path|" + identity.SourcePath.lexically_normal().generic_string();
    }
}
