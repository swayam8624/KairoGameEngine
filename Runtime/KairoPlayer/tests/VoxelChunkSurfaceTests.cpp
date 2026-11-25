#include "../VoxelChunkSurface.hpp"
#include <iostream>
#include <set>
#include <tuple>
#include <random>

namespace {
using namespace kairo::player::detail;
using Face=std::tuple<int,int,int,unsigned,int,VoxelMaterial>;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
int main()
{
    constexpr int n=16;
    const auto solid=[](VoxelCoordinate c)->std::optional<VoxelMaterial>{
        return c.X>=0&&c.X<n&&c.Y>=0&&c.Y<n&&c.Z>=0&&c.Z<n?1:0;
    };
    const auto cube=BuildVoxelChunkSurface({0,0,0},n,solid);
    Check(cube.Quads.size()==6&&cube.ExposedUnitFaces==6*n*n,"Solid cube did not merge");
    const auto adjacent=[](VoxelCoordinate c)->std::optional<VoxelMaterial>{
        return c.X>=0&&c.X<2*n&&c.Y>=0&&c.Y<n&&c.Z>=0&&c.Z<n?1:0;
    };
    const auto left=BuildVoxelChunkSurface({0,0,0},n,adjacent);
    const auto right=BuildVoxelChunkSurface({n,0,0},n,adjacent);
    Check(left.Quads.size()==5&&right.Quads.size()==5&&
        left.ExposedUnitFaces+right.ExposedUnitFaces==10*n*n,"Shared chunk face retained");
    const auto unknown=[](VoxelCoordinate c)->std::optional<VoxelMaterial>{
        if(c.X<0||c.X>=n||c.Y<0||c.Y>=n||c.Z<0||c.Z>=n)return {};
        return 1;
    };
    const auto temporary=BuildVoxelChunkSurface({0,0,0},n,unknown);
    Check(temporary.MissingNeighbors&&temporary.Quads.size()==6,"Unknown halo not flagged");
    bool rejected=false;
    try{(void)BuildVoxelChunkSurface({0,0,0},n,[](VoxelCoordinate)->std::optional<VoxelMaterial>{return {};});}
    catch(const std::runtime_error&){rejected=true;}
    Check(rejected,"Unknown interior accepted");

    std::mt19937 rng(84821);
    std::uint64_t checkedFaces=0,totalQuads=0;
    for(unsigned scene=0;scene<40;++scene)
    {
        constexpr int width=n+2;
        std::vector<VoxelMaterial> occupancy(width*width*width);
        for(auto& cell:occupancy)cell=static_cast<VoxelMaterial>(rng()%4);
        const VoxelCoordinate origin{-32,16,-48};
        const auto sample=[&](VoxelCoordinate c)->std::optional<VoxelMaterial>{
            const int x=c.X-origin.X+1,y=c.Y-origin.Y+1,z=c.Z-origin.Z+1;
            return occupancy.at(static_cast<std::size_t>((z*width+y)*width+x));
        };
        const auto mesh=BuildVoxelChunkSurface(origin,n,sample);
        std::set<Face> expected,actual;
        for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x)
        {
            const auto mat=*sample({origin.X+x,origin.Y+y,origin.Z+z});
            if(mat==VoxelAir)continue;
            for(unsigned axis=0;axis<3;++axis)for(int sign:{-1,1})
            {
                std::array p{x,y,z};p[axis]+=sign;
                if(*sample({origin.X+p[0],origin.Y+p[1],origin.Z+p[2]})==VoxelAir)
                    expected.emplace(x,y,z,axis,sign,mat);
            }
        }
        for(const auto& q:mesh.Quads)
        {
            unsigned axis=0;
            while(axis<3&&q.Normal[axis]==0)++axis;
            Check(axis<3,"Missing face normal");
            const int sign=q.Normal[axis];
            const unsigned u=(axis+1)%3,v=(axis+2)%3;
            const auto a=q.Corners[0],b=q.Corners[1],c=q.Corners[2];
            const int cross=(int(b[u])-int(a[u]))*(int(c[v])-int(a[v]))-
                (int(b[v])-int(a[v]))*(int(c[u])-int(a[u]));
            Check(cross*sign>0,"Wrong quad winding");
            std::array<std::uint32_t,3> lo=a,hi=a;
            for(const auto& p:q.Corners)
                for(unsigned k=0;k<3;++k){lo[k]=std::min(lo[k],p[k]);hi[k]=std::max(hi[k],p[k]);}
            Check(lo[axis]==hi[axis]&&hi[u]>lo[u]&&hi[v]>lo[v],"Invalid quad geometry");
            for(auto j=lo[v];j<hi[v];++j)for(auto i=lo[u];i<hi[u];++i)
            {
                std::array<int,3> p{};
                p[axis]=int(lo[axis])-(sign>0);p[u]=int(i);p[v]=int(j);
                Check(actual.emplace(p[0],p[1],p[2],axis,sign,q.Material).second,
                    "Duplicate merged face");
            }
        }
        Check(actual==expected&&mesh.ExposedUnitFaces==expected.size(),"Greedy coverage differs from unit face oracle");
        checkedFaces+=actual.size();totalQuads+=mesh.Quads.size();
    }
    std::cout<<"Solid 16^3 chunk: "<<cube.ExposedUnitFaces<<" unit faces -> "<<cube.Quads.size()<<" quads\n";
    std::cout<<checkedFaces<<" randomized exposed faces, "<<totalQuads
        <<" greedy quads, exact material coverage, winding and shared border passed\n";
}
