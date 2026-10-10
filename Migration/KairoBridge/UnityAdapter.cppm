module;

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

export module Kairo.Bridge.UnityAdapter;

import Kairo.Bridge.Types;
import Kairo.Bridge.SourceProject;
import Kairo.Bridge.CanonicalIR;

export namespace kairo::bridge
{
    struct UnityAssetRecord final
    {
        SourceIdentity Source;
        CanonicalKind Kind = CanonicalKind::Other;
        bool IsDirectory = false;
    };

    struct UnityProjectScan final
    {
        std::string EditorVersion;
        std::string EditorVersionWithRevision;
        std::vector<UnityAssetRecord> Assets;
    };

    namespace unity_detail
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

        [[nodiscard]] inline std::optional<std::string> ReadKey(
            const std::filesystem::path& file, std::string_view key)
        {
            std::ifstream stream(file);
            if (!stream)
                throw std::runtime_error("Unable to read Unity metadata file: " + file.string());

            std::string line;
            const std::string prefix = std::string(key) + ":";
            while (std::getline(stream, line))
            {
                if (!line.starts_with(prefix)) continue;
                return Trim(line.substr(prefix.size()));
            }
            return std::nullopt;
        }

        [[nodiscard]] inline bool IsUnityGuid(std::string_view guid) noexcept
        {
            if (guid.size() != 32u) return false;
            for (const char value : guid)
            {
                const auto c = static_cast<unsigned char>(value);
                if (!std::isxdigit(c)) return false;
            }
            return true;
        }

        [[nodiscard]] inline std::string LowerExtension(std::filesystem::path path)
        {
            std::string extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return extension;
        }

        [[nodiscard]] inline CanonicalKind KindForPath(const std::filesystem::path& path)
        {
            const std::string extension = LowerExtension(path);
            if (extension == ".unity") return CanonicalKind::Scene;
            if (extension == ".prefab") return CanonicalKind::Prefab;
            if (extension == ".mat") return CanonicalKind::Material;
            if (extension == ".shader" || extension == ".shadergraph") return CanonicalKind::Shader;
            if (extension == ".fbx" || extension == ".obj" || extension == ".dae" ||
                extension == ".blend") return CanonicalKind::Mesh;
            if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
                extension == ".tga" || extension == ".tif" || extension == ".tiff" ||
                extension == ".exr" || extension == ".hdr") return CanonicalKind::Texture;
            if (extension == ".anim") return CanonicalKind::AnimationClip;
            if (extension == ".controller" || extension == ".overridecontroller")
                return CanonicalKind::AnimationGraph;
            if (extension == ".wav" || extension == ".ogg" || extension == ".mp3" ||
                extension == ".aiff" || extension == ".aif") return CanonicalKind::AudioClip;
            if (extension == ".cs") return CanonicalKind::Script;
            return CanonicalKind::Other;
        }

        [[nodiscard]] inline std::filesystem::path AssetPathForMeta(
            const std::filesystem::path& root, const std::filesystem::path& meta)
        {
            std::filesystem::path asset = meta;
            asset.replace_extension();
            return std::filesystem::relative(asset, root).lexically_normal();
        }
    }

    /// Performs the first lossless Unity discovery pass. It intentionally reads
    /// only source-controlled Unity metadata: project version and .meta GUIDs.
    /// Importer-specific YAML semantics are layered on later passes so stable
    /// identity never depends on a particular Unity serializer implementation.
    [[nodiscard]] inline UnityProjectScan ScanUnityProject(
        const SourceProjectDescriptor& descriptor)
    {
        descriptor.Validate();
        if (descriptor.Engine != SourceEngine::Unity)
            throw std::invalid_argument("Unity adapter requires a Unity source project.");

        UnityProjectScan scan;
        const auto versionFile = descriptor.Root / "ProjectSettings" / "ProjectVersion.txt";
        scan.EditorVersion = unity_detail::ReadKey(versionFile, "m_EditorVersion").value_or("");
        scan.EditorVersionWithRevision =
            unity_detail::ReadKey(versionFile, "m_EditorVersionWithRevision").value_or("");
        if (scan.EditorVersion.empty())
            throw std::invalid_argument("Unity ProjectVersion.txt is missing m_EditorVersion.");

        const auto assetsRoot = descriptor.Root / "Assets";
        std::unordered_set<std::string> guids;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(assetsRoot))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".meta") continue;

            const auto guidValue = unity_detail::ReadKey(entry.path(), "guid");
            if (!guidValue.has_value() || !unity_detail::IsUnityGuid(*guidValue))
                throw std::invalid_argument(
                    "Unity .meta file has no valid 32-hex GUID: " + entry.path().string());
            std::string guid = *guidValue;
            std::transform(guid.begin(), guid.end(), guid.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (!guids.insert(guid).second)
                throw std::invalid_argument("Unity project contains a duplicate asset GUID: " + guid);

            const std::filesystem::path sourcePath =
                unity_detail::AssetPathForMeta(descriptor.Root, entry.path());
            const bool directory = std::filesystem::is_directory(descriptor.Root / sourcePath);

            UnityAssetRecord record;
            record.Source = { SourceEngine::Unity, guid, sourcePath };
            record.Kind = directory ? CanonicalKind::Other : unity_detail::KindForPath(sourcePath);
            record.IsDirectory = directory;
            record.Source.Validate();
            scan.Assets.push_back(std::move(record));
        }

        std::sort(scan.Assets.begin(), scan.Assets.end(),
            [](const UnityAssetRecord& left, const UnityAssetRecord& right)
            {
                return left.Source.SourcePath.generic_string() <
                    right.Source.SourcePath.generic_string();
            });
        return scan;
    }

    /// Converts Unity discovery into deterministic canonical identities. This is
    /// intentionally a discovery IR: scene/prefab references and serialized
    /// component fields are filled by later semantic passes without changing IDs.
    [[nodiscard]] inline CanonicalProjectIR BuildUnityDiscoveryIR(
        const SourceProjectDescriptor& descriptor)
    {
        const UnityProjectScan scan = ScanUnityProject(descriptor);

        CanonicalProjectIR project;
        project.Source = SourceEngine::Unity;
        project.ProjectName = descriptor.DisplayName;
        for (const UnityAssetRecord& asset : scan.Assets)
        {
            CanonicalNode node;
            node.CanonicalID = "unity/" + asset.Source.StableID;
            node.Source = asset.Source;
            node.Kind = asset.Kind;
            node.Name = asset.Source.SourcePath.filename().string();
            node.Properties.push_back({"unity.editor-version", scan.EditorVersion});
            if (!scan.EditorVersionWithRevision.empty())
                node.Properties.push_back(
                    {"unity.editor-version-with-revision", scan.EditorVersionWithRevision});
            node.Properties.push_back({"unity.directory", asset.IsDirectory ? "true" : "false"});
            project.Nodes.push_back(std::move(node));
        }
        project.SortDeterministically();
        project.Validate();
        return project;
    }
}
