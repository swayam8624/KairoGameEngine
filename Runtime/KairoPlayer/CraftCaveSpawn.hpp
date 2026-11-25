#pragma once
#include "CraftCaveWorld.hpp"
#include "VoxelPlayer.hpp"

namespace kairo::player::detail
{
    // Spawn against the authoritative edited world, not an unloaded chunk
    // or the seed alone. Prefer supported surface positions in a fixed
    // bounded neighborhood. Never begin inside a solid block or above a shaft.
    template<class Query>
    std::optional<VoxelPlayer> FindCaveSpawn(const VoxelPlayerConfig& config,Query&& query)
    {
        VoxelPlayer candidate;
        ValidateVoxelPlayer(candidate,config);
        for(int radius=0;radius<=8;++radius)
            for(int z=-radius;z<=radius;++z)
                for(int x=-radius;x<=radius;++x)
                {
                    if(std::max(std::abs(x),std::abs(z))!=radius)continue;
                    for(int y=CaveTop-1;y>=0;--y)
                    {
                        if(double(y)+config.Height>CaveTop)continue;
                        candidate.Feet={8.5+x,double(y),8.5+z};
                        if(VoxelPlayerBlocked(candidate,config,query))continue;
                        // Require an actual solid supporting cell, not an
                        // unavailable query or an artificial unloaded boundary.
                        const auto ground=query({8+x,y-1,8+z});
                        if(!ground||*ground==VoxelAir)continue;
                        candidate.Grounded=true;
                        return candidate;
                    }
                }
        return std::nullopt;
    }
}
