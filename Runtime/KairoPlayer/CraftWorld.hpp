#pragma once
#include "VoxelChunkSurface.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace kairo::player::detail
{
    // Small bounded deterministic Craft sandbox. All chunks are known,
    // including their exterior air; this is not yet a streaming voxel world.
    inline constexpr int CraftExtent=16,CraftFirst=-16,CraftLast=32;
    inline constexpr std::size_t CraftChunkCount=9;
    inline constexpr VoxelMaterial CraftGrass=1,CraftDirt=2,CraftStone=3,CraftPlaced=4;

    inline std::uint64_t CraftHash(std::uint64_t x) noexcept
    {
        x+=0x9e3779b97f4a7c15ULL;
        x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;
        x=(x^(x>>27))*0x94d049bb133111ebULL;
        return x^(x>>31);
    }
    inline bool InCraftWorld(VoxelCoordinate c) noexcept
    {return c.X>=CraftFirst&&c.X<CraftLast&&c.Z>=CraftFirst&&c.Z<CraftLast&&c.Y>=0&&c.Y<CraftExtent;}
    struct CraftBaseline
    {
        std::uint64_t Seed=42;
        VoxelMaterial operator()(VoxelCoordinate c) const noexcept
        {
            if(!InCraftWorld(c))return VoxelAir;
            // Integer plateau heights, reproducible across hosts and signs.
            // Floor division preserves the negative-side tile convention.
            const auto tile=[](int x){return x>=0?x/4:(x-3)/4;};
            const auto h=CraftHash(Seed^CraftHash(static_cast<std::uint64_t>(tile(c.X)))^
                CraftHash(static_cast<std::uint64_t>(tile(c.Z))+0x4cf5ad432745937fULL));
            const int top=3+static_cast<int>(h%4);
            if(c.Y>top)return VoxelAir;
            if(c.Y==top)return CraftGrass;
            return c.Y>=top-2?CraftDirt:CraftStone;
        }
    };
    inline VoxelCoordinate CraftChunkOrigin(std::size_t index)
    {
        if(index>=CraftChunkCount)throw std::out_of_range("Craft chunk index.");
        return {CraftFirst+static_cast<int>(index%3)*CraftExtent,0,
            CraftFirst+static_cast<int>(index/3)*CraftExtent};
    }
    // Only the owner and its face-neighbors can depend on an edited block.
    inline std::array<bool,CraftChunkCount> CraftDirtyChunks(VoxelCoordinate cell)
    {
        if(!InCraftWorld(cell))throw std::out_of_range("Edit outside Craft world.");
        std::array<bool,CraftChunkCount> dirty{};
        for(std::size_t i=0;i<CraftChunkCount;++i)
        {
            const auto c=CraftChunkOrigin(i);
            const int dx=cell.X<c.X?c.X-cell.X:cell.X>=c.X+CraftExtent?cell.X-(c.X+CraftExtent-1):0;
            const int dz=cell.Z<c.Z?c.Z-cell.Z:cell.Z>=c.Z+CraftExtent?cell.Z-(c.Z+CraftExtent-1):0;
            dirty[i]=dx+dz<=1;
        }
        return dirty;
    }

    struct CraftVertex
    {
        std::array<float,3> Position{},Color{},Normal{};
        std::array<float,2> UV{};
    };
    struct CraftMeshData
    {
        std::vector<CraftVertex> Vertices;
        std::vector<std::uint32_t> Indices;
    };
    inline std::array<float,3> CraftColor(VoxelMaterial material)
    {
        switch(material)
        {
            case CraftGrass:return {.25f,.65f,.20f};
            case CraftDirt:return {.42f,.24f,.12f};
            case CraftStone:return {.46f,.48f,.53f};
            case CraftPlaced:return {.87f,.46f,.16f};
            default:throw std::invalid_argument("Unsupported Craft material.");
        }
    }
    // Convert each rectangle to two indexed triangles; vertex duplication
    // preserves sharp face normals and per-material colors at all corners.
    inline CraftMeshData BuildCraftMesh(const VoxelChunkSurface& surface)
    {
        if(surface.Quads.size()>6u*64u*64u*64u)
            throw std::length_error("Voxel surface exceeds supported chunk bound.");
        CraftMeshData mesh;
        mesh.Vertices.reserve(surface.Quads.size()*4u);
        mesh.Indices.reserve(surface.Quads.size()*6u);
        constexpr std::array<std::array<float,2>,4> uv{{{0,0},{1,0},{1,1},{0,1}}};
        for(const auto& quad:surface.Quads)
        {
            const auto color=CraftColor(quad.Material);
            const auto start=static_cast<std::uint32_t>(mesh.Vertices.size());
            for(unsigned i=0;i<4;++i)
            {
                const auto& p=quad.Corners[i];const auto& n=quad.Normal;
                mesh.Vertices.push_back({{float(p[0]),float(p[1]),float(p[2])},color,
                    {float(n[0]),float(n[1]),float(n[2])},uv[i]});
            }
            mesh.Indices.insert(mesh.Indices.end(),{start,start+1,start+2,start,start+2,start+3});
        }
        return mesh;
    }
}
