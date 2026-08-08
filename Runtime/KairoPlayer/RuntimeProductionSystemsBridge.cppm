module;

#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>

export module Kairo.Player.RuntimeProductionSystemsBridge;

import Kairo.EngineCore;
import Kairo.Player.RuntimeProject;

export namespace kairo::player
{
    class RuntimeProductionSystemsBridge final
    {
    public:
        explicit RuntimeProductionSystemsBridge(RuntimeProject& project,
            kairo::engine::ProductionPerformanceBudget budget = {})
            : m_Project(project), m_Budget(budget)
        {
            const auto path = project.Root() /
                kairo::engine::DefaultProductionSystemsManifestPath;
            std::error_code error;
            if (!std::filesystem::exists(path, error))
            {
                if (error) throw std::runtime_error(
                    "Failed while checking production systems manifest: " + error.message());
                return;
            }
            if (!std::filesystem::is_regular_file(path, error) || error)
                throw std::invalid_argument(
                    "Production systems manifest is not a readable regular file: " + path.string());
            auto manifest = kairo::engine::LoadProductionSystemsManifest(path);
            kairo::engine::ValidateProductionSystemsManifest(manifest, m_Budget);
            m_Runtime = std::make_unique<kairo::engine::ProductionRuntime>(
                std::move(manifest), m_Budget);
        }

        [[nodiscard]] bool Enabled() const noexcept { return static_cast<bool>(m_Runtime); }
        [[nodiscard]] kairo::engine::ProductionRuntime* Runtime() noexcept { return m_Runtime.get(); }
        [[nodiscard]] const kairo::engine::ProductionRuntime* Runtime() const noexcept { return m_Runtime.get(); }

        void Step(double deltaSeconds)
        {
            if (!m_Runtime) return;
            double x = 0.0;
            double z = 0.0;
            if (const auto camera = m_Project.Scene().PrimaryCamera(); camera.has_value())
            {
                const auto world = m_Project.Scene().WorldTransform(*camera);
                x = static_cast<double>(world.Translation.x);
                z = static_cast<double>(world.Translation.z);
            }
            m_Runtime->Step(deltaSeconds, x, z);
        }

        [[nodiscard]] std::optional<kairo::engine::ProductionRuntimeProfile> Profile() const
        {
            if (!m_Runtime) return std::nullopt;
            return m_Runtime->Profile();
        }

    private:
        RuntimeProject& m_Project;
        kairo::engine::ProductionPerformanceBudget m_Budget;
        std::unique_ptr<kairo::engine::ProductionRuntime> m_Runtime;
    };
}
