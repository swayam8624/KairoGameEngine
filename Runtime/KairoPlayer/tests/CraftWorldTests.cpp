#include "../CraftWorld.hpp"
#include <iostream>
#include <set>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
}
int main()
{
    const CraftBaseline a{42},b{43};
    unsigned different=0;
    for(int z=CraftFirst;z<CraftLast;++z)for(int x=CraftFirst;x<CraftLast;++x)
    {
        unsigned grass=0;
        for(int y=0;y<CraftExtent;++y)
        {
            const VoxelCoordinate c{x,y,z};
            Check(a(c)==CraftBaseline{42}(c),"Seed nondeterministic");
            grass+=a(c)==CraftGrass;
            different+=a(c)!=b(c);
        }
        Check(grass==1,"Column missing grass surface");
    }
    Check(different>0,"Seeds did not differ");
    Check(a({CraftLast,0,0})==VoxelAir&&a({0,-1,0})==VoxelAir,"Exterior not air");
    auto corner=CraftDirtyChunks({0,4,0});
    Check(std::count(corner.begin(),corner.end(),true)==3&&corner[1]&&corner[3]&&corner[4],
        "Boundary invalidation missed face neighbor or included diagonal");
    auto inner=CraftDirtyChunks({1,4,1});
    Check(std::count(inner.begin(),inner.end(),true)==1&&inner[4],"Interior invalidation incorrect");
    std::size_t quads=0,indices=0;
    for(std::size_t i=0;i<CraftChunkCount;++i)
    {
        const auto surface=BuildVoxelChunkSurface(CraftChunkOrigin(i),CraftExtent,a);
        const auto mesh=BuildCraftMesh(surface);
        Check(!surface.MissingNeighbors&&mesh.Vertices.size()==4*surface.Quads.size()&&
            mesh.Indices.size()==6*surface.Quads.size(),"Mesh counts differ");
        for(std::size_t j=0;j<surface.Quads.size();++j)
        {
            const auto& quad=surface.Quads[j];
            for(unsigned k=0;k<4;++k)
            {
                const auto& v=mesh.Vertices[j*4+k];
                Check(v.Color==CraftColor(quad.Material),"Material color lost");
                for(unsigned axis=0;axis<3;++axis)
                    Check(v.Position[axis]==quad.Corners[k][axis]&&v.Normal[axis]==quad.Normal[axis],
                        "Native vertex differs");
            }
            const auto offset=static_cast<std::uint32_t>(j*4);
            constexpr std::array<std::uint32_t,6> pattern{0,1,2,0,2,3};
            for(unsigned k=0;k<6;++k)Check(mesh.Indices[j*6+k]==offset+pattern[k],"Invalid triangle index");
        }
        quads+=surface.Quads.size();indices+=mesh.Indices.size();
    }
    VoxelEdits world(a);
    auto top=TraceVoxels({.5,12,.5},{0,-1,0},12,32,[&](auto c){return world.Get(c);});
    Check(top&&top->Material==CraftGrass,"Seeded grass ray failed");
    Check(world.Set(top->Cell,VoxelAir),"Remove failed");
    auto below=TraceVoxels({.5,12,.5},{0,-1,0},12,32,[&](auto c){return world.Get(c);});
    Check(below&&below->Placement==top->Cell,"Removal did not reveal placement location");
    Check(world.Set(*below->Placement,top->Material)&&world.Overrides().empty(),"Restore did not reclaim override");
    std::cout<<"9 seeded Craft chunks, "<<quads<<" quads, "<<indices/3<<" triangles; deterministic materials and boundary invalidation passed\n";
}
