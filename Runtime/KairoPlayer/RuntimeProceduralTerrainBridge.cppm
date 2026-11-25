module;

#include "TerrainNativeStore.hpp"
#include <cmath>
#include <cstdio>
#include <exception>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>

export module Kairo.Player.RuntimeProceduralTerrainBridge;

export import Kairo.EngineCore.WorldStreaming;
import Kairo.Renderer;
import Kairo.Foundation.Math;

export namespace kairo::player
{
    struct RuntimeProceduralTerrainUpdate final
    {
        kairo::engine::WorldStreamingPlan Plan;
        std::size_t Loaded=0,Unloaded=0,FailedLoads=0,FailedUnloads=0;
        std::exception_ptr FirstFailure;
        [[nodiscard]] bool Succeeded() const noexcept
        { return FailedLoads==0 && FailedUnloads==0; }
    };

    // Owns GPU mesh handles, not a duplicate resident CPU terrain store.
    // The renderer must outlive this bridge. Authored scene draws must not
    // borrow these handles; the bridge appends terrain each frame itself.
    class RuntimeProceduralTerrainBridge final
    {
    public:
        RuntimeProceduralTerrainBridge(kairo::renderer::RendererRuntime& renderer,
            kairo::engine::ProceduralWorldConfig config={})
            : m_Renderer(renderer),m_Streaming(std::move(config)) {}
        RuntimeProceduralTerrainBridge(const RuntimeProceduralTerrainBridge&)=delete;
        RuntimeProceduralTerrainBridge& operator=(const RuntimeProceduralTerrainBridge&)=delete;

        ~RuntimeProceduralTerrainBridge() noexcept
        {
            const auto failures=Shutdown(kairo::renderer::RenderScene{});
            if(failures)std::fprintf(stderr,"Terrain bridge could not release %zu mesh handles.\n",failures);
        }

        [[nodiscard]] std::size_t ResidentCount() const noexcept {return m_Store.Cells().size();}
        [[nodiscard]] const kairo::engine::ProceduralWorldStreamingRuntime& Streaming() const noexcept
        {return m_Streaming;}

        // Observer coordinates are absolute doubles. The supplied authored
        // scene and camera must already use the same render origin as terrain.
        // Default origin zero preserves existing small-world behaviour.
        [[nodiscard]] RuntimeProceduralTerrainUpdate UpdateAndSubmit(
            std::span<const kairo::engine::WorldStreamingObserver> observers,
            const kairo::renderer::RenderScene& authored,
            const kairo::foundation::math::Vec3d& renderOrigin={})
        {
            if(m_Closed)throw std::logic_error("Terrain bridge has been shut down.");
            if(!std::isfinite(renderOrigin.x)||!std::isfinite(renderOrigin.y)||!std::isfinite(renderOrigin.z))
                throw std::invalid_argument("Terrain render origin must be finite.");
            auto plan=m_Streaming.PlanUpdate(observers);
            const auto stats=detail::ApplyTerrainNativePlan(m_Store,m_Streaming,plan,
                [&]{m_Renderer.SubmitRenderScene(authored);},
                [&](kairo::engine::WorldCellCoordinate coordinate)
                {
                    const auto cell=m_Streaming.BuildMesh(coordinate);
                    return m_Renderer.CreateMesh(kairo::renderer::Mesh::FromArtifact(
                        cell.Mesh,{0.32f,0.63f,0.22f}));
                },
                [&](kairo::renderer::MeshHandle handle){m_Renderer.DestroyMesh(handle);});
            // If detachment or final submission fails, all reservations
            // are still settled and handles remain owned for retry/cleanup.
            if(stats.DetachFailed)
                return {std::move(plan),stats.Loaded,stats.Unloaded,
                    stats.FailedLoads,stats.FailedUnloads,stats.FirstFailure};

            auto scene=authored;
            const double extent=m_Streaming.Config().Streaming.CellSize;
            for(const auto& [coordinate,handle]:m_Store.Cells())
            {
                kairo::renderer::MeshDraw draw;
                draw.Mesh=handle;
                draw.Model=kairo::foundation::math::MakeTranslation(
                    kairo::foundation::math::Vec3f{
                        RenderFloat(coordinate.X*extent-renderOrigin.x),
                        RenderFloat(-renderOrigin.y),
                        RenderFloat(coordinate.Z*extent-renderOrigin.z)});
                // Terrain is non-pickable until a stable terrain picking ID
                // scheme is integrated with authored entity IDs.
                scene.Add(draw);
            }
            m_Renderer.SubmitRenderScene(scene);
            return {std::move(plan),stats.Loaded,stats.Unloaded,
                stats.FailedLoads,stats.FailedUnloads,stats.FirstFailure};
        }

        // Terminal operation, retryable if backend retirement fails.
        // Restore the caller's authored scene before destroying terrain.
        [[nodiscard]] std::size_t Shutdown(const kairo::renderer::RenderScene& authored) noexcept
        {
            m_Closed=true;
            return m_Store.Release([&]{m_Renderer.SubmitRenderScene(authored);},
                [&](kairo::renderer::MeshHandle handle){m_Renderer.DestroyMesh(handle);});
        }

    private:
        static float RenderFloat(double value)
        {
            if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())
                throw std::overflow_error("Terrain render-relative coordinate exceeds float range.");
            return static_cast<float>(value);
        }
        kairo::renderer::RendererRuntime& m_Renderer;
        kairo::engine::ProceduralWorldStreamingRuntime m_Streaming;
        detail::TerrainNativeStore<kairo::engine::WorldCellCoordinate,kairo::renderer::MeshHandle> m_Store;
        bool m_Closed=false;
    };
}
