#pragma once
#include "VoxelInteraction.hpp"
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace kairo::player::detail
{
    // Chunk-local quads with integer corners. The positive winding
    // is outward for both signs. Material IDs survive merging, making
    // the result suitable for opaque material batching or texture arrays.
    // Corner coordinates are local integers in [0,extent], not absolute world
    // positions. The one nonzero normal component identifies the face axis.
    struct VoxelQuad
    {
        std::array<std::array<std::uint32_t,3>,4> Corners{};
        std::array<int,3> Normal{};
        VoxelMaterial Material=VoxelAir;
    };
    // ExposedUnitFaces counts before greedy merging; Quads counts after it.
    // MissingNeighbors records provisional boundaries requiring later remesh.
    struct VoxelChunkSurface
    {
        std::vector<VoxelQuad> Quads;
        std::uint64_t ExposedUnitFaces=0;
        bool MissingNeighbors=false;
    };

    // Greedy opaque-solid surface mesher. A chunk contains extent^3
    // cells, beginning at an absolute integer coordinate. Sample the
    // interior and one-cell halo exactly once. Unknown interior means
    // no valid mesh can be produced; unknown exterior emits temporary
    // exposed faces, with a flag requiring remesh when neighbors arrive.
    // A face belongs ONLY to the chunk containing its solid voxel.
    // Query maps absolute coordinates to optional material IDs. This CPU-only
    // routine returns owned geometry and never touches the renderer. Runtime
    // callers convert via BuildCraftMesh in CraftWorld.hpp; streaming workers
    // can run it on immutable occupancy snapshots.
    template<class Query>
    [[nodiscard]] VoxelChunkSurface BuildVoxelChunkSurface(
        VoxelCoordinate origin,std::uint32_t extent,Query&& query)
    {
        if(extent==0||extent>64)
            throw std::invalid_argument("Voxel chunk extent must be 1..64.");
        constexpr auto lo=std::numeric_limits<std::int32_t>::min();
        constexpr auto hi=std::numeric_limits<std::int32_t>::max();
        const std::array<std::int64_t,3> base{origin.X,origin.Y,origin.Z};
        for(auto a:base)
            if(a-1<lo||a+extent>hi)
                throw std::out_of_range("Voxel chunk halo outside signed domain.");
        const int n=static_cast<int>(extent),width=n+2;
        // Cache (extent+2)^3 samples once, including the one-voxel halo.
        // Shift local [-1,n] coordinates by one when flattening the array.
        std::vector<VoxelMaterial> voxels(static_cast<std::size_t>(width)*width*width);
        const auto index=[&](int x,int y,int z)
        {return static_cast<std::size_t>(((z+1)*width+(y+1))*width+x+1);};
        VoxelChunkSurface result;
        for(int z=-1;z<=n;++z)for(int y=-1;y<=n;++y)for(int x=-1;x<=n;++x)
        {
            const std::optional<VoxelMaterial> value=query(VoxelCoordinate{
                static_cast<std::int32_t>(base[0]+x),static_cast<std::int32_t>(base[1]+y),
                static_cast<std::int32_t>(base[2]+z)});
            // Unknown interior invalidates the candidate mesh. Unknown halo
            // becomes temporary air; the neighbor-arrival policy must remesh.
            const bool interior=x>=0&&x<n&&y>=0&&y<n&&z>=0&&z<n;
            if(!value&&interior)throw std::runtime_error("Voxel chunk interior unavailable.");
            if(!value)result.MissingNeighbors=true;
            voxels[index(x,y,z)]=value.value_or(VoxelAir);
        }
        const auto get=[&](const std::array<int,3>& p){return voxels[index(p[0],p[1],p[2])];};
        // Reused 2D slice of material IDs: air denotes absent/consumed face,
        // any other ID is a face eligible for same-material rectangle merging.
        std::vector<VoxelMaterial> mask(static_cast<std::size_t>(n)*n);
        const auto at=[&](int u,int v)->VoxelMaterial&
        {return mask[static_cast<std::size_t>(v)*n+u];};
        // Deterministic axis/sign/slice/row order. For each exposed-face
        // plane, merge adjacent equal-material faces into rectangles.
        for(unsigned axis=0;axis<3;++axis)
        {
            const unsigned u=(axis+1)%3,v=(axis+2)%3;
            for(int sign:{-1,1})for(int plane=0;plane<n;++plane)
            {
                for(int j=0;j<n;++j)for(int i=0;i<n;++i)
                {
                    std::array<int,3> p{};
                    p[axis]=plane;p[u]=i;p[v]=j;
                    auto neighbor=p;neighbor[axis]+=sign;
                    // Only the solid voxel owns this exposed face. Do not emit
                    // a face between two solids, even if their materials differ.
                    const auto material=get(p);
                    at(i,j)=material!=VoxelAir&&get(neighbor)==VoxelAir?material:VoxelAir;
                    if(at(i,j)!=VoxelAir)++result.ExposedUnitFaces;
                }
                for(int j=0;j<n;++j)for(int i=0;i<n;++i)
                {
                    const auto material=at(i,j);
                    if(material==VoxelAir)continue;
                    // Greedy merge: extend a horizontal run, then extend rows
                    // while all cells in that run match. This is deterministic,
                    // not a proof of globally minimum rectangle count.
                    int w=1,h=1;
                    while(i+w<n&&at(i+w,j)==material)++w;
                    while(j+h<n)
                    {
                        bool matches=true;
                        for(int k=0;k<w;++k)if(at(i+k,j+h)!=material){matches=false;break;}
                        if(!matches)break;
                        ++h;
                    }
                    VoxelQuad quad;
                    quad.Material=material;quad.Normal[axis]=sign;
                    const auto corner=[&](int x,int y)
                    {
                        std::array<std::uint32_t,3> p{};
                        p[axis]=static_cast<std::uint32_t>(plane+(sign>0));
                        p[u]=static_cast<std::uint32_t>(x);
                        p[v]=static_cast<std::uint32_t>(y);
                        return p;
                    };
                    quad.Corners={corner(i,j),corner(i+w,j),corner(i+w,j+h),corner(i,j+h)};
                    // Reversing two corners flips winding on negative faces,
                    // so front-face culling and the normal agree.
                    if(sign<0)std::swap(quad.Corners[1],quad.Corners[3]);
                    result.Quads.push_back(quad);
                    // Consume exactly the rectangle we emitted. Remaining masks
                    // are processed once by the outer row/column traversal.
                    for(int y=0;y<h;++y)for(int x=0;x<w;++x)at(i+x,j+y)=VoxelAir;
                }
            }
        }
        return result;
    }
}
