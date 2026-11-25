#include "../VoxelInteraction.hpp"
#include <iostream>
#include <random>
#include <vector>

namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
}
int main()
{
    const auto baseline=[](VoxelCoordinate c)->VoxelMaterial{return c.Y<0?1:0;};
    VoxelEdits edits(baseline,2);
    Check(edits.Get({0,-1,0})==1&&edits.Get({0,0,0})==0,"Baseline occupancy differs");
    Check(edits.Set({0,-1,0},0)&&edits.Set({0,0,0},2),"Remove/place failed");
    Check(!edits.Set({1,0,0},2)&&edits.Overrides().size()==2,"Edit cap exceeded");
    Check(edits.Set({0,-1,0},1)&&edits.Set({1,0,0},2),"Baseline revert did not reclaim slot");
    Check(edits.Get({0,-1,0})==1&&edits.Get({1,0,0})==2,"Edit lookup incorrect");
    const auto single=[](VoxelCoordinate c)->std::optional<VoxelMaterial>
    {return c==VoxelCoordinate{-2,0,0}?3:0;};
    auto hit=TraceVoxels({0.0,.5,.5},{-1,0,0},4,16,single);
    Check(hit&&hit->Cell==VoxelCoordinate{-2,0,0}&&hit->Distance==1&&
        hit->Placement==VoxelCoordinate{-1,0,0}&&hit->Material==3,"Negative boundary ray");
    Check(!TraceVoxels({0.0,.5,.5},{-1,0,0},.9,16,single),"Out-of-range hit");
    Check(!TraceVoxels({0.0,.5,.5},{-1,0,0},4,1,single),"Visit cap ignored");
    auto inside=TraceVoxels({-1.5,.5,.5},{1,0,0},4,16,single);
    Check(inside&&inside->Distance==0&&!inside->Placement,"Inside-solid placement");
    const auto corner=[](VoxelCoordinate c)->std::optional<VoxelMaterial>
    {return (c==VoxelCoordinate{1,0,0}||c==VoxelCoordinate{1,1,0})?1:0;};
    auto diagonal=TraceVoxels({.5,.5,.5},{1,1,0},4,16,corner);
    Check(diagonal&&diagonal->Cell==VoxelCoordinate{1,1,0}&&!diagonal->Placement,
        "Corner contact selected zero-length side cell");
    const auto unknown=[](VoxelCoordinate c)->std::optional<VoxelMaterial>
    {if(c.X==1)return {};return c.X==2?1:0;};
    Check(!TraceVoxels({.5,.5,.5},{1,0,0},10,16,unknown),"Ray crossed unknown terrain");
    const auto empty=[](VoxelCoordinate)->std::optional<VoxelMaterial>{return 0;};
    Check(!TraceVoxels({2147483647.5,0,0},{1,0,0},10,16,empty),"Signed cell overflow");
    std::mt19937_64 rng(49823);
    std::uniform_real_distribution<double> pos(-6,6),dir(-1,1);
    std::vector<VoxelCoordinate> solids;
    for(int x=-3;x<=3;++x)for(int y=-3;y<=3;++y)for(int z=-3;z<=3;++z)
        if(rng()%5==0)solids.push_back({x,y,z});
    const auto query=[&](VoxelCoordinate c)->std::optional<VoxelMaterial>
    {return std::find(solids.begin(),solids.end(),c)!=solids.end()?1:0;};
    unsigned hits=0;
    for(unsigned i=0;i<20000;++i)
    {
        std::array o{pos(rng),pos(rng),pos(rng)},d{dir(rng),dir(rng),dir(rng)};
        const auto len=std::hypot(d[0],d[1],d[2]);
        for(auto& v:d)v/=len;
        // Independent ray-box slab oracle over every occupied unit cube.
        double best=std::numeric_limits<double>::infinity();
        std::optional<VoxelCoordinate> expected;
        for(const auto cell:solids)
        {
            const std::array<double,3> lo{double(cell.X),double(cell.Y),double(cell.Z)};
            double enter=0,exit=12;
            for(unsigned axis=0;axis<3;++axis)
            {
                if(d[axis]==0)
                {if(o[axis]<lo[axis]||o[axis]>=lo[axis]+1)exit=-1;continue;}
                const double a=(lo[axis]-o[axis])/d[axis],b=(lo[axis]+1-o[axis])/d[axis];
                enter=std::max(enter,std::min(a,b));exit=std::min(exit,std::max(a,b));
            }
            if(exit>=enter&&enter<best){best=enter;expected=cell;}
        }
        const auto actual=TraceVoxels(o,d,12,128,query);
        Check(bool(actual)==bool(expected),"DDA hit disagrees with box oracle");
        if(actual)
        {
            ++hits;
            Check(actual->Cell==*expected&&std::abs(actual->Distance-best)<1e-9,"DDA nearest hit differs");
            if(actual->Placement)
            {
                const auto p=*actual->Placement,c=actual->Cell;
                Check(std::abs(p.X-c.X)+std::abs(p.Y-c.Y)+std::abs(p.Z-c.Z)==1&&
                    query(p)==VoxelAir,"Placement not face-adjacent air");
            }
        }
    }
    std::cout<<"20000 voxel rays compared against independent slab oracle, "<<hits
        <<" hits; bounded edits, signed boundaries, corners and unknown cells passed\n";
}
