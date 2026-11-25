#pragma once
#include "CraftStreamingWindow.hpp"

namespace kairo::player::detail
{
    inline constexpr int CaveBottom=-64,CaveTop=32;
    struct CaveChunk
    {
        std::int32_t X=0,Y=0,Z=0;
        auto operator<=>(const CaveChunk&) const=default;
    };
    inline CaveChunk CaveOwner(VoxelCoordinate c) noexcept
    {return {CraftColumnIndex(c.X),CraftColumnIndex(c.Y),CraftColumnIndex(c.Z)};}
    inline VoxelCoordinate StreamingCraftOrigin(CaveChunk c)
    {
        if(c.Y<-4||c.Y>1)throw std::out_of_range("Cave chunk layer.");
        auto origin=StreamingCraftOrigin(CraftColumn{c.X,c.Z});
        origin.Y=c.Y*CraftExtent;
        return origin;
    }
    inline bool InCaveWorld(VoxelCoordinate c) noexcept
    {
        if(c.Y<CaveBottom||c.Y>=CaveTop)return false;
        const auto k=CaveOwner(c);
        constexpr auto lo=std::numeric_limits<std::int32_t>::min();
        constexpr auto hi=std::numeric_limits<std::int32_t>::max();
        const auto x=std::int64_t(k.X)*16,z=std::int64_t(k.Z)*16;
        return x-1>=lo&&z-1>=lo&&x+16<=hi&&z+16<=hi;
    }
    // Integer modular arithmetic is independent of chunk order and
    // platform floating-point behavior. Interconnected galleries occur
    // at four depths; shafts at lattice intersections join levels.
    // Seed-dependent entrances and chambers break the regular structure.
    struct CaveCraftBaseline
    {
        std::uint64_t Seed=42;
        VoxelMaterial operator()(VoxelCoordinate c) const noexcept
        {
            if(c.Y<CaveBottom||c.Y>=CaveTop)return VoxelAir;
            const auto mod=[](std::int32_t v){const int m=v%16;return m<0?m+16:m;};
            const int x=mod(c.X),z=mod(c.Z),y=mod(c.Y);
            const bool shaft=std::abs(x-8)<=1&&std::abs(z-8)<=1;
            const auto column=CraftHash(Seed^CraftHash(static_cast<std::uint64_t>(CraftColumnIndex(c.X)))^
                CraftHash(static_cast<std::uint64_t>(CraftColumnIndex(c.Z))+0x8932abULL));
            if(c.Y>=0)
            {
                if(shaft&&column%3==0)return VoxelAir;
                return StreamingCraftBaseline{Seed}(c);
            }
            if(c.Y==CaveBottom)return CraftStone;
            if(shaft)return VoxelAir;
            if(std::abs(y-8)<=1&&(std::abs(x-8)<=1||std::abs(z-8)<=1))return VoxelAir;
            const auto chamber=CraftHash(column^CraftHash(static_cast<std::uint64_t>(CraftColumnIndex(c.Y))));
            const int radius=3+static_cast<int>(chamber%3);
            const int dx=x-8,dy=y-8,dz=z-8;
            if(dx*dx+dy*dy+dz*dz<=radius*radius)return VoxelAir;
            return CraftStone;
        }
    };
    inline CaveChunk CaveObserver(double x,double y,double z)
    {
        const auto c=CraftObserverColumn(x,z);
        if(!std::isfinite(y))throw std::invalid_argument("Invalid cave observer height.");
        const int layer=CraftColumnIndex(static_cast<std::int32_t>(std::floor(std::clamp(y,-48.,15.))));
        return {c.X,layer,c.Z};
    }

    template<unsigned Radius>
    struct CaveWindowPlan
    {
        static_assert(Radius>=1&&Radius<=3);
        static constexpr std::size_t Width=2*Radius+1,Capacity=Width*Width*3;
        using Cell=CaveChunk;
        using Slots=std::array<std::optional<Cell>,Capacity>;
        Slots Next{};
        std::array<bool,Capacity> Dirty{};
        std::size_t Retained=0,Loaded=0,Evicted=0;
    };
    template<unsigned Radius>
    CaveWindowPlan<Radius> PlanCaveWindow(const typename CaveWindowPlan<Radius>::Slots& current,CaveChunk center)
    {
        using Plan=CaveWindowPlan<Radius>;
        if(center.Y<-3||center.Y>0)throw std::out_of_range("Cave window center layer.");
        Plan p;
        std::array<CaveChunk,Plan::Capacity> desired{};
        std::size_t count=0;
        for(int y=-1;y<=1;++y)for(int z=-int(Radius);z<=int(Radius);++z)
            for(int x=-int(Radius);x<=int(Radius);++x)
            {
                const auto wx=std::int64_t(center.X)+x,wz=std::int64_t(center.Z)+z;
                if(wx<std::numeric_limits<std::int32_t>::min()||wx>std::numeric_limits<std::int32_t>::max()||
                    wz<std::numeric_limits<std::int32_t>::min()||wz>std::numeric_limits<std::int32_t>::max())
                    throw std::out_of_range("Cave window overflow.");
                CaveChunk k{static_cast<std::int32_t>(wx),center.Y+y,static_cast<std::int32_t>(wz)};
                (void)StreamingCraftOrigin(k);desired[count++]=k;
            }
        std::array<bool,Plan::Capacity> assigned{};
        for(std::size_t i=0;i<current.size();++i)if(current[i])
        {
            const auto it=std::find(desired.begin(),desired.end(),*current[i]);
            if(it==desired.end()){++p.Evicted;continue;}
            const auto j=static_cast<std::size_t>(it-desired.begin());
            if(assigned[j])throw std::invalid_argument("Duplicate cave slot.");
            assigned[j]=true;p.Next[i]=current[i];++p.Retained;
        }
        std::size_t next=0;
        for(std::size_t i=0;i<p.Next.size();++i)if(!p.Next[i])
        {
            while(assigned[next])++next;
            p.Next[i]=desired[next];assigned[next]=true;p.Dirty[i]=true;++p.Loaded;
        }
        return p;
    }
    template<std::size_t N>
    std::array<bool,N> CaveDirty(const std::array<std::optional<CaveChunk>,N>& slots,VoxelCoordinate c)
    {
        std::array<bool,N> dirty{};
        if(!InCaveWorld(c))return dirty;
        const auto d=[](std::int64_t p,std::int64_t a)
        {return std::max({a-p,std::int64_t{0},p-(a+15)});};
        for(std::size_t i=0;i<N;++i)if(slots[i])
        {
            const auto o=StreamingCraftOrigin(*slots[i]);
            dirty[i]=d(c.X,o.X)+d(c.Y,o.Y)+d(c.Z,o.Z)<=1;
        }
        return dirty;
    }
}
