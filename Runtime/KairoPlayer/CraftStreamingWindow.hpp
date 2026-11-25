#pragma once
#include "CraftWorld.hpp"
#include <optional>
#include <array>

namespace kairo::player::detail
{
    // World v2: unbounded in horizontal directions within the signed
    // coordinate domain; one editable vertical band for the first slice.
    // Same integer terrain rule as the finite v1 sample, without its X/Z clip.
    // Same seed/tile rule as CraftBaseline, but horizontally unbounded within
    // the signed voxel coordinate type. The vertical band remains finite.
    struct StreamingCraftBaseline
    {
        std::uint64_t Seed=42;
        // Pure world occupancy function. Int64 arithmetic handles negative
        // extreme coordinates without overflowing when adjusting floor division.
        VoxelMaterial operator()(VoxelCoordinate c) const noexcept
        {
            if(c.Y<0||c.Y>=CraftExtent)return VoxelAir;
            const auto tile=[](std::int32_t x)
            {
                const auto wide=static_cast<std::int64_t>(x);
                return wide>=0?wide/4:(wide-3)/4;
            };
            const auto h=CraftHash(Seed^CraftHash(static_cast<std::uint64_t>(tile(c.X)))^
                CraftHash(static_cast<std::uint64_t>(tile(c.Z))+0x4cf5ad432745937fULL));
            const int top=3+static_cast<int>(h%4);
            if(c.Y>top)return VoxelAir;
            if(c.Y==top)return CraftGrass;
            return c.Y>=top-2?CraftDirt:CraftStone;
        }
    };

    // Identity of a vertical column in chunk units, not world meters. Default
    // comparison enables deterministic map/set ordering when used as a key.
    struct CraftColumn
    {
        std::int32_t X=0,Z=0;
        auto operator<=>(const CraftColumn&) const=default;
    };
    // Convert one absolute voxel axis to a column by floor division. C++ signed
    // division truncates toward zero, so widen and correct negative values.
    inline std::int32_t CraftColumnIndex(std::int32_t coordinate) noexcept
    {
        const auto wide=static_cast<std::int64_t>(coordinate);
        return static_cast<std::int32_t>(wide>=0?wide/CraftExtent:(wide-(CraftExtent-1))/CraftExtent);
    }
    // Resolve a voxel's owning column from X/Z; Y stays within the same band.
    inline CraftColumn CraftOwner(VoxelCoordinate voxel) noexcept
    {return {CraftColumnIndex(voxel.X),CraftColumnIndex(voxel.Z)};}
    // Multiply in int64, then check both the interior and the one-cell halo.
    // BuildVoxelChunkSurface will sample that halo, so checking only origin
    // would allow overflow at the positive/negative edge.
    inline VoxelCoordinate StreamingCraftOrigin(CraftColumn cell)
    {
        const auto x=static_cast<std::int64_t>(cell.X)*CraftExtent;
        const auto z=static_cast<std::int64_t>(cell.Z)*CraftExtent;
        constexpr auto lo=std::numeric_limits<std::int32_t>::min();
        constexpr auto hi=std::numeric_limits<std::int32_t>::max();
        if(x-1<lo||x+CraftExtent>hi||z-1<lo||z+CraftExtent>hi)
            throw std::out_of_range("Streaming Craft chunk halo out of range.");
        return {static_cast<std::int32_t>(x),0,static_cast<std::int32_t>(z)};
    }
    // The camera/player position is continuous. Floor to a voxel before
    // mapping to a column, retaining the half-open ownership convention.
    inline CraftColumn CraftObserverColumn(double x,double z)
    {
        const auto convert=[](double value)
        {
            if(!std::isfinite(value)||value<std::numeric_limits<std::int32_t>::min()||
                value>=static_cast<double>(std::numeric_limits<std::int32_t>::max())+1)
                throw std::out_of_range("Streaming Craft observer out of range.");
            return CraftColumnIndex(static_cast<std::int32_t>(std::floor(value)));
        };
        return {convert(x),convert(z)};
    }
    // Bounded square residency plan. Optional slots distinguish not-yet-loaded
    // chunks from real column (0,0). Dirty means the slot requires new mesh data;
    // retained/loaded/evicted are accounting, not native resource ownership.
    template<unsigned Radius>
    struct CraftWindowPlan
    {
        static_assert(Radius>=1&&Radius<=8);
        static constexpr std::size_t Width=2*Radius+1,Capacity=Width*Width;
        using Slots=std::array<std::optional<CraftColumn>,Capacity>;
        Slots Next{};
        std::array<bool,Capacity> Dirty{};
        std::size_t Retained=0,Loaded=0,Evicted=0;
    };

    // Stable slot reuse: keep every overlapping chunk at the same slot;
    // only vacant/outdated slots acquire newly desired coordinates.
    // Pure planning permits a native transaction to fail without changing
    // the committed window. Caller commits Next only after publication.
    template<unsigned Radius>
    // Input is the previously committed slot array. Output is a candidate
    // array preserving overlap indices. CraftMeshBatch and VoxelMeshPublication
    // produce/upload the dirty slots before the host commits this plan.
    CraftWindowPlan<Radius> PlanCraftWindow(
        const typename CraftWindowPlan<Radius>::Slots& current,CraftColumn center)
    {
        using Plan=CraftWindowPlan<Radius>;
        Plan plan;
        std::array<CraftColumn,Plan::Capacity> desired{};
        std::size_t count=0;
        for(int dz=-static_cast<int>(Radius);dz<=static_cast<int>(Radius);++dz)
            for(int dx=-static_cast<int>(Radius);dx<=static_cast<int>(Radius);++dx)
            {
                const auto x=static_cast<std::int64_t>(center.X)+dx,z=static_cast<std::int64_t>(center.Z)+dz;
                if(x<std::numeric_limits<std::int32_t>::min()||x>std::numeric_limits<std::int32_t>::max()||
                    z<std::numeric_limits<std::int32_t>::min()||z>std::numeric_limits<std::int32_t>::max())
                    throw std::out_of_range("Streaming Craft window coordinate overflow.");
                const CraftColumn cell{static_cast<std::int32_t>(x),static_cast<std::int32_t>(z)};
                (void)StreamingCraftOrigin(cell);
                desired[count++]=cell;
            }
        // Mark each desired coordinate once. Reject duplicate committed slots
        // rather than allowing two GPU resources to represent one column.
        std::array<bool,Plan::Capacity> assigned{};
        for(std::size_t i=0;i<current.size();++i)if(current[i])
        {
            const auto found=std::find(desired.begin(),desired.end(),*current[i]);
            if(found==desired.end()){++plan.Evicted;continue;}
            const auto j=static_cast<std::size_t>(found-desired.begin());
            if(assigned[j])throw std::invalid_argument("Duplicate committed Craft chunk.");
            assigned[j]=true;plan.Next[i]=current[i];++plan.Retained;
        }
        // Every retained slot consumes exactly one desired coordinate, leaving
        // the same number of unassigned coordinates as empty candidate slots.
        std::size_t next=0;
        for(std::size_t i=0;i<plan.Next.size();++i)if(!plan.Next[i])
        {
            while(assigned[next])++next;
            plan.Next[i]=desired[next];assigned[next]=true;
            plan.Dirty[i]=true;++plan.Loaded;
        }
        return plan;
    }

    // Map an edited voxel to the resident slots whose meshing halo intersects
    // it. This only identifies invalidation; it does not schedule uploads.
    template<std::size_t N>
    std::array<bool,N> StreamingCraftDirty(
        const std::array<std::optional<CraftColumn>,N>& slots,VoxelCoordinate voxel)
    {
        std::array<bool,N> dirty{};
        if(voxel.Y<0||voxel.Y>=CraftExtent)return dirty;
        for(std::size_t i=0;i<N;++i)if(slots[i])
        {
            const auto origin=StreamingCraftOrigin(*slots[i]);
            const auto distance=[](std::int64_t p,std::int64_t a)
            {return std::max({a-p,std::int64_t{0},p-(a+CraftExtent-1)});};
            dirty[i]=distance(voxel.X,origin.X)+distance(voxel.Z,origin.Z)<=1;
        }
        return dirty;
    }
}
