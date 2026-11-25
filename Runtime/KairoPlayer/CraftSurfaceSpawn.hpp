#pragma once
#include "VoxelPlayer.hpp"

namespace kairo::player::detail
{
    // Search the authoritative edited surface, not the artificial collision
    // floor or resident chunk set. Centered feet fit within one column for
    // every supported player radius. A top-layer block can support a spawn
    // above the editable height, provided the headroom query is known air.
    // Query returns optional occupancy for an absolute voxel coordinate. Null
    // means unknown, not air. A successful result is a grounded VoxelPlayer
    // with zero initial velocity; the caller still owns streaming residency.
    // Search bounded concentric square rings, high-to-low within each column.
    template<class Query>
    std::optional<VoxelPlayer> FindCraftSurfaceSpawn(const VoxelPlayerConfig& config,int extent,Query&& query)
    {
        VoxelPlayer candidate;
        ValidateVoxelPlayer(candidate,config);
        if(extent<1||extent>256)throw std::invalid_argument("Invalid surface spawn extent.");
        // Chebyshev rings visit each horizontal coordinate exactly once, with
        // a deterministic z/x ordering for tied distances to the world origin.
        for(int radius=0;radius<=16;++radius)
            for(int z=-radius;z<=radius;++z)
                for(int x=-radius;x<=radius;++x)
                {
                    if(std::max(std::abs(x),std::abs(z))!=radius)continue;
                    for(int y=extent;y>=1;--y)
                    {
                        // Physical support must be known and solid. The separate
                        // VoxelPlayerBlocked test covers the entire player body,
                        // not just the voxel immediately above the feet.
                        const std::optional<VoxelMaterial> support=query({x,y-1,z});
                        if(!support||*support==VoxelAir)continue;
                        candidate.Feet={x+.5,double(y),z+.5};
                        if(VoxelPlayerBlocked(candidate,config,query))continue;
                        candidate.Grounded=true;
                        return candidate;
                    }
                }
        return std::nullopt;
    }
}
