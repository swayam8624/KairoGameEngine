module;

#include "TerrainNativeStore.hpp"
#include "TerrainGenerationQueue.hpp"
#include "TerrainRenderOrigin.hpp"
#include "TerrainErrorProfile.hpp"
#include "TerrainScreenError.hpp"
#include <optional>
#include <algorithm>
#include <memory>
#include <cmath>
#include <cstdio>
#include <exception>
#include <limits>
#include <span>
#include <set>
#include <cstdint>
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
        std::size_t DeferredLoads=0,StaleLoads=0,PendingLoads=0;
        std::size_t Replaced=0,FailedReplacements=0,StaleReplacements=0;
        std::size_t PendingReplacements=0,RetainedRetirements=0;
        [[nodiscard]] bool Succeeded() const noexcept
        { return FailedLoads==0 && FailedUnloads==0 && FailedReplacements==0; }
    };

    // Absolute world camera coordinates, matching the perspective render view.
    struct TerrainLodView final
    {
        kairo::foundation::math::Vec3d Position{},Forward{0,0,-1},Up{0,1,0};
        double VerticalFovRadians=1.0471975511965976,ViewportHeight=540,NearPlane=0.1;
    };

    struct TerrainBackgroundConfig final
    {
        // Zero preserves the synchronous reference path.
        std::size_t MaximumPendingMeshes=0;
        std::size_t MaximumUploadsPerUpdate=4;
        std::size_t WorkerCount=1;
        kairo::engine::TerrainLodPolicy Lod;
        std::optional<std::uint64_t> MaximumLodErrorMillimeters;
        std::optional<double> MaximumLodErrorPixels;
    };

    // Owns GPU mesh handles, not a duplicate resident CPU terrain store.
    // The renderer must outlive this bridge. Authored scene draws must not
    // borrow these handles; the bridge appends terrain each frame itself.
    class RuntimeProceduralTerrainBridge final
    {
    public:
        RuntimeProceduralTerrainBridge(kairo::renderer::RendererRuntime& renderer,
            kairo::engine::ProceduralWorldConfig config={},
            TerrainBackgroundConfig background={})
            : m_Renderer(renderer),m_Streaming(std::move(config)),m_Background(std::move(background))
        {
            if(!m_Background.Lod.SwitchDistances.empty() && m_Background.MaximumPendingMeshes==0)
                throw std::invalid_argument("Terrain LOD requires background generation.");
            (void)ChooseStride({}, {}, 0);
            if(m_Background.MaximumLodErrorPixels && (!std::isfinite(*m_Background.MaximumLodErrorPixels)||*m_Background.MaximumLodErrorPixels<=0))
                throw std::invalid_argument("Invalid terrain pixel error budget.");
            if((m_Background.MaximumLodErrorMillimeters||m_Background.MaximumLodErrorPixels) && m_Background.MaximumPendingMeshes==0)
                throw std::invalid_argument("Terrain error profiles require background generation.");
            if(m_Background.MaximumPendingMeshes!=0)
            {
                if(m_Background.MaximumUploadsPerUpdate==0)
                    throw std::invalid_argument("Background terrain upload budget must be positive.");
                const auto validated=m_Streaming.Config();
                const auto budget=m_Background.MaximumLodErrorMillimeters;
                const bool profileRequired=budget.has_value()||m_Background.MaximumLodErrorPixels.has_value();
                const auto maximumStride=std::uint32_t{1}<<m_Background.Lod.SwitchDistances.size();
                m_Queue=std::make_unique<Queue>(m_Background.MaximumPendingMeshes,
                    [validated,budget,maximumStride,profileRequired](Job job)
                    {
                        GeneratedMesh result;
                        result.Stride=job.Stride;
                        if(profileRequired && !job.Replacement)
                        {
                            result.Profile=detail::BuildTerrainErrorProfile(maximumStride,
                                [&](std::uint32_t stride)
                                {
                                    return kairo::engine::MeasureSeededTerrainLodErrorMillimeters(
                                        validated.Seed,job.Cell,validated.Terrain,stride);
                                });
                            if(budget)result.Stride=result.Profile->Choose(job.Stride,*budget);
                        }
                        result.Cell=kairo::engine::BuildSeededTerrainLodMeshCell(validated.Seed,job.Cell,
                            validated.Terrain,validated.SampleSpacingMillimeters,result.Stride);
                        return result;
                    },m_Background.WorkerCount);
            }
        }
        RuntimeProceduralTerrainBridge(const RuntimeProceduralTerrainBridge&)=delete;
        RuntimeProceduralTerrainBridge& operator=(const RuntimeProceduralTerrainBridge&)=delete;

        ~RuntimeProceduralTerrainBridge() noexcept
        {
            const auto failures=Shutdown(kairo::renderer::RenderScene{});
            if(failures)std::fprintf(stderr,"Terrain bridge could not release %zu mesh handles.\n",failures);
        }

        [[nodiscard]] std::size_t ResidentCount() const noexcept {return m_Store.Cells().size();}
        [[nodiscard]] std::size_t PendingCount() const {return m_Queue?m_Queue->Occupied():0u;}
        [[nodiscard]] std::size_t PendingLoadCount() const {return PendingCount()-m_PendingReplacements.size();}
        [[nodiscard]] std::size_t PendingReplacementCount() const noexcept {return m_PendingReplacements.size();}
        [[nodiscard]] bool IsFullDetailResident(kairo::engine::WorldCellCoordinate coordinate) const
        {
            const auto found=m_Store.Cells().find(coordinate);
            return found!=m_Store.Cells().end()&&found->second.Stride==1u;
        }
        [[nodiscard]] std::size_t ResidentLodCount(std::uint32_t stride) const noexcept
        {
            std::size_t count=0;
            for(const auto& [coordinate,mesh]:m_Store.Cells())
            { (void)coordinate; if(mesh.Stride==stride)++count; }
            return count;
        }
        [[nodiscard]] std::size_t ResidentErrorProfileCount() const noexcept
        {
            std::size_t count=0;
            for(const auto& [coordinate,mesh]:m_Store.Cells())
            { (void)coordinate; if(mesh.Profile)++count; }
            return count;
        }

        [[nodiscard]] std::uint64_t MaximumResidentLodErrorMillimeters() const
        {
            std::uint64_t maximum=0;
            for(const auto& [coordinate,mesh]:m_Store.Cells())
            {
                (void)coordinate;
                if(mesh.Profile)maximum=std::max(maximum,mesh.Profile->Error(mesh.Stride));
            }
            return maximum;
        }
        [[nodiscard]] double MaximumResidentProjectedErrorPixels(const TerrainLodView& view) const
        {
            const auto prepared=PrepareView(view);
            double maximum=0;
            for(const auto& [coordinate,mesh]:m_Store.Cells())
                if(mesh.Profile)maximum=std::max(maximum,ProjectedError(coordinate,*mesh.Profile,mesh.Stride,prepared));
            return maximum;
        }
        [[nodiscard]] const kairo::engine::ProceduralWorldStreamingRuntime& Streaming() const noexcept
        {return m_Streaming;}

        // Observer coordinates are absolute doubles. The supplied authored
        // scene and camera must already use the same render origin as terrain.
        // Default origin zero preserves existing small-world behaviour.
        [[nodiscard]] RuntimeProceduralTerrainUpdate UpdateAndSubmit(
            std::span<const kairo::engine::WorldStreamingObserver> observers,
            const kairo::renderer::RenderScene& authored,
            const kairo::foundation::math::Vec3d& renderOrigin={},const TerrainLodView* view=nullptr)
        {
            if(m_Closed)throw std::logic_error("Terrain bridge has been shut down.");
            if(!std::isfinite(renderOrigin.x)||!std::isfinite(renderOrigin.y)||!std::isfinite(renderOrigin.z))
                throw std::invalid_argument("Terrain render origin must be finite.");
            std::optional<detail::PreparedTerrainView> prepared;
            if(m_Background.MaximumLodErrorPixels)
            {
                if(!view)throw std::invalid_argument("Pixel error policy requires the current camera view.");
                prepared=PrepareView(*view);
            }
            auto plan=m_Streaming.PlanUpdate(observers);
            if(m_Queue)return UpdateBackground(std::move(plan),observers,authored,renderOrigin,prepared);
            const auto stats=detail::ApplyTerrainNativePlan(m_Store,m_Streaming,plan,
                [&]{m_Renderer.SubmitRenderScene(authored);},
                [&](kairo::engine::WorldCellCoordinate coordinate)
                {
                    const auto cell=m_Streaming.BuildMesh(coordinate);
                    return NativeMesh{m_Renderer.CreateMesh(kairo::renderer::Mesh::FromArtifact(
                        cell.Mesh,{0.32f,0.63f,0.22f})),1u,{}};
                },
                [&](NativeMesh mesh){m_Renderer.DestroyMesh(mesh.Handle);});
            // If detachment or final submission fails, all reservations
            // are still settled and handles remain owned for retry/cleanup.
            if(stats.DetachFailed)
                return {std::move(plan),stats.Loaded,stats.Unloaded,
                    stats.FailedLoads,stats.FailedUnloads,stats.FirstFailure};

            SubmitResident(authored,renderOrigin);
            return {std::move(plan),stats.Loaded,stats.Unloaded,
                stats.FailedLoads,stats.FailedUnloads,stats.FirstFailure};
        }

        // Terminal operation, retryable if backend retirement fails.
        // Join the worker before destroying renderer handles or configuration.
        [[nodiscard]] std::size_t Shutdown(const kairo::renderer::RenderScene& authored) noexcept
        {
            m_Closed=true;
            m_Queue.reset();
            m_PendingReplacements.clear();
            return m_Store.Release([&]{m_Renderer.SubmitRenderScene(authored);},
                [&](NativeMesh mesh){m_Renderer.DestroyMesh(mesh.Handle);});
        }

    private:
        struct NativeMesh {kairo::renderer::MeshHandle Handle{};std::uint32_t Stride=1u;
            std::optional<detail::TerrainErrorProfile> Profile;};
        struct Job {kairo::engine::WorldCellCoordinate Cell;std::uint32_t Stride=1u;bool Replacement=false;};
        struct GeneratedMesh {kairo::engine::SeededTerrainMeshCell Cell;std::uint32_t Stride=1u;
            std::optional<detail::TerrainErrorProfile> Profile;};
        using Queue=detail::TerrainGenerationQueue<Job,GeneratedMesh>;

        [[nodiscard]] std::uint32_t ChooseStride(kairo::engine::WorldCellCoordinate cell,
            std::span<const kairo::engine::WorldStreamingObserver> observers,std::uint32_t current) const
        {
            const auto& cfg=m_Streaming.Config();
            return kairo::engine::SelectSeededTerrainLodStride(cell,cfg.Streaming.CellSize,
                cfg.Terrain.Intervals,observers,m_Background.Lod,current);
        }

        static detail::PreparedTerrainView PrepareView(const TerrainLodView& view)
        {
            return detail::PrepareTerrainView({{view.Position.x,view.Position.y,view.Position.z},
                {view.Forward.x,view.Forward.y,view.Forward.z},{view.Up.x,view.Up.y,view.Up.z},
                view.VerticalFovRadians,view.ViewportHeight,view.NearPlane});
        }

        [[nodiscard]] double ProjectedError(kairo::engine::WorldCellCoordinate cell,
            const detail::TerrainErrorProfile& profile,std::uint32_t stride,
            const detail::PreparedTerrainView& view) const
        {
            const auto& cfg=m_Streaming.Config();
            const double extent=cfg.Streaming.CellSize,x=cell.X*extent,z=cell.Z*extent;
            return detail::TerrainProjectedErrorPixels(view,
                {x,cfg.Terrain.MinimumMillimeters/1000.0,z},
                {x+extent,cfg.Terrain.MaximumMillimeters/1000.0,z+extent},
                profile.Error(stride)/1000.0);
        }

        [[nodiscard]] std::uint32_t DesiredResidentStride(kairo::engine::WorldCellCoordinate cell,
            std::span<const kairo::engine::WorldStreamingObserver> observers,const NativeMesh& mesh,
            const std::optional<detail::PreparedTerrainView>& view) const
        {
            auto requested=ChooseStride(cell,observers,mesh.Stride);
            if(mesh.Profile && m_Background.MaximumLodErrorMillimeters)
                requested=mesh.Profile->Choose(requested,*m_Background.MaximumLodErrorMillimeters);
            if(mesh.Profile && view && m_Background.MaximumLodErrorPixels)
            {
                // Check both independent constraints at every candidate;
                // measured geometric errors are not necessarily monotone.
                for(auto stride=requested;stride>1;stride/=2)
                {
                    const auto error=mesh.Profile->Error(stride);
                    if(m_Background.MaximumLodErrorMillimeters&&error>*m_Background.MaximumLodErrorMillimeters)continue;
                    const double limit=*m_Background.MaximumLodErrorPixels*(stride>mesh.Stride?0.8:1.0);
                    if(ProjectedError(cell,*mesh.Profile,stride,*view)<=limit)return stride;
                }
                return 1;
            }
            return requested;
        }

        [[nodiscard]] bool WantsKeep(kairo::engine::WorldCellCoordinate coordinate,
            std::span<const kairo::engine::WorldStreamingObserver> observers) const noexcept
        {
            const auto& config=m_Streaming.Config().Streaming;
            const double x=coordinate.X*config.CellSize,z=coordinate.Z*config.CellSize;
            for(const auto& observer:observers)
            {
                const double dx=std::max({x-observer.Position.x,0.0,
                    observer.Position.x-(x+config.CellSize)});
                const double dz=std::max({z-observer.Position.z,0.0,
                    observer.Position.z-(z+config.CellSize)});
                const double radius=config.KeepRadius*observer.RadiusScale;
                if(dx*dx+dz*dz<=radius*radius)return true;
            }
            return false;
        }

        RuntimeProceduralTerrainUpdate UpdateBackground(kairo::engine::WorldStreamingPlan plan,
            std::span<const kairo::engine::WorldStreamingObserver> observers,
            const kairo::renderer::RenderScene& authored,
            const kairo::foundation::math::Vec3d& renderOrigin,
            const std::optional<detail::PreparedTerrainView>& view)
        {
            RuntimeProceduralTerrainUpdate result;
            result.Plan=std::move(plan);
            const auto failure=[&]{if(!result.FirstFailure)result.FirstFailure=std::current_exception();};
            if(!result.Plan.Unloads.empty())
            {
                try{m_Renderer.SubmitRenderScene(authored);}
                catch(...)
                {
                    failure();
                    for(const auto& request:result.Plan.Unloads)
                        m_Streaming.CompleteUnload(request.Coordinate,false);
                    for(const auto& request:result.Plan.Loads)
                        m_Streaming.CompleteLoad(request.Coordinate,false);
                    result.FailedUnloads=result.Plan.Unloads.size();
                    result.FailedLoads=result.Plan.Loads.size();
                    result.PendingLoads=PendingLoadCount();
                    result.PendingReplacements=PendingReplacementCount();
                    return result;
                }
            }
            for(const auto& request:result.Plan.Unloads)
            {
                bool ok=false;
                try{m_Store.Unload(request.Coordinate,[&](auto handle){m_Renderer.DestroyMesh(handle.Handle);});ok=true;}
                catch(...){failure();}
                m_Streaming.CompleteUnload(request.Coordinate,ok);
                if(ok)++result.Unloaded;else ++result.FailedUnloads;
            }
            // Reclaim obsolete queued reservations before consuming ready
            // meshes. Running jobs retain ownership until they complete.
            while(auto cancelled=m_Queue->CancelQueued(
                [&](Job job){return !WantsKeep(job.Cell,observers);}))
            {
                if(cancelled->Replacement)
                {
                    m_PendingReplacements.erase(cancelled->Cell);
                    ++result.StaleReplacements;
                }
                else
                {
                    m_Streaming.CompleteLoad(cancelled->Cell,false);
                    ++result.StaleLoads;
                }
            }
            // Bound completion processing, including failed and stale work.
            // A camera teleport can invalidate a mesh while it is generated;
            // do not upload that mesh merely to retire it next frame.
            for(std::size_t i=0;i<m_Background.MaximumUploadsPerUpdate;++i)
            {
                auto ready=m_Queue->TryTake();
                if(!ready)break;
                const auto job=ready->Cell;
                if(job.Replacement)
                {
                    m_PendingReplacements.erase(job.Cell);
                    const auto found=m_Store.Cells().find(job.Cell);
                    if(found==m_Store.Cells().end()||!WantsKeep(job.Cell,observers)||
                        DesiredResidentStride(job.Cell,observers,found->second,view)!=job.Stride||
                        found->second.Stride==job.Stride)
                    {++result.StaleReplacements;continue;}
                    if(ready->Failure)
                    {
                        ++result.FailedReplacements;
                        if(!result.FirstFailure)result.FirstFailure=ready->Failure;
                        continue;
                    }
                    try
                    {
                        if(!ready->Value)throw std::logic_error("Terrain worker returned no LOD mesh.");
                        if(m_Store.Replace(job.Cell,[&](auto){
                            return NativeMesh{m_Renderer.CreateMesh(kairo::renderer::Mesh::FromArtifact(
                                ready->Value->Cell.Mesh,{0.32f,0.63f,0.22f})),job.Stride,found->second.Profile};
                        }))++result.Replaced;
                    }
                    catch(...){failure();++result.FailedReplacements;}
                    continue;
                }
                bool ok=false;
                if(!WantsKeep(job.Cell,observers))
                    ++result.StaleLoads;
                else if(ready->Failure)
                {
                    ++result.FailedLoads;
                    if(!result.FirstFailure)result.FirstFailure=ready->Failure;
                }
                else
                {
                    try
                    {
                        if(!ready->Value)throw std::logic_error("Terrain worker returned no mesh.");
                        m_Store.Load(job.Cell,[&](auto)
                        {
                            return NativeMesh{m_Renderer.CreateMesh(kairo::renderer::Mesh::FromArtifact(
                                ready->Value->Cell.Mesh,{0.32f,0.63f,0.22f})),ready->Value->Stride,ready->Value->Profile};
                        });
                        ok=true;++result.Loaded;
                    }
                    catch(...){failure();++result.FailedLoads;}
                }
                m_Streaming.CompleteLoad(job.Cell,ok);
            }
            for(const auto& request:result.Plan.Loads)
            {
                bool accepted=false;
                try
                {
                    accepted=m_Queue->TrySubmit({request.Coordinate,view?1u:ChooseStride(request.Coordinate,observers,0),false});
                    if(!accepted)++result.DeferredLoads;
                }
                catch(...){failure();++result.FailedLoads;}
                if(!accepted)m_Streaming.CompleteLoad(request.Coordinate,false);
            }
            // Loads get first use of the shared queue. Existing resident
            // meshes stay visible while replacement jobs run.
            if(!m_Background.Lod.SwitchDistances.empty())
                for(const auto& [coordinate,mesh]:m_Store.Cells())
                {
                    if(PendingCount()>=m_Background.MaximumPendingMeshes)break;
                    if(m_PendingReplacements.contains(coordinate)||!WantsKeep(coordinate,observers))continue;
                    const auto stride=DesiredResidentStride(coordinate,observers,mesh,view);
                    if(stride==mesh.Stride)continue;
                    try
                    {
                        m_PendingReplacements.insert(coordinate);
                        if(!m_Queue->TrySubmit({coordinate,stride,true}))
                            m_PendingReplacements.erase(coordinate);
                    }
                    catch(...)
                    {
                        m_PendingReplacements.erase(coordinate);
                        failure();++result.FailedReplacements;
                    }
                }
            result.PendingLoads=PendingLoadCount();
            result.PendingReplacements=PendingReplacementCount();
            SubmitResident(authored,renderOrigin);
            // No submitted draw references any old LOD handle after this
            // point. Retain failed retirements for retry in the next update.
            result.RetainedRetirements=m_Store.ReleaseRetired(
                [&](NativeMesh mesh){m_Renderer.DestroyMesh(mesh.Handle);});
            return result;
        }

        void SubmitResident(const kairo::renderer::RenderScene& authored,
            const kairo::foundation::math::Vec3d& renderOrigin)
        {
            auto scene=authored;
            const double extent=m_Streaming.Config().Streaming.CellSize;
            for(const auto& [coordinate,handle]:m_Store.Cells())
            {
                kairo::renderer::MeshDraw draw;
                draw.Mesh=handle.Handle;
                draw.Model=kairo::foundation::math::MakeTranslation(
                    kairo::foundation::math::Vec3f{
                        detail::TerrainRelativeFloat(coordinate.X*extent,renderOrigin.x),
                        detail::TerrainRelativeFloat(0.0,renderOrigin.y),
                        detail::TerrainRelativeFloat(coordinate.Z*extent,renderOrigin.z)});
                // Terrain is non-pickable until a stable terrain picking ID
                // scheme is integrated with authored entity IDs.
                scene.Add(draw);
            }
            m_Renderer.SubmitRenderScene(scene);
        }

        kairo::renderer::RendererRuntime& m_Renderer;
        kairo::engine::ProceduralWorldStreamingRuntime m_Streaming;
        detail::TerrainNativeStore<kairo::engine::WorldCellCoordinate,NativeMesh> m_Store;
        std::set<kairo::engine::WorldCellCoordinate> m_PendingReplacements;
        TerrainBackgroundConfig m_Background;
        std::unique_ptr<Queue> m_Queue;
        bool m_Closed=false;
    };
}
