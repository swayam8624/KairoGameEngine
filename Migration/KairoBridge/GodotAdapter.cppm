module;

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

export module Kairo.Bridge.GodotAdapter;

import Kairo.Bridge.Types;
import Kairo.Bridge.SourceProject;
import Kairo.Bridge.CanonicalIR;

export namespace kairo::bridge
{
    struct GodotResourceRecord final
    {
        SourceIdentity Source;
        CanonicalKind Kind = CanonicalKind::Other;
        std::string ResourceType;
        bool HasExplicitUID = false;
    };

    struct GodotProjectScan final
    {
        std::uint32_t ConfigVersion = 0u;
        std::string ProjectName;
        std::string MainScene;
        std::vector<GodotResourceRecord> Resources;
    };

    namespace godot_detail
    {
        [[nodiscard]] inline std::string Trim(std::string value)
        {
            const auto first = std::find_if_not(value.begin(), value.end(),
                [](unsigned char c) { return std::isspace(c) != 0; });
            const auto last = std::find_if_not(value.rbegin(), value.rend(),
                [](unsigned char c) { return std::isspace(c) != 0; }).base();
            if (first >= last) return {};
            return std::string(first, last);
        }

        [[nodiscard]] inline std::string Unquote(std::string value)
        {
            value = Trim(std::move(value));
            if (value.size() >= 2u && value.front() == '"' && value.back() == '"')
                return value.substr(1u, value.size() - 2u);
            return value;
        }

        [[nodiscard]] inline std::string LowerExtension(std::filesystem::path path)
        {
            std::string extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return extension;
        }

        [[nodiscard]] inline std::optional<std::string> ExtractQuotedAttribute(
            std::string_view line, std::string_view name)
        {
            const std::string pattern = std::string(name) + "=\"";
            const std::size_t begin = line.find(pattern);
            if (begin == std::string_view::npos) return std::nullopt;
            const std::size_t valueBegin = begin + pattern.size();
            const std::size_t end = line.find('"', valueBegin);
            if (end == std::string_view::npos)
                throw std::invalid_argument(
                    "Godot text resource contains an unterminated quoted attribute.");
            return std::string(line.substr(valueBegin, end - valueBegin));
        }

        [[nodiscard]] inline bool IsGodotUID(std::string_view uid) noexcept
        {
            if (!uid.starts_with("uid://") || uid.size() <= 6u) return false;
            for (const char value : uid.substr(6u))
            {
                const auto c = static_cast<unsigned char>(value);
                if (std::isspace(c) || value == '"' || value == '\\') return false;
            }
            return true;
        }

        struct TextResourceHeader final
        {
            std::string UID;
            std::string Type;
        };

        [[nodiscard]] inline TextResourceHeader ReadTextResourceHeader(
            const std::filesystem::path& file)
        {
            std::ifstream stream(file);
            if (!stream)
                throw std::runtime_error("Unable to read Godot resource: " + file.string());

            std::string line;
            while (std::getline(stream, line))
            {
                line = Trim(std::move(line));
                if (line.empty() || line.starts_with(';') || line.starts_with('#'))
                    continue;
                if (!line.starts_with("[gd_scene") && !line.starts_with("[gd_resource"))
                    return {};

                TextResourceHeader header;
                if (const auto uid = ExtractQuotedAttribute(line, "uid"); uid.has_value())
                {
                    if (!IsGodotUID(*uid))
                        throw std::invalid_argument(
                            "Godot text resource contains an invalid uid attribute: " +
                            file.string());
                    header.UID = *uid;
                }
                if (const auto type = ExtractQuotedAttribute(line, "type"); type.has_value())
                    header.Type = *type;
                return header;
            }
            return {};
        }

        [[nodiscard]] inline CanonicalKind KindForPath(
            const std::filesystem::path& path, std::string_view resourceType)
        {
            const std::string extension = LowerExtension(path);
            if (extension == ".tscn" || extension == ".scn") return CanonicalKind::Scene;
            if (extension == ".gd" || extension == ".cs") return CanonicalKind::Script;
            if (extension == ".gdshader") return CanonicalKind::Shader;
            if (extension == ".glb" || extension == ".gltf" || extension == ".obj" ||
                extension == ".fbx" || extension == ".dae") return CanonicalKind::Mesh;
            if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
                extension == ".webp" || extension == ".svg" || extension == ".tga" ||
                extension == ".exr" || extension == ".hdr") return CanonicalKind::Texture;
            if (extension == ".wav" || extension == ".ogg" || extension == ".mp3")
                return CanonicalKind::AudioClip;

            if (extension == ".tres" || extension == ".res")
            {
                if (resourceType.find("NavigationMesh") != std::string_view::npos)
                    return CanonicalKind::NavigationMesh;
                if (resourceType.find("Material") != std::string_view::npos)
                    return CanonicalKind::Material;
                if (resourceType.find("Shader") != std::string_view::npos)
                    return CanonicalKind::Shader;
                if (resourceType.find("Animation") != std::string_view::npos)
                    return CanonicalKind::AnimationClip;
                if (resourceType.find("Mesh") != std::string_view::npos)
                    return CanonicalKind::Mesh;
            }
            return CanonicalKind::Other;
        }

        [[nodiscard]] inline std::optional<std::string> ReadProjectSetting(
            const std::filesystem::path& projectFile,
            std::string_view section,
            std::string_view key)
        {
            std::ifstream stream(projectFile);
            if (!stream)
                throw std::runtime_error("Unable to read Godot project.godot file.");

            std::string currentSection;
            std::string line;
            while (std::getline(stream, line))
            {
                line = Trim(std::move(line));
                if (line.empty() || line.starts_with(';') || line.starts_with('#')) continue;
                if (line.front() == '[' && line.back() == ']')
                {
                    currentSection = line.substr(1u, line.size() - 2u);
                    continue;
                }
                if (currentSection != section) continue;
                const std::size_t equals = line.find('=');
                if (equals == std::string::npos) continue;
                if (Trim(line.substr(0u, equals)) != key) continue;
                return Unquote(line.substr(equals + 1u));
            }
            return std::nullopt;
        }

        [[nodiscard]] inline std::uint32_t ReadConfigVersion(
            const std::filesystem::path& projectFile)
        {
            std::ifstream stream(projectFile);
            if (!stream)
                throw std::runtime_error("Unable to read Godot project.godot file.");
            std::string line;
            while (std::getline(stream, line))
            {
                line = Trim(std::move(line));
                if (!line.starts_with("config_version=")) continue;
                const std::string value = Trim(line.substr(std::string("config_version=").size()));
                if (value.empty()) break;
                std::size_t parsed = 0u;
                const unsigned long version = std::stoul(value, &parsed, 10);
                if (parsed != value.size() || version == 0ul)
                    throw std::invalid_argument("Godot config_version must be a positive integer.");
                return static_cast<std::uint32_t>(version);
            }
            throw std::invalid_argument("Godot project.godot is missing config_version.");
        }

        [[nodiscard]] inline bool ShouldSkipDirectory(const std::filesystem::path& path)
        {
            const std::string name = path.filename().string();
            return name == ".godot" || name == ".git" || name == ".hg" || name == ".svn";
        }
    }

    /// Performs a deterministic Godot discovery pass over source-controlled
    /// project data. Godot 4 text resources expose durable uid:// identities;
    /// formats without an embedded UID use a project-relative path identity as a
    /// deliberate fallback until the binary UID cache adapter is implemented.
    [[nodiscard]] inline GodotProjectScan ScanGodotProject(
        const SourceProjectDescriptor& descriptor)
    {
        descriptor.Validate();
        if (descriptor.Engine != SourceEngine::Godot)
            throw std::invalid_argument("Godot adapter requires a Godot source project.");

        GodotProjectScan scan;
        scan.ConfigVersion = godot_detail::ReadConfigVersion(descriptor.ProjectFile);
        scan.ProjectName = godot_detail::ReadProjectSetting(
            descriptor.ProjectFile, "application", "config/name").value_or(descriptor.DisplayName);
        scan.MainScene = godot_detail::ReadProjectSetting(
            descriptor.ProjectFile, "application", "run/main_scene").value_or("");

        std::unordered_set<std::string> identities;
        std::filesystem::recursive_directory_iterator iterator(descriptor.Root);
        const std::filesystem::recursive_directory_iterator end;
        while (iterator != end)
        {
            const auto entry = *iterator;
            if (entry.is_directory() && godot_detail::ShouldSkipDirectory(entry.path()))
            {
                iterator.disable_recursion_pending();
                ++iterator;
                continue;
            }
            if (!entry.is_regular_file())
            {
                ++iterator;
                continue;
            }

            const std::filesystem::path relative =
                std::filesystem::relative(entry.path(), descriptor.Root).lexically_normal();
            if (relative == "project.godot")
            {
                ++iterator;
                continue;
            }

            const std::string extension = godot_detail::LowerExtension(relative);
            GodotResourceRecord record;
            std::string explicitUID;
            std::string resourceType;
            if (extension == ".tscn" || extension == ".tres")
            {
                const auto header = godot_detail::ReadTextResourceHeader(entry.path());
                explicitUID = header.UID;
                resourceType = header.Type;
            }

            const std::string stableID = explicitUID.empty()
                ? "path:" + relative.generic_string()
                : explicitUID;
            if (!identities.insert(stableID).second)
                throw std::invalid_argument(
                    "Godot project contains a duplicate resource identity: " + stableID);

            record.Source = { SourceEngine::Godot, stableID, relative };
            record.ResourceType = std::move(resourceType);
            record.HasExplicitUID = !explicitUID.empty();
            record.Kind = godot_detail::KindForPath(relative, record.ResourceType);
            record.Source.Validate();
            scan.Resources.push_back(std::move(record));
            ++iterator;
        }

        std::sort(scan.Resources.begin(), scan.Resources.end(),
            [](const GodotResourceRecord& left, const GodotResourceRecord& right)
            {
                return left.Source.SourcePath.generic_string() <
                    right.Source.SourcePath.generic_string();
            });
        return scan;
    }

    [[nodiscard]] inline CanonicalProjectIR BuildGodotDiscoveryIR(
        const SourceProjectDescriptor& descriptor)
    {
        const GodotProjectScan scan = ScanGodotProject(descriptor);

        CanonicalProjectIR project;
        project.Source = SourceEngine::Godot;
        project.ProjectName = scan.ProjectName.empty() ? descriptor.DisplayName : scan.ProjectName;
        for (const GodotResourceRecord& resource : scan.Resources)
        {
            CanonicalNode node;
            if (resource.HasExplicitUID)
                node.CanonicalID = "godot/uid/" + resource.Source.StableID.substr(6u);
            else
                node.CanonicalID = "godot/path/" + resource.Source.SourcePath.generic_string();
            node.Source = resource.Source;
            node.Kind = resource.Kind;
            node.Name = resource.Source.SourcePath.filename().string();
            node.Properties.push_back(
                {"godot.config-version", std::to_string(scan.ConfigVersion)});
            node.Properties.push_back(
                {"godot.identity", resource.HasExplicitUID ? "uid" : "path-fallback"});
            if (!resource.ResourceType.empty())
                node.Properties.push_back({"godot.resource-type", resource.ResourceType});
            if (!scan.MainScene.empty())
                node.Properties.push_back({"godot.main-scene", scan.MainScene});
            project.Nodes.push_back(std::move(node));
        }

        project.SortDeterministically();
        project.Validate();
        return project;
    }
}
