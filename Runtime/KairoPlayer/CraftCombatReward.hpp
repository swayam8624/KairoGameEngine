#pragma once
#include "CraftCreature.hpp"

namespace kairo::player::detail
{
    struct CraftCombatResult
    {
        bool Hit=false,Defeated=false;
        std::uint32_t StoneAwarded=0;
    };
    // Defeating a stone sentinel awards up to two stone blocks immediately.
    // The caller must establish a visible, in-range target before striking.
    // No overflow, no delayed or repeatable reward: the creature is removed
    // on the killing blow, and any excess above stack capacity is discarded.
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
}
