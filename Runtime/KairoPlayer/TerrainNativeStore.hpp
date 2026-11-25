#pragma once
#include <cstddef>
#include <exception>
#include <map>
#include <stdexcept>
#include <utility>
#include <type_traits>
#include <vector>

namespace kairo::player::detail
{
    struct TerrainNativeStats
    {
        std::size_t Loaded=0,Unloaded=0,FailedLoads=0,FailedUnloads=0;
        std::exception_ptr FirstFailure;
        bool DetachFailed=false;
    };

    // A mesh handle belongs to exactly one cell. Allocate the journal node
    // BEFORE calling the native factory, so a successful upload cannot be
    // lost to a subsequent map allocation failure.
    template<class Coordinate,class Handle>
    class TerrainNativeStore final
    {
    public:
        static_assert(std::is_nothrow_copy_constructible_v<Handle>);
        static_assert(std::is_nothrow_copy_assignable_v<Handle>);
        explicit TerrainNativeStore(std::size_t retirementCapacity=32)
            : m_RetirementCapacity(retirementCapacity)
        {m_Retired.reserve(retirementCapacity);}

        // Upload first, publish the replacement only on success. The old
        // handle stays owned until a successful submission no longer uses it.
        // No allocation after Create returns a native resource.
        template<class Create>
        bool Replace(Coordinate coordinate,Create&& create)
        {
            const auto it=m_Cells.find(coordinate);
            if(it==m_Cells.end())throw std::logic_error("Terrain cell has no mesh to replace.");
            if(m_Retired.size()==m_RetirementCapacity)return false;
            const Handle replacement=create(coordinate);
            m_Retired.push_back(it->second);
            it->second=replacement;
            return true;
        }

        // Only call after a scene submission that excludes ALL retired
        // handles. A failing destroy leaves its handle in the journal.
        template<class Destroy>
        std::size_t ReleaseRetired(Destroy&& destroy) noexcept
        {
            std::size_t retained=0;
            for(const auto& handle:m_Retired)
            {
                try{destroy(handle);}
                catch(...){m_Retired[retained++]=handle;}
            }
            m_Retired.resize(retained);
            return retained;
        }
        [[nodiscard]] std::size_t RetiredCount() const noexcept {return m_Retired.size();}

        template<class Create>
        void Load(Coordinate coordinate,Create&& create)
        {
            const auto [it,inserted]=m_Cells.try_emplace(coordinate);
            if(!inserted)throw std::logic_error("Terrain cell already owns a mesh.");
            try { it->second=create(coordinate); }
            catch(...) { m_Cells.erase(it); throw; }
        }

        template<class Destroy>
        void Unload(Coordinate coordinate,Destroy&& destroy)
        {
            const auto it=m_Cells.find(coordinate);
            if(it==m_Cells.end())throw std::logic_error("Terrain cell has no native mesh.");
            destroy(it->second); // keep ownership on failure for retry
            m_Cells.erase(it);
        }

        [[nodiscard]] const std::map<Coordinate,Handle>& Cells() const noexcept
        { return m_Cells; }

        template<class Detach,class Destroy>
        std::size_t Release(Detach&& detach,Destroy&& destroy) noexcept
        {
            if(m_Cells.empty()&&m_Retired.empty())return 0;
            try{detach();}catch(...){return m_Cells.size()+m_Retired.size();}
            ReleaseRetired(destroy);
            for(auto it=m_Cells.begin();it!=m_Cells.end();)
            {
                try{destroy(it->second);it=m_Cells.erase(it);}
                catch(...){++it;}
            }
            return m_Cells.size()+m_Retired.size();
        }

    private:
        std::map<Coordinate,Handle> m_Cells;
        std::vector<Handle> m_Retired;
        std::size_t m_RetirementCapacity;
    };

    // The backend may reject deleting a mesh referenced by its most recent
    // submitted scene. Detach once before any unload; if detachment fails,
    // acknowledge every request as failed and preserve all owned handles.
    template<class Store,class Runtime,class Plan,class Detach,class Create,class Destroy>
    TerrainNativeStats ApplyTerrainNativePlan(Store& store,Runtime& runtime,
        const Plan& plan,Detach&& detach,Create&& create,Destroy&& destroy)
    {
        TerrainNativeStats stats;
        const auto failure=[&]{if(!stats.FirstFailure)stats.FirstFailure=std::current_exception();};
        if(!plan.Unloads.empty())
        {
            try{detach();}
            catch(...)
            {
                failure();
                stats.DetachFailed=true;
                for(const auto& request:plan.Unloads)
                    runtime.CompleteUnload(request.Coordinate,false);
                for(const auto& request:plan.Loads)
                    runtime.CompleteLoad(request.Coordinate,false);
                stats.FailedUnloads=plan.Unloads.size();
                stats.FailedLoads=plan.Loads.size();
                return stats;
            }
        }
        for(const auto& request:plan.Unloads)
        {
            bool ok=false;
            try{store.Unload(request.Coordinate,destroy);ok=true;}
            catch(...){failure();}
            runtime.CompleteUnload(request.Coordinate,ok);
            if(ok)++stats.Unloaded;else ++stats.FailedUnloads;
        }
        for(const auto& request:plan.Loads)
        {
            bool ok=false;
            try{store.Load(request.Coordinate,create);ok=true;}
            catch(...){failure();}
            runtime.CompleteLoad(request.Coordinate,ok);
            if(ok)++stats.Loaded;else ++stats.FailedLoads;
        }
        return stats;
    }
}
