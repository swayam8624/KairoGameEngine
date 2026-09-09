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

export module Kairo.Bridge.UnrealAdapter;

import Kairo.Bridge.Types;
import Kairo.Bridge.SourceProject;
import Kairo.Bridge.CanonicalIR;

export namespace kairo::bridge
{
    struct UnrealAssetRecord final
    {
        SourceIdentity Source;
        CanonicalKind Kind = CanonicalKind::Other;
        std::string MountPoint;
        bool IsBinaryPackage = false;
        bool IsPluginDescriptor = false;
    };

    struct UnrealProjectScan final
    {
        std::uint32_t FileVersion = 0u;
        std::string EngineAssociation;
        std::vector<std::string> Modules;
        std::vector<std::string> Plugins;
        std::vector<UnrealAssetRecord> Assets;
    };

    namespace unreal_detail
    {
        [[nodiscard]] inline std::string Lower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        [[nodiscard]] inline std::string LowerExtension(std::filesystem::path path)
        {
            return Lower(path.extension().string());
        }

        [[nodiscard]] inline std::string ReadBoundedText(
            const std::filesystem::path& path, std::uintmax_t maxBytes = 4u * 1024u * 1024u)
        {
            const std::uintmax_t size = std::filesystem::file_size(path);
            if (size > maxBytes)
                throw std::invalid_argument(
                    "Unreal project metadata exceeds the bounded parser size limit: " + path.string());
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
                throw std::runtime_error("Unable to read Unreal metadata file: " + path.string());
            return std::string(std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>());
        }

        [[nodiscard]] inline std::optional<std::size_t> FindJsonValueStart(
            std::string_view text, std::string_view key, std::size_t searchFrom = 0u)
        {
            const std::string token = "\"" + std::string(key) + "\"";
            const std::size_t keyPosition = text.find(token, searchFrom);
            if (keyPosition == std::string_view::npos) return std::nullopt;
            const std::size_t colon = text.find(':', keyPosition + token.size());
            if (colon == std::string_view::npos)
                throw std::invalid_argument("Unreal JSON metadata contains a field without a value.");
            std::size_t value = colon + 1u;
            while (value < text.size() &&
                std::isspace(static_cast<unsigned char>(text[value])) != 0) ++value;
            if (value >= text.size())
                throw std::invalid_argument("Unreal JSON metadata contains a truncated field value.");
            return value;
        }

        [[nodiscard]] inline std::string ParseJsonStringAt(
            std::string_view text, std::size_t start)
        {
            if (start >= text.size() || text[start] != '"')
                throw std::invalid_argument("Expected a JSON string in Unreal project metadata.");
            std::string value;
            bool escaped = false;
            for (std::size_t index = start + 1u; index < text.size(); ++index)
            {
                const char c = text[index];
                if (escaped)
                {
                    switch (c)
                    {
                        case '"': value.push_back('"'); break;
                        case '\\': value.push_back('\\'); break;
                        case '/': value.push_back('/'); break;
                        case 'b': value.push_back('\b'); break;
                        case 'f': value.push_back('\f'); break;
                        case 'n': value.push_back('\n'); break;
                        case 'r': value.push_back('\r'); break;
                        case 't': value.push_back('\t'); break;
                        default:
                            throw std::invalid_argument(
                                "Unsupported JSON escape in Unreal project metadata.");
                    }
                    escaped = false;
                    continue;
                }
                if (c == '\\')
                {
                    escaped = true;
                    continue;
                }
                if (c == '"') return value;
                value.push_back(c);
            }
            throw std::invalid_argument("Unterminated JSON string in Unreal project metadata.");
        }

        [[nodiscard]] inline std::uint32_t ParseJsonUnsignedAt(
            std::string_view text, std::size_t start)
        {
            std::size_t end = start;
            while (end < text.size() &&
                std::isdigit(static_cast<unsigned char>(text[end])) != 0) ++end;
            if (end == start)
                throw std::invalid_argument("Expected an unsigned JSON number in Unreal metadata.");
            const std::string value(text.substr(start, end - start));
            std::size_t parsed = 0u;
            const unsigned long number = std::stoul(value, &parsed, 10);
            if (parsed != value.size())
                throw std::invalid_argument("Invalid numeric field in Unreal project metadata.");
            return static_cast<std::uint32_t>(number);
        }

        [[nodiscard]] inline std::optional<std::string> JsonStringField(
            std::string_view text, std::string_view key)
        {
            const auto start = FindJsonValueStart(text, key);
            if (!start.has_value()) return std::nullopt;
            return ParseJsonStringAt(text, *start);
        }

        [[nodiscard]] inline std::optional<std::uint32_t> JsonUnsignedField(
            std::string_view text, std::string_view key)
        {
            const auto start = FindJsonValueStart(text, key);
            if (!start.has_value()) return std::nullopt;
            return ParseJsonUnsignedAt(text, *start);
        }

        [[nodiscard]] inline std::optional<std::pair<std::size_t, std::size_t>> JsonArraySpan(
            std::string_view text, std::string_view key)
        {
            const auto valueStart = FindJsonValueStart(text, key);
            if (!valueStart.has_value()) return std::nullopt;
            if (text[*valueStart] != '[')
                throw std::invalid_argument("Expected a JSON array in Unreal project metadata.");

            std::size_t depth = 0u;
            bool inString = false;
            bool escaped = false;
            for (std::size_t index = *valueStart; index < text.size(); ++index)
            {
                const char c = text[index];
                if (inString)
                {
                    if (escaped) { escaped = false; continue; }
                    if (c == '\\') { escaped = true; continue; }
                    if (c == '"') inString = false;
                    continue;
                }
                if (c == '"') { inString = true; continue; }
                if (c == '[') ++depth;
                else if (c == ']')
                {
                    if (depth == 0u)
                        throw std::invalid_argument("Malformed JSON array in Unreal metadata.");
                    --depth;
                    if (depth == 0u) return std::pair{*valueStart, index + 1u};
                }
            }
            throw std::invalid_argument("Unterminated JSON array in Unreal project metadata.");
        }

        [[nodiscard]] inline std::vector<std::string> ObjectNamesInArray(
            std::string_view text, std::string_view arrayKey)
        {
            std::vector<std::string> names;
            const auto span = JsonArraySpan(text, arrayKey);
            if (!span.has_value()) return names;

            std::size_t cursor = span->first;
            while (cursor < span->second)
            {
                const auto nameStart = FindJsonValueStart(text, "Name", cursor);
                if (!nameStart.has_value() || *nameStart >= span->second) break;
                names.push_back(ParseJsonStringAt(text, *nameStart));
                cursor = *nameStart + 1u;
            }
            std::sort(names.begin(), names.end());
            names.erase(std::unique(names.begin(), names.end()), names.end());
            return names;
        }

        [[nodiscard]] inline bool ShouldSkipDirectory(const std::filesystem::path& path)
        {
            const std::string name = path.filename().string();
            return name == ".git" || name == ".hg" || name == ".svn" ||
                name == "Binaries" || name == "DerivedDataCache" ||
                name == "Intermediate" || name == "Saved";
        }

        [[nodiscard]] inline CanonicalKind KindForRawSource(const std::filesystem::path& path)
        {
            const std::string extension = LowerExtension(path);
            if (extension == ".fbx" || extension == ".obj" || extension == ".dae" ||
                extension == ".gltf" || extension == ".glb") return CanonicalKind::Mesh;
            if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
                extension == ".tga" || extension == ".tif" || extension == ".tiff" ||
                extension == ".exr" || extension == ".hdr") return CanonicalKind::Texture;
            if (extension == ".wav" || extension == ".ogg" || extension == ".mp3")
                return CanonicalKind::AudioClip;
            if (extension == ".h" || extension == ".hpp" || extension == ".inl" ||
                extension == ".cpp" || extension == ".cc" || extension == ".cxx" ||
                extension == ".cs") return CanonicalKind::Script;
            return CanonicalKind::Other;
        }

        [[nodiscard]] inline bool IsRawSourceAsset(const std::filesystem::path& path)
        {
            return KindForRawSource(path) != CanonicalKind::Other;
        }

        [[nodiscard]] inline std::string PackageName(
            std::string_view mount, const std::filesystem::path& contentRelative)
        {
            std::filesystem::path withoutExtension = contentRelative;
            withoutExtension.replace_extension();
            std::string package = "/" + std::string(mount);
            const std::string suffix = withoutExtension.generic_string();
            if (!suffix.empty()) package += "/" + suffix;
            return package;
        }

        [[nodiscard]] inline std::string IdentityCollisionKey(const SourceIdentity& identity)
        {
            if (!identity.StableID.empty()) return Lower(identity.StableID);
            return "path:" + Lower(identity.SourcePath.lexically_normal().generic_string());
        }

        inline void AddRecord(UnrealProjectScan& scan, UnrealAssetRecord record,
            std::unordered_set<std::string>& identities)
        {
            record.Source.Validate();
            const std::string key = IdentityCollisionKey(record.Source);
            if (!identities.insert(key).second)
                throw std::invalid_argument(
                    "Unreal project contains a cross-platform identity collision: " + key);
            scan.Assets.push_back(std::move(record));
        }

        inline void ScanContentRoot(UnrealProjectScan& scan,
            const std::filesystem::path& projectRoot,
            const std::filesystem::path& contentRoot,
            std::string_view mount,
            std::unordered_set<std::string>& identities)
        {
            if (!std::filesystem::is_directory(contentRoot)) return;
            std::filesystem::recursive_directory_iterator iterator(contentRoot);
            const std::filesystem::recursive_directory_iterator end;
            while (iterator != end)
            {
                const auto entry = *iterator;
                if (entry.is_directory() && ShouldSkipDirectory(entry.path()))
                {
                    iterator.disable_recursion_pending();
                    ++iterator;
                    continue;
                }
                if (!entry.is_regular_file()) { ++iterator; continue; }

                const std::filesystem::path projectRelative =
                    std::filesystem::relative(entry.path(), projectRoot).lexically_normal();
                const std::filesystem::path contentRelative =
                    std::filesystem::relative(entry.path(), contentRoot).lexically_normal();
                const std::string extension = LowerExtension(entry.path());

                UnrealAssetRecord record;
                record.Source.Engine = SourceEngine::Unreal;
                record.Source.SourcePath = projectRelative;
                record.MountPoint = "/" + std::string(mount);
                if (extension == ".uasset" || extension == ".umap")
                {
                    record.Source.StableID = PackageName(mount, contentRelative);
                    record.Kind = extension == ".umap" ? CanonicalKind::World : CanonicalKind::Other;
                    record.IsBinaryPackage = true;
                    AddRecord(scan, std::move(record), identities);
                }
                else if (IsRawSourceAsset(entry.path()))
                {
                    record.Kind = KindForRawSource(entry.path());
                    AddRecord(scan, std::move(record), identities);
                }
                ++iterator;
            }
        }

        inline void ScanSourceRoot(UnrealProjectScan& scan,
            const std::filesystem::path& projectRoot,
            const std::filesystem::path& sourceRoot,
            std::unordered_set<std::string>& identities)
        {
            if (!std::filesystem::is_directory(sourceRoot)) return;
            std::filesystem::recursive_directory_iterator iterator(sourceRoot);
            const std::filesystem::recursive_directory_iterator end;
            while (iterator != end)
            {
                const auto entry = *iterator;
                if (entry.is_directory() && ShouldSkipDirectory(entry.path()))
                {
                    iterator.disable_recursion_pending();
                    ++iterator;
                    continue;
                }
                if (!entry.is_regular_file()) { ++iterator; continue; }
                if (!IsRawSourceAsset(entry.path())) { ++iterator; continue; }

                UnrealAssetRecord record;
                record.Source = { SourceEngine::Unreal, "",
                    std::filesystem::relative(entry.path(), projectRoot).lexically_normal() };
                record.Kind = KindForRawSource(entry.path());
                AddRecord(scan, std::move(record), identities);
                ++iterator;
            }
        }
    }

    /// Discovers Unreal project topology without claiming to decode opaque
    /// .uasset/.umap internals. Binary packages retain their Unreal mount/package
    /// names as durable canonical identities; future AssetRegistry/UAsset and
    /// Blueprint passes enrich those identities rather than replacing them.
    [[nodiscard]] inline UnrealProjectScan ScanUnrealProject(
        const SourceProjectDescriptor& descriptor)
    {
        descriptor.Validate();
        if (descriptor.Engine != SourceEngine::Unreal)
            throw std::invalid_argument("Unreal adapter requires an Unreal source project.");

        const std::string projectJson = unreal_detail::ReadBoundedText(descriptor.ProjectFile);
        UnrealProjectScan scan;
        scan.FileVersion = unreal_detail::JsonUnsignedField(projectJson, "FileVersion").value_or(0u);
        if (scan.FileVersion == 0u)
            throw std::invalid_argument("Unreal .uproject is missing a positive FileVersion.");
        scan.EngineAssociation = unreal_detail::JsonStringField(
            projectJson, "EngineAssociation").value_or("");
        scan.Modules = unreal_detail::ObjectNamesInArray(projectJson, "Modules");
        scan.Plugins = unreal_detail::ObjectNamesInArray(projectJson, "Plugins");

        std::unordered_set<std::string> identities;
        unreal_detail::ScanContentRoot(scan, descriptor.Root,
            descriptor.Root / "Content", "Game", identities);
        unreal_detail::ScanSourceRoot(scan, descriptor.Root,
            descriptor.Root / "Source", identities);

        const std::filesystem::path pluginsRoot = descriptor.Root / "Plugins";
        if (std::filesystem::is_directory(pluginsRoot))
        {
            std::filesystem::recursive_directory_iterator iterator(pluginsRoot);
            const std::filesystem::recursive_directory_iterator end;
            while (iterator != end)
            {
                const auto entry = *iterator;
                if (entry.is_directory() && unreal_detail::ShouldSkipDirectory(entry.path()))
                {
                    iterator.disable_recursion_pending();
                    ++iterator;
                    continue;
                }
                if (!entry.is_regular_file() || unreal_detail::LowerExtension(entry.path()) != ".uplugin")
                {
                    ++iterator;
                    continue;
                }

                const std::string pluginName = entry.path().stem().string();
                if (pluginName.empty())
                    throw std::invalid_argument("Unreal plugin descriptor requires a non-empty name.");

                UnrealAssetRecord pluginRecord;
                pluginRecord.Source = { SourceEngine::Unreal,
                    "plugin:" + pluginName,
                    std::filesystem::relative(entry.path(), descriptor.Root).lexically_normal() };
                pluginRecord.Kind = CanonicalKind::Other;
                pluginRecord.MountPoint = "/" + pluginName;
                pluginRecord.IsPluginDescriptor = true;
                unreal_detail::AddRecord(scan, std::move(pluginRecord), identities);

                const std::filesystem::path pluginRoot = entry.path().parent_path();
                unreal_detail::ScanContentRoot(scan, descriptor.Root,
                    pluginRoot / "Content", pluginName, identities);
                unreal_detail::ScanSourceRoot(scan, descriptor.Root,
                    pluginRoot / "Source", identities);
                ++iterator;
            }
        }

        std::sort(scan.Assets.begin(), scan.Assets.end(),
            [](const UnrealAssetRecord& left, const UnrealAssetRecord& right)
            {
                return left.Source.SourcePath.generic_string() <
                    right.Source.SourcePath.generic_string();
            });
        return scan;
    }

    [[nodiscard]] inline CanonicalProjectIR BuildUnrealDiscoveryIR(
        const SourceProjectDescriptor& descriptor)
    {
        const UnrealProjectScan scan = ScanUnrealProject(descriptor);
        CanonicalProjectIR project;
        project.Source = SourceEngine::Unreal;
        project.ProjectName = descriptor.DisplayName;

        for (const UnrealAssetRecord& asset : scan.Assets)
        {
            CanonicalNode node;
            if (asset.Source.StableID.starts_with("/"))
                node.CanonicalID = "unreal/package" + asset.Source.StableID;
            else if (asset.Source.StableID.starts_with("plugin:"))
                node.CanonicalID = "unreal/plugin/" + asset.Source.StableID.substr(7u);
            else if (!asset.Source.StableID.empty())
                node.CanonicalID = "unreal/id/" + asset.Source.StableID;
            else
                node.CanonicalID = "unreal/path/" + asset.Source.SourcePath.generic_string();

            node.Source = asset.Source;
            node.Kind = asset.Kind;
            node.Name = asset.Source.SourcePath.filename().string();
            node.Properties.push_back({"unreal.file-version", std::to_string(scan.FileVersion)});
            if (!scan.EngineAssociation.empty())
                node.Properties.push_back({"unreal.engine-association", scan.EngineAssociation});
            node.Properties.push_back(
                {"unreal.binary-package", asset.IsBinaryPackage ? "true" : "false"});
            node.Properties.push_back(
                {"unreal.plugin-descriptor", asset.IsPluginDescriptor ? "true" : "false"});
            if (!asset.MountPoint.empty())
                node.Properties.push_back({"unreal.mount-point", asset.MountPoint});
            node.Properties.push_back(
                {"unreal.module-count", std::to_string(scan.Modules.size())});
            node.Properties.push_back(
                {"unreal.plugin-count", std::to_string(scan.Plugins.size())});
            project.Nodes.push_back(std::move(node));
        }

        project.SortDeterministically();
        project.Validate();
        return project;
    }
}
