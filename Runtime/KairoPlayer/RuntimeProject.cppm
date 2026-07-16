module;

#include <filesystem>
#include <stdexcept>
#include <string>

export module Kairo.Player.RuntimeProject;

import Kairo.Assets;
import Kairo.EngineCore;

export namespace kairo::player
{
    /// Input: a user-supplied project descriptor path.
    /// Output: an absolute, normalized path to a regular `.kproject` file.
    /// Task: establish one deterministic project root before resolving any
    /// portable path from authored data. Symlinks are resolved by the host
    /// filesystem so subsequent containment checks operate on real locations.
    [[nodiscard]] inline std::filesystem::path ResolveProjectDescriptorPath(
        const std::filesystem::path& requested)
    {
        if (requested.empty()) throw std::invalid_argument("KairoPlayer requires a project descriptor path.");
        std::error_code error;
        const auto path = std::filesystem::weakly_canonical(requested, error);
        if (error || !std::filesystem::is_regular_file(path, error) || error)
            throw std::invalid_argument("Project descriptor is not a readable regular file: " + requested.string());
        if (path.extension() != ".kproject")
            throw std::invalid_argument("KairoPlayer project descriptors must use the .kproject extension.");
        return path;
    }

    /// Fully validated project bootstrap state shared by headless validation
    /// and the native player. Runtime-only GPU and physics handles are
    /// deliberately absent; their owning adapters are constructed afterwards.
    class RuntimeProject final
    {
    public:
        explicit RuntimeProject(const std::filesystem::path& requestedDescriptor)
            : m_DescriptorPath(ResolveProjectDescriptorPath(requestedDescriptor)),
              m_Root(m_DescriptorPath.parent_path()),
              m_Descriptor(kairo::engine::LoadProjectDescriptor(m_DescriptorPath))
        {
            const auto manifest = RequireProjectFile(m_Descriptor.AssetManifest, "asset manifest");
            const auto scene = RequireProjectFile(m_Descriptor.StartupScene, "startup scene");
            kairo::assets::LoadAssetManifest(manifest, m_Assets);
            kairo::engine::LoadScene(scene, m_Assets, m_Scene);
        }

        [[nodiscard]] const std::filesystem::path& DescriptorPath() const noexcept { return m_DescriptorPath; }
        [[nodiscard]] const std::filesystem::path& Root() const noexcept { return m_Root; }
        [[nodiscard]] const kairo::engine::ProjectDescriptor& Descriptor() const noexcept { return m_Descriptor; }
        [[nodiscard]] const kairo::assets::AssetRegistry& Assets() const noexcept { return m_Assets; }
        [[nodiscard]] const kairo::engine::Scene& Scene() const noexcept { return m_Scene; }
        [[nodiscard]] kairo::engine::Scene& Scene() noexcept { return m_Scene; }

        /// Input: a validated project-relative portable path.
        /// Output: canonical regular file contained by this project root.
        /// Degeneracy: missing files, directories, and symlink escapes fail
        /// before a parser receives bytes from outside the project boundary.
        [[nodiscard]] std::filesystem::path RequireProjectFile(
            const std::filesystem::path& relative, const char* role) const
        {
            const auto portable = kairo::assets::NormalizeAssetPath(relative);
            std::error_code error;
            const auto resolved = std::filesystem::weakly_canonical(m_Root / portable, error);
            if (error || !IsWithinRoot(resolved))
                throw std::invalid_argument(std::string(role) + " escapes or cannot be resolved inside the project.");
            if (!std::filesystem::is_regular_file(resolved, error) || error)
                throw std::invalid_argument(std::string(role) + " is not a readable regular file: " + portable.string());
            return resolved;
        }

    private:
        [[nodiscard]] bool IsWithinRoot(const std::filesystem::path& path) const noexcept
        {
            auto root = m_Root.begin();
            auto candidate = path.begin();
            for (; root != m_Root.end(); ++root, ++candidate)
                if (candidate == path.end() || *candidate != *root) return false;
            return true;
        }

        std::filesystem::path m_DescriptorPath;
        std::filesystem::path m_Root;
        kairo::engine::ProjectDescriptor m_Descriptor;
        kairo::assets::AssetRegistry m_Assets;
        kairo::engine::Scene m_Scene;
    };
}
