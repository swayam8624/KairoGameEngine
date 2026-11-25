#pragma once
#include "CraftCreature.hpp"
#include "CraftInventory.hpp"
#include "RuntimePickupPool.hpp"

namespace kairo::player::detail
{
    // Return separate hit/kill facts and immediate stone award. World drops
    // are represented by CraftPickups and do not inflate StoneAwarded.
    struct CraftCombatResult
    {
        bool Hit=false,Defeated=false;
        std::uint32_t StoneAwarded=0;
    };
    // Defeating a stone sentinel awards up to two stone blocks immediately.
    // The caller must establish a visible, in-range target before striking.
    // No overflow, no delayed or repeatable reward: the creature is removed
    // on the killing blow, and any excess above stack capacity is discarded.
    // Legacy immediate credit path: clamp award to inventory stack capacity.
    inline CraftCombatResult CraftStrikeReward(CraftCreature& creature,std::array<std::uint32_t,4>& counts)
    {
        if(std::any_of(counts.begin(),counts.end(),[](auto n){return n>999;}))
            throw std::invalid_argument("Invalid combat inventory.");
        if(!creature.Strike())return {};
        if(creature.Body())return {true,false,0};
        const auto awarded=std::min(2u,999u-counts[2]);
        counts[2]+=awarded;
        return {true,true,awarded};
    }
    // RuntimePickupPool stores the world drops independently of renderer.
    using CraftPickups=RuntimePickupPool<128>;

    // On death, leave a real world item rather than teleporting the loot.
    // Saturation uses the old bounded inventory award instead of allocating
    // unbounded objects or evicting an unrelated player's loot.
    // Read the death position before Strike removes the body. Prefer world
    // drop; bounded inventory credit is a fallback only when pool is full.
    inline CraftCombatResult CraftStrikeDrop(CraftCreature& creature,CraftInventory& inventory,CraftPickups& pickups)
    {
        if(!inventory.Valid())throw std::invalid_argument("Invalid combat inventory.");
        if(!creature.Body())return {};
        auto center=creature.Body()->Feet;center[1]+=.4;
        if(!creature.Strike())return {};
        if(creature.Body())return {true,false,0};
        if(pickups.TrySpawn(center,CraftStone,2))return {true,true,0};
        const auto award=std::min(2u,CraftInventory::StackLimit-inventory.Count(CraftStone));
        inventory.Counts[CraftStone-1]+=award;
        return {true,true,award};
    }

    // Query supplies optional voxel occupancy in absolute coordinates.
    // TraceVoxels performs occlusion; inventory callback credits only the
    // accepted stack portion and the pool retains any remainder.
    template<class Query>
    void CollectCraftPickups(CraftPickups& pickups,CraftInventory& inventory,
        const std::array<double,3>& eye,Query&& query)
    {
        if(!inventory.Valid())throw std::invalid_argument("Invalid pickup inventory.");
        pickups.Collect(eye,2.,[&](const auto& from,const auto& to)
        {
            // Unknown space must be treated as solid, not as an unobstructed ray.
            const auto solid=[&](VoxelCoordinate c){return query(c).value_or(VoxelMaterial{65535});};
            const auto cell=VoxelCoordinate{static_cast<std::int32_t>(std::floor(to[0])),
                static_cast<std::int32_t>(std::floor(to[1])),static_cast<std::int32_t>(std::floor(to[2]))};
            if(solid(cell)!=VoxelAir)return false;
            const std::array direction{to[0]-from[0],to[1]-from[1],to[2]-from[2]};
            const double length=std::hypot(direction[0],direction[1],direction[2]);
            return length<=1e-12||!TraceVoxels(from,direction,length,64,solid);
        },[&](std::uint32_t kind,std::uint32_t count)
        {
            if(kind<CraftGrass||kind>CraftPlaced)return 0u;
            auto& held=inventory.Counts[kind-1];
            const auto accepted=std::min(count,CraftInventory::StackLimit-held);
            held+=accepted;return accepted;
        });
    }
}
