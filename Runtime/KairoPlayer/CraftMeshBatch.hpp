#pragma once
#include "CraftStreamingWindow.hpp"
#include "TerrainGenerationQueue.hpp"
#include <memory>

namespace kairo::player::detail
{
    // CPU-only batch over immutable occupancy. The renderer can continue
    // drawing the committed window until every dirty chunk is ready.
    // A new window or edit invalidates the entire previous batch; running
    // workers retain their snapshot and obsolete completions are discarded.
    template<unsigned Radius,class PlanType=CraftWindowPlan<Radius>,class Baseline=StreamingCraftBaseline,class Cell=CraftColumn>
    class CraftMeshBatch final
    {
        using Plan=PlanType;
        using World=VoxelEdits<Baseline>;
        struct Job
        {
            std::uint64_t Generation=0;
            std::size_t Slot=0;
            Cell Coordinate;
            std::shared_ptr<const World> Snapshot;
        };
        using Queue=TerrainGenerationQueue<Job,CraftMeshData>;
    public:
        explicit CraftMeshBatch(std::size_t workers,std::size_t capacity=8)
            :m_Queue(capacity,[](Job job){
                return BuildCraftMesh(BuildVoxelChunkSurface(StreamingCraftOrigin(job.Coordinate),CraftExtent,
                    [&](VoxelCoordinate c){return job.Snapshot->Get(c);}));
            },workers) {}
        void Cancel()
        {
            // Never allow generation identifiers to wrap while an old job
            // might still be running.
            if(m_Generation==std::numeric_limits<std::uint64_t>::max())
                throw std::overflow_error("Craft generation identifier exhausted.");
            ++m_Generation;
            m_Plan.reset();m_Snapshot.reset();
            m_Submitted.fill(false);
            for(auto& result:m_Results)result.reset();
            while(m_Queue.CancelQueued([](const Job&){return true;}))++m_Stale;
        }
        void Begin(const Plan& plan,std::shared_ptr<const World> snapshot)
        {
            if(!snapshot)throw std::invalid_argument("Missing Craft snapshot.");
            for(std::size_t i=0;i<Plan::Capacity;++i)
                if(plan.Dirty[i]&&!plan.Next[i])throw std::invalid_argument("Dirty Craft slot has no coordinate.");
            Cancel();
            m_Plan=plan;m_Snapshot=std::move(snapshot);
        }
        // Nonblocking: only the fixed queue and at most Capacity CPU mesh
        // results are owned. Failure propagates on the consumer thread.
        bool Poll()
        {
            while(auto completion=m_Queue.TryTake())
            {
                if(!m_Plan||completion->Cell.Generation!=m_Generation){++m_Stale;continue;}
                if(completion->Failure)std::rethrow_exception(completion->Failure);
                if(!completion->Value)throw std::logic_error("Missing Craft mesh completion.");
                m_Results[completion->Cell.Slot]=std::move(*completion->Value);
            }
            if(!m_Plan)return false;
            bool ready=true;
            for(std::size_t i=0;i<Plan::Capacity;++i)if(m_Plan->Dirty[i]&&!m_Results[i])
            {
                ready=false;
                if(!m_Submitted[i])
                    m_Submitted[i]=m_Queue.TrySubmit({m_Generation,i,*m_Plan->Next[i],m_Snapshot});
            }
            return ready;
        }
        CraftMeshData Take(std::size_t i)
        {
            if(i>=Plan::Capacity||!m_Results[i])throw std::logic_error("Craft mesh not ready.");
            auto result=std::move(*m_Results[i]);m_Results[i].reset();
            return result;
        }
        [[nodiscard]] std::size_t Occupied() const{return m_Queue.Occupied();}
        [[nodiscard]] std::size_t StaleCount() const noexcept{return m_Stale;}
    private:
        std::uint64_t m_Generation=0;
        std::size_t m_Stale=0;
        std::optional<Plan> m_Plan;
        std::shared_ptr<const World> m_Snapshot;
        std::array<bool,Plan::Capacity> m_Submitted{};
        std::array<std::optional<CraftMeshData>,Plan::Capacity> m_Results{};
        Queue m_Queue; // destroyed first; joins workers before snapshots/results
    };
}
