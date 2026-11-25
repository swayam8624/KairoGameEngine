#include "../CraftSurfaceSpawn.hpp"
#include <iostream>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
int main()
{
    const VoxelPlayerConfig config;
    const auto flat=[](VoxelCoordinate c)->VoxelMaterial{return c.Y>=0&&c.Y<4?1:0;};
    VoxelEdits world(flat);
    const auto query=[&](VoxelCoordinate c)->std::optional<VoxelMaterial>{return world.Get(c);};
    auto spawn=FindCraftSurfaceSpawn(config,16,query);
    Check(spawn&&spawn->Feet==std::array{.5,4.,.5},"Baseline spawn");
    for(int y=0;y<4;++y)Check(world.Set({0,y,0},0),"Shaft edit failed");
    spawn=FindCraftSurfaceSpawn(config,16,query);
    Check(spawn&&spawn->Feet!=std::array{.5,4.,.5}&&spawn->Feet[1]==4,"Empty column not relocated");
    auto p=*spawn;AdvanceVoxelPlayer(p,config,{},.1,query);
    Check(p.Grounded&&p.Feet==spawn->Feet,"Relocated spawn not supported by physics");
    Check(world.Set({0,15,0},1),"Top block edit failed");
    spawn=FindCraftSurfaceSpawn(config,16,query);
    Check(spawn&&spawn->Feet==std::array{.5,16.,.5},"Top layer not usable");
    Check(!FindCraftSurfaceSpawn(config,16,[](VoxelCoordinate)->std::optional<VoxelMaterial>{return {};}),"Unknown world accepted");
    Check(!FindCraftSurfaceSpawn(config,16,[](VoxelCoordinate)->std::optional<VoxelMaterial>{return 0;}),"Empty world accepted");
    Check(!FindCraftSurfaceSpawn(config,16,[](VoxelCoordinate)->std::optional<VoxelMaterial>{return 1;}),"Solid world accepted");
    std::mt19937 rng(512);
    for(unsigned i=0;i<10000;++i)
    {
        const int x=int(rng()%33)-16,z=int(rng()%33)-16,y=int(rng()%16);
        const auto single=[&](VoxelCoordinate c)->std::optional<VoxelMaterial>{return c==VoxelCoordinate{x,y,z}?1:0;};
        const auto result=FindCraftSurfaceSpawn(config,16,single);
        Check(result&&result->Feet==std::array{x+.5,double(y+1),z+.5},"Single-support search missed or moved candidate");
        Check(result->Grounded&&!VoxelPlayerBlocked(*result,config,single),"Spawn overlaps block");
        auto below=*result;below.Feet[1]-=.000001;
        Check(VoxelPlayerBlocked(below,config,single),"Spawn lacks support");
    }
    std::cout<<"10000 signed support fixtures, real voxel movement, edited shaft, top layer and unavailable worlds passed\n";
}
