module;

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

export module Kairo.Player.RuntimePackaging;

import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Player.RuntimeProject;

export namespace kairo::player
{
    /// Packaging is intentionally bounded. These limits protect launcher and
    /// CI workflows from accidentally traversing a cache, mounted volume, or
    /// malformed project containing an unreasonable number of files.
    inline constexpr std::uint64_t MaxPackageProjectBytes = 16ull * 1024ull * 1024ull * 1024ull;
    inline constexpr std::size_t MaxPackageProjectFiles = 100'000u;

    /// Input: an exact authored build-profile name, the player executable to
    /// bundle, and whether an existing artifact may be atomically replaced.
    /// Task: keep destructive replacement explicit. Normal builds fail when
    /// the profile output already exists so a typo cannot silently erase it.
    struct PackageOptions final
    {
        std::string ProfileName;
        std::filesystem::path PlayerExecutable;
        bool ReplaceExisting = false;
    };

    /// Output: the published bundle location and deterministic project payload
    /// accounting. Project byte/file counts exclude the copied player,
    /// launcher, and package manifest because those are toolchain artifacts.
    struct PackageResult final
    {
        std::filesystem::path OutputDirectory;
        std::filesystem::path ManifestPath;
        std::size_t ProjectFileCount = 0u;
        std::uint64_t ProjectByteCount = 0u;
    };

    namespace runtime_packaging_detail
    {
        struct ProjectPayload final
        {
            std::vector<std::filesystem::path> Files;
            std::uint64_t Bytes = 0u;
        };

        class StagingDirectory final
        {
        public:
            explicit StagingDirectory(std::filesystem::path path) : m_Path(std::move(path)) {}
            StagingDirectory(const StagingDirectory&) = delete;
            StagingDirectory& operator=(const StagingDirectory&) = delete;

            ~StagingDirectory()
            {
                if (!m_Armed) return;
                std::error_code ignored;
                std::filesystem::remove_all(m_Path, ignored);
            }

            void Release() noexcept { m_Armed = false; }

        private:
            std::filesystem::path m_Path;
            bool m_Armed = true;
        };

        [[nodiscard]] inline bool IsSameOrDescendant(
            const std::filesystem::path& candidate, const std::filesystem::path& ancestor)
        {
            auto candidatePart = candidate.begin();
            for (auto ancestorPart = ancestor.begin(); ancestorPart != ancestor.end();
                ++ancestorPart, ++candidatePart)
            {
                if (candidatePart == candidate.end() || *candidatePart != *ancestorPart) return false;
            }
            return true;
        }

        [[nodiscard]] inline bool IsTransientKairoPath(const std::filesystem::path& relative)
        {
            auto part = relative.begin();
            if (part == relative.end() || *part != ".kairo") return false;
            ++part;
            return part != relative.end() && *part != "compiled";
        }

        [[nodiscard]] inline std::string QuoteManifestText(std::string_view value)
        {
            std::string result = "\"";
            for (const char character : value)
            {
                if (character == '\\' || character == '"') result.push_back('\\');
                if (character == '\n') result += "\\n";
                else if (character == '\r') result += "\\r";
                else result.push_back(character);
            }
            result.push_back('"');
            return result;
        }

        inline void WriteTextFile(const std::filesystem::path& path, std::string_view text)
        {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output) throw std::runtime_error("Cannot create package file: " + path.string());
            output.write(text.data(), static_cast<std::streamsize>(text.size()));
            output.flush();
            if (!output) throw std::runtime_error("Cannot finish package file: " + path.string());
        }

        [[nodiscard]] inline std::filesystem::path FindTemporarySibling(
            const std::filesystem::path& output, std::string_view role)
        {
            const auto seed = static_cast<std::uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count());
            for (std::uint64_t attempt = 0u; attempt < 64u; ++attempt)
            {
                const auto name = "." + output.filename().string() + ".kairo-" +
                    std::string(role) + "-" + std::to_string(seed + attempt);
                const auto candidate = output.parent_path() / name;
                std::error_code error;
                if (!std::filesystem::exists(candidate, error) && !error) return candidate;
            }
            throw std::runtime_error("Cannot reserve an adjacent package " + std::string(role) + " directory.");
        }

        [[nodiscard]] inline const kairo::engine::ProjectBuildProfile& FindProfile(
            const RuntimeProject& project, std::string_view name)
        {
            if (name.empty()) throw std::invalid_argument("Package build-profile name cannot be empty.");
            const auto& profiles = project.Descriptor().BuildProfiles;
            const auto found = std::find_if(profiles.begin(), profiles.end(),
                [name](const auto& profile) { return profile.Name == name; });
            if (found == profiles.end())
                throw std::invalid_argument("Unknown project build profile: " + std::string(name));
            return *found;
        }

        [[nodiscard]] inline std::filesystem::path RequirePlayerExecutable(
            const std::filesystem::path& requested)
        {
            if (requested.empty()) throw std::invalid_argument("Packaging requires a KairoPlayer executable path.");
            std::error_code error;
            const auto resolved = std::filesystem::canonical(requested, error);
            if (error || !std::filesystem::is_regular_file(resolved, error) || error)
                throw std::invalid_argument("KairoPlayer executable is not a readable regular file: " + requested.string());
            return resolved;
        }

        [[nodiscard]] inline ProjectPayload CollectProjectPayload(
            const RuntimeProject& project,
            const std::vector<std::filesystem::path>& excludedOutputs)
        {
            ProjectPayload payload;
            std::error_code error;
            std::filesystem::recursive_directory_iterator iterator(
                project.Root(), std::filesystem::directory_options::none, error);
            const std::filesystem::recursive_directory_iterator end;
            if (error) throw std::runtime_error("Cannot enumerate project root: " + error.message());

            for (; iterator != end; iterator.increment(error))
            {
                if (error) throw std::runtime_error("Cannot enumerate project payload: " + error.message());
                const auto relative = iterator->path().lexically_relative(project.Root());
                const auto status = iterator->symlink_status(error);
                if (error) throw std::runtime_error("Cannot inspect project payload entry: " + relative.string());

                const bool excludedOutput = std::any_of(excludedOutputs.begin(), excludedOutputs.end(),
                    [&relative](const auto& output) { return IsSameOrDescendant(relative, output); });
                const auto first = relative.begin();
                const bool gitMetadata = first != relative.end() && *first == ".git";
                const bool transientKairo = IsTransientKairoPath(relative);
                if (excludedOutput || gitMetadata || transientKairo)
                {
                    if (std::filesystem::is_directory(status)) iterator.disable_recursion_pending();
                    continue;
                }

                if (std::filesystem::is_symlink(status))
                    throw std::invalid_argument("Project packages cannot contain symbolic links: " + relative.string());
                if (std::filesystem::is_directory(status)) continue;
                if (!std::filesystem::is_regular_file(status))
                    throw std::invalid_argument("Project package entry is not a regular file: " + relative.string());

                // The active descriptor is always relocated to this canonical
                // name. Omit a secondary root descriptor with that reserved
                // name rather than allowing it to collide with the active one.
                if (iterator->path() == project.DescriptorPath() || relative == "Project.kproject") continue;
                const auto size = std::filesystem::file_size(iterator->path(), error);
                if (error) throw std::runtime_error("Cannot measure project payload file: " + relative.string());
                if (size > MaxPackageProjectBytes - payload.Bytes)
                    throw std::length_error("Project package exceeds the 16 GiB payload limit.");
                payload.Bytes += size;
                payload.Files.push_back(relative);
                if (payload.Files.size() > MaxPackageProjectFiles)
                    throw std::length_error("Project package exceeds the 100000-file payload limit.");
            }

            const auto descriptorBytes = std::filesystem::file_size(project.DescriptorPath(), error);
            if (error || descriptorBytes > MaxPackageProjectBytes - payload.Bytes)
                throw std::length_error("Project package descriptor exceeds the payload limit.");
            payload.Bytes += descriptorBytes;
            payload.Files.push_back("Project.kproject");
            std::sort(payload.Files.begin(), payload.Files.end(),
                [](const auto& left, const auto& right)
                { return left.generic_string() < right.generic_string(); });
            return payload;
        }

        inline void CopyFilePreservingPermissions(
            const std::filesystem::path& source, const std::filesystem::path& destination)
        {
            std::error_code error;
            std::filesystem::create_directories(destination.parent_path(), error);
            if (error) throw std::runtime_error("Cannot create package directory: " + destination.parent_path().string());
            if (!std::filesystem::copy_file(source, destination,
                std::filesystem::copy_options::none, error) || error)
                throw std::runtime_error("Cannot copy package file " + source.string() + ": " + error.message());
            const auto permissions = std::filesystem::status(source, error).permissions();
            if (!error) std::filesystem::permissions(destination, permissions, error);
            if (error) throw std::runtime_error("Cannot preserve package file permissions: " + destination.string());
        }

        inline void PublishStagingDirectory(const std::filesystem::path& staging,
            const std::filesystem::path& output, bool replaceExisting)
        {
            std::error_code error;
            const bool exists = std::filesystem::exists(output, error);
            if (error) throw std::runtime_error("Cannot inspect package output: " + error.message());
            if (exists && !replaceExisting)
                throw std::invalid_argument("Package output already exists; pass --replace to replace it atomically: " + output.string());
            if (exists && !std::filesystem::is_directory(output, error))
                throw std::invalid_argument("Package output exists but is not a directory: " + output.string());
            if (error) throw std::runtime_error("Cannot inspect package output type: " + error.message());

            if (!exists)
            {
                std::filesystem::rename(staging, output, error);
                if (error) throw std::runtime_error("Cannot publish package output: " + error.message());
                return;
            }

            const auto backup = FindTemporarySibling(output, "backup");
            std::filesystem::rename(output, backup, error);
            if (error) throw std::runtime_error("Cannot move existing package aside: " + error.message());
            std::filesystem::rename(staging, output, error);
            if (error)
            {
                const auto publishError = error.message();
                std::error_code restoreError;
                std::filesystem::rename(backup, output, restoreError);
                if (restoreError)
                    throw std::runtime_error("Package publish failed and rollback also failed: " +
                        publishError + "; rollback: " + restoreError.message());
                throw std::runtime_error("Package publish failed; previous output restored: " + publishError);
            }
            std::filesystem::remove_all(backup, error);
            if (error) throw std::runtime_error("Package published, but the previous-output backup could not be removed: " + error.message());
        }
    }

    /// Input: a fully validated runtime project and explicit packaging options.
    /// Output: an atomically published runtime bundle containing the project,
    /// current KairoPlayer executable, launcher, and inspectable manifest.
    ///
    /// Task: create a portable project/runtime boundary without copying source
    /// control metadata, editor recovery journals, derived caches, or any build
    /// profile output back into itself. The packaged project is loaded again
    /// from staging before publication, so malformed or incomplete artifacts
    /// never replace a previously working bundle.
    [[nodiscard]] inline PackageResult PackageRuntimeProject(
        const RuntimeProject& project, const PackageOptions& options)
    {
        using namespace runtime_packaging_detail;
        const auto& profile = FindProfile(project, options.ProfileName);
        const auto player = RequirePlayerExecutable(options.PlayerExecutable);
        const auto outputRelative = kairo::assets::NormalizeAssetPath(profile.OutputDirectory);
        std::vector<std::filesystem::path> excludedOutputs;
        excludedOutputs.reserve(project.Descriptor().BuildProfiles.size());
        for (const auto& buildProfile : project.Descriptor().BuildProfiles)
            excludedOutputs.push_back(kairo::assets::NormalizeAssetPath(buildProfile.OutputDirectory));

        const auto requestedOutput = project.Root() / outputRelative;
        std::error_code error;
        std::filesystem::create_directories(requestedOutput.parent_path(), error);
        if (error) throw std::runtime_error("Cannot create package output parent: " + error.message());
        const auto canonicalParent = std::filesystem::canonical(requestedOutput.parent_path(), error);
        if (error || !IsSameOrDescendant(canonicalParent, project.Root()))
            throw std::invalid_argument("Package output parent escapes the project through a symbolic link.");
        const auto output = canonicalParent / requestedOutput.filename();
        const auto outputStatus = std::filesystem::symlink_status(output, error);
        if (error && error != std::errc::no_such_file_or_directory)
            throw std::runtime_error("Cannot inspect package output path: " + error.message());
        if (!error && std::filesystem::is_symlink(outputStatus))
            throw std::invalid_argument("Package output cannot be a symbolic link: " + output.string());
        error.clear();
        const auto staging = FindTemporarySibling(output, "stage");
        std::filesystem::create_directory(staging, error);
        if (error) throw std::runtime_error("Cannot create package staging directory: " + error.message());
        StagingDirectory cleanup(staging);

        const auto payload = CollectProjectPayload(project, excludedOutputs);
        const auto packagedProject = staging / "project";
        for (const auto& relative : payload.Files)
        {
            if (relative == "Project.kproject") continue;
            CopyFilePreservingPermissions(project.Root() / relative, packagedProject / relative);
        }
        CopyFilePreservingPermissions(project.DescriptorPath(), packagedProject / "Project.kproject");

        const auto packagedPlayer = staging / "bin" / player.filename();
        CopyFilePreservingPermissions(player, packagedPlayer);

#if defined(_WIN32)
        const auto launcher = staging / "run.cmd";
        WriteTextFile(launcher,
            "@echo off\r\n"
            "set \"KAIRO_PACKAGE_ROOT=%~dp0\"\r\n"
            "\"%KAIRO_PACKAGE_ROOT%bin\\" + player.filename().string() +
            "\" \"%KAIRO_PACKAGE_ROOT%project\\Project.kproject\" %*\r\n");
#else
        const auto launcher = staging / "run.sh";
        WriteTextFile(launcher,
            "#!/bin/sh\n"
            "set -eu\n"
            "KAIRO_PACKAGE_ROOT=$(CDPATH= cd -- \"$(dirname -- \"$0\")\" && pwd)\n"
            "exec \"$KAIRO_PACKAGE_ROOT/bin/" + player.filename().string() +
            "\" \"$KAIRO_PACKAGE_ROOT/project/Project.kproject\" \"$@\"\n");
        std::filesystem::permissions(launcher,
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
            std::filesystem::perms::owner_exec | std::filesystem::perms::group_read |
            std::filesystem::perms::group_exec | std::filesystem::perms::others_read |
            std::filesystem::perms::others_exec, std::filesystem::perm_options::replace, error);
        if (error) throw std::runtime_error("Cannot mark package launcher executable: " + error.message());
#endif

        const auto manifest = staging / "package.kmanifest";
        const std::string manifestSource =
            "kairo-package 1\nproject \"project/Project.kproject\"\nprofile " +
            QuoteManifestText(profile.Name) + "\nkind " +
            std::string(kairo::engine::Name(profile.Kind)) + "\nengine-version " +
            QuoteManifestText(project.Descriptor().EngineVersion) + "\nplayer " +
            QuoteManifestText((std::filesystem::path("bin") / player.filename()).generic_string()) +
            "\nproject-files " + std::to_string(payload.Files.size()) +
            "\nproject-bytes " + std::to_string(payload.Bytes) + "\n";
        WriteTextFile(manifest, manifestSource);

        // Re-open the relocated project before publication. This exercises all
        // descriptor-relative bootstrap paths inside the exact staged layout.
        (void)RuntimeProject(packagedProject / "Project.kproject");
        PublishStagingDirectory(staging, output, options.ReplaceExisting);
        cleanup.Release();
        return { output, output / "package.kmanifest", payload.Files.size(), payload.Bytes };
    }
}
