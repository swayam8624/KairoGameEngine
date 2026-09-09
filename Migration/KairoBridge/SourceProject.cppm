module;

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

export module Kairo.Bridge.SourceProject;

import Kairo.Bridge.Types;

export namespace kairo::bridge
{
    struct SourceProjectDescriptor final
    {
        SourceEngine Engine = SourceEngine::Unknown;
        std::filesystem::path Root;
        std::filesystem::path ProjectFile;
        std::string DisplayName;

        [[nodiscard]] bool IsRecognized() const noexcept
        {
            return Engine != SourceEngine::Unknown;
        }

        void Validate() const
        {
            if (Root.empty())
                throw std::invalid_argument("Source project root cannot be empty.");
            if (!Root.is_absolute())
                throw std::invalid_argument("Source project root must be absolute.");
            if (Engine != SourceEngine::Unknown && ProjectFile.empty())
                throw std::invalid_argument(
                    "Recognized source projects require a project marker file.");
        }
    };

    namespace detail
    {
        [[nodiscard]] inline std::vector<std::filesystem::path> FindUnrealProjectFiles(
            const std::filesystem::path& root)
        {
            std::vector<std::filesystem::path> files;
            if (!std::filesystem::is_directory(root)) return files;

            for (const auto& entry : std::filesystem::directory_iterator(root))
            {
                if (!entry.is_regular_file()) continue;
                if (entry.path().extension() == ".uproject")
                    files.push_back(entry.path().filename());
            }
            return files;
        }
    }

    /// Detects a source engine by project-owned marker files only. The detector
    /// intentionally rejects ambiguous roots rather than guessing because a
    /// wrong source API can silently corrupt large migrations.
    [[nodiscard]] inline SourceProjectDescriptor DetectSourceProject(
        const std::filesystem::path& inputRoot)
    {
        if (inputRoot.empty())
            throw std::invalid_argument("Source project root cannot be empty.");

        const std::filesystem::path root =
            std::filesystem::absolute(inputRoot).lexically_normal();
        if (!std::filesystem::is_directory(root))
            throw std::invalid_argument("Source project root must be an existing directory.");

        const std::filesystem::path unityMarker =
            root / "ProjectSettings" / "ProjectVersion.txt";
        const std::filesystem::path unityAssets = root / "Assets";
        const bool unity = std::filesystem::is_regular_file(unityMarker) &&
            std::filesystem::is_directory(unityAssets);

        const std::filesystem::path godotMarker = root / "project.godot";
        const bool godot = std::filesystem::is_regular_file(godotMarker);

        const auto unrealFiles = detail::FindUnrealProjectFiles(root);
        if (unrealFiles.size() > 1u)
            throw std::invalid_argument(
                "Unreal migration roots must contain exactly one .uproject file.");
        const bool unreal = unrealFiles.size() == 1u;

        const std::size_t recognizedCount =
            static_cast<std::size_t>(unity) +
            static_cast<std::size_t>(unreal) +
            static_cast<std::size_t>(godot);
        if (recognizedCount > 1u)
            throw std::invalid_argument(
                "Source project contains markers for multiple game engines.");

        SourceProjectDescriptor descriptor;
        descriptor.Root = root;
        descriptor.DisplayName = root.filename().string();

        if (unity)
        {
            descriptor.Engine = SourceEngine::Unity;
            descriptor.ProjectFile = unityMarker;
        }
        else if (unreal)
        {
            descriptor.Engine = SourceEngine::Unreal;
            descriptor.ProjectFile = root / unrealFiles.front();
            descriptor.DisplayName = unrealFiles.front().stem().string();
        }
        else if (godot)
        {
            descriptor.Engine = SourceEngine::Godot;
            descriptor.ProjectFile = godotMarker;
        }

        descriptor.Validate();
        return descriptor;
    }
}
