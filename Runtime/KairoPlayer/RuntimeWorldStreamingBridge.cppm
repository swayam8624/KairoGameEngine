module;

#include <cstddef>
#include <exception>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module Kairo.Player.RuntimeWorldStreamingBridge;

export import Kairo.Player.RuntimeProject;
export import Kairo.EngineCore.WorldStreaming;
export import Kairo.EngineCore.SceneComposition;
import Kairo.EngineCore.SceneSerializationV5;
import Kairo.Player.RuntimePhysicsBridge;

export namespace kairo::player
{
    enum class WorldStreamingOperation : unsigned char
    {
        Load,
        Unload
    };

    struct WorldStreamingFailure final
    {
        kairo::engine::WorldCellCoordinate Coordinate;
        std::string ContentKey;
        WorldStreamingOperation Operation = WorldStreamingOperation::Load;
        std::string Message;
    };

    struct RuntimeWorldStreamingUpdate final
    {
        kairo::engine::WorldStreamingPlan Plan;
        std::size_t LoadedCells = 0u;
        std::size_t UnloadedCells = 0u;
        std::size_t ActivatedPhysicsBodies = 0u;
        std::size_t DeactivatedPhysicsBodies = 0u;
        std::vector<WorldStreamingFailure> Failures;

        [[nodiscard]] bool Succeeded() const noexcept
        {
            return Failures.empty();
        }
    };

    class RuntimeWorldStreamingBridge final
    {
    public:
        explicit RuntimeWorldStreamingBridge(RuntimeProject& project,
            kairo::engine::WorldStreamingConfig config = {})
            : m_Project(project), m_Runtime(std::move(config)) {}

        RuntimeWorldStreamingBridge(RuntimeProject& project,
            RuntimePhysicsBridge& physics,
            kairo::engine::WorldStreamingConfig config = {})
            : m_Project(project), m_Runtime(std::move(config)), m_Physics(&physics) {}

        void RegisterCell(kairo::engine::WorldStreamingCellDescriptor descriptor)
        {
            if (!EndsWithSceneExtension(descriptor.ContentKey))
                throw std::invalid_argument(
                    "Runtime world streaming cell content must use the .kscene extension.");
            m_Runtime.RegisterCell(std::move(descriptor));
        }

        bool UnregisterCell(kairo::engine::WorldCellCoordinate coordinate)
        {
            if (m_Ownership.contains(coordinate))
                throw std::logic_error(
                    "A loaded runtime world cell cannot be unregistered.");
            return m_Runtime.UnregisterCell(coordinate);
        }

        [[nodiscard]] kairo::engine::WorldStreamingRuntime& Runtime() noexcept
        {
            return m_Runtime;
        }

        [[nodiscard]] const kairo::engine::WorldStreamingRuntime& Runtime() const noexcept
        {
            return m_Runtime;
        }

        [[nodiscard]] std::size_t LoadedCellCount() const noexcept
        {
            return m_Ownership.size();
        }

        [[nodiscard]] bool SynchronizesPhysics() const noexcept
        {
            return m_Physics != nullptr;
        }

        [[nodiscard]] const kairo::engine::SceneAppendResult* Ownership(
            kairo::engine::WorldCellCoordinate coordinate) const noexcept
        {
            const auto found = m_Ownership.find(coordinate);
            return found == m_Ownership.end() ? nullptr : &found->second;
        }

        [[nodiscard]] RuntimeWorldStreamingUpdate Update(
            std::span<const kairo::engine::WorldStreamingObserver> observers)
        {
            RuntimeWorldStreamingUpdate result;
            result.Plan = m_Runtime.PlanUpdate(observers);

            for (const auto& request : result.Plan.Unloads)
                ExecuteUnload(request, result);
            for (const auto& request : result.Plan.Loads)
                ExecuteLoad(request, result);
            return result;
        }

    private:
        RuntimeProject& m_Project;
        kairo::engine::WorldStreamingRuntime m_Runtime;
        RuntimePhysicsBridge* m_Physics = nullptr;
        std::map<kairo::engine::WorldCellCoordinate,
            kairo::engine::SceneAppendResult> m_Ownership;

        [[nodiscard]] static bool EndsWithSceneExtension(
            const std::string& contentKey) noexcept
        {
            constexpr std::string_view suffix = ".kscene";
            return contentKey.size() >= suffix.size() &&
                std::string_view(contentKey).substr(contentKey.size() - suffix.size()) == suffix;
        }

        static std::string CurrentExceptionMessage()
        {
            try { throw; }
            catch (const std::exception& error) { return error.what(); }
            catch (...) { return "Unknown world streaming failure."; }
        }

        void ExecuteLoad(const kairo::engine::WorldStreamingRequest& request,
            RuntimeWorldStreamingUpdate& result)
        {
            kairo::engine::SceneAppendResult appended;
            bool appendedToWorld = false;
            bool activatedPhysics = false;
            RuntimePhysicsTopologyChange topology;
            try
            {
                if (m_Ownership.contains(request.Coordinate))
                    throw std::logic_error(
                        "World streaming attempted to load a cell that already owns live scene entities.");

                const auto path = m_Project.RequireProjectFile(
                    request.ContentKey, "world streaming scene cell");
                if (path.extension() != ".kscene")
                    throw std::invalid_argument(
                        "Resolved world streaming cell must use the .kscene extension.");

                kairo::engine::Scene fragment;
                // Scene v5 is the world-cell disk boundary. It delegates legacy
                // v1-v4 documents unchanged while preserving authored emitters and
                // listeners for new cells, so streaming no longer strips audio.
                kairo::engine::LoadSceneV5(path, m_Project.Assets(), fragment);
                appended = kairo::engine::AppendScene(m_Project.Scene(), fragment);
                appendedToWorld = true;

                const auto destinations = appended.DestinationEntities();
                if (m_Physics != nullptr)
                {
                    topology = m_Physics->ActivateEntities(destinations);
                    activatedPhysics = true;
                }

                try
                {
                    const auto [entry, inserted] = m_Ownership.emplace(
                        request.Coordinate, appended);
                    (void)entry;
                    if (!inserted)
                        throw std::logic_error(
                            "World streaming ownership token already exists for the loading cell.");
                }
                catch (...)
                {
                    if (activatedPhysics && m_Physics != nullptr)
                        (void)m_Physics->DeactivateEntities(destinations);
                    if (appendedToWorld)
                        (void)kairo::engine::RemoveAppendedScene(
                            m_Project.Scene(), appended);
                    throw;
                }

                m_Runtime.CompleteLoad(request.Coordinate, true);
                ++result.LoadedCells;
                result.ActivatedPhysicsBodies += topology.ActivatedBodies;
            }
            catch (...)
            {
                if (appendedToWorld && !activatedPhysics)
                {
                    try
                    {
                        (void)kairo::engine::RemoveAppendedScene(
                            m_Project.Scene(), appended);
                    }
                    catch (...) {}
                }
                const std::string message = CurrentExceptionMessage();
                if (m_Runtime.State(request.Coordinate) ==
                    kairo::engine::WorldCellState::Loading)
                    m_Runtime.CompleteLoad(request.Coordinate, false);
                result.Failures.push_back({
                    request.Coordinate,
                    request.ContentKey,
                    WorldStreamingOperation::Load,
                    message
                });
            }
        }

        void ExecuteUnload(const kairo::engine::WorldStreamingRequest& request,
            RuntimeWorldStreamingUpdate& result)
        {
            try
            {
                const auto found = m_Ownership.find(request.Coordinate);
                if (found == m_Ownership.end())
                    throw std::logic_error(
                        "World streaming unload has no scene-composition ownership token.");

                kairo::engine::Scene candidate = m_Project.Scene();
                (void)kairo::engine::RemoveAppendedScene(candidate, found->second);
                const auto destinations = found->second.DestinationEntities();

                RuntimePhysicsTopologyChange topology;
                if (m_Physics != nullptr)
                    topology = m_Physics->DeactivateEntities(destinations);

                m_Project.Scene() = std::move(candidate);
                m_Ownership.erase(found);
                m_Runtime.CompleteUnload(request.Coordinate, true);
                ++result.UnloadedCells;
                result.DeactivatedPhysicsBodies += topology.DeactivatedBodies;
            }
            catch (...)
            {
                const std::string message = CurrentExceptionMessage();
                if (m_Runtime.State(request.Coordinate) ==
                    kairo::engine::WorldCellState::Unloading)
                    m_Runtime.CompleteUnload(request.Coordinate, false);
                result.Failures.push_back({
                    request.Coordinate,
                    request.ContentKey,
                    WorldStreamingOperation::Unload,
                    message
                });
            }
        }
    };
}
