#include "../VoxelPlayer.hpp"
#include "../CraftVitals.hpp"
#include <iostream>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    VoxelPlayerConfig c;
    const auto floor=[](VoxelCoordinate p)->std::optional<VoxelMaterial>{return p.Y<0?1:0;};
    VoxelPlayer p{{.5,0,.5}};
    AdvanceVoxelPlayer(p,c,{},0,floor);
    Check(p.Grounded,"Ground contact missing");
    double apex=0;
    for(unsigned i=0;i<240;++i)
    {
        AdvanceVoxelPlayer(p,c,{0,0,i==0,false},1./120,floor);
        apex=std::max(apex,p.Feet[1]);
        Check(!VoxelPlayerBlocked(p,c,floor),"Floor penetration");
    }
    Check(p.Grounded&&std::abs(p.Feet[1])<1e-6&&std::abs(apex-25/(2*9.81))<.0002,"Ballistic landing");
    const auto wall=[](VoxelCoordinate p)->std::optional<VoxelMaterial>
    {return p.Y<0||p.X==2?1:0;};
    p=VoxelPlayer{{.5,0,.5}};
    for(unsigned i=0;i<120;++i)
        AdvanceVoxelPlayer(p,c,{1,1,false,false},1./120,wall);
    Check(std::abs(p.Feet[0]-1.7)<1e-6&&p.Feet[2]>3,"Wall sliding failed");
    Check(!VoxelPlayerBlocked(p,c,wall),"Wall penetration");
    const auto roof=[](VoxelCoordinate p)->std::optional<VoxelMaterial>
    {return p.Y<0||p.Y==2?1:0;};
    p=VoxelPlayer{{.5,0,.5}};
    AdvanceVoxelPlayer(p,c,{0,0,true,false},.1,roof);
    Check(p.Feet[1]<=.20000001&&!VoxelPlayerBlocked(p,c,roof),"Ceiling penetration");
    const auto unknown=[](VoxelCoordinate p)->std::optional<VoxelMaterial>
    {if(p.X>=1)return {};return p.Y<0?1:0;};
    p=VoxelPlayer{{.5,0,.5}};
    for(unsigned i=0;i<20;++i)AdvanceVoxelPlayer(p,c,{1,0,false,true},.1,unknown);
    Check(std::abs(p.Feet[0]-.7)<1e-6,"Unknown cells traversed");
    Check(PlayerIntersectsBlock(p,c,{0,0,0})&&!PlayerIntersectsBlock(p,c,{0,-1,0}),"Placement overlap");
    p=VoxelPlayer{{.5,20,.5},-100};
    for(unsigned i=0;i<10;++i)AdvanceVoxelPlayer(p,c,{},.1,floor);
    Check(p.Grounded&&p.Feet[1]<1e-6&&!VoxelPlayerBlocked(p,c,floor),"Fast fall tunneled");
    // Exercise the actual collision integrator, not only sampled heights.
    CraftVitals vitals;
    for(double height:{5.25,30.})
    {
        p=VoxelPlayer{{.5,height,.5}};
        vitals.Respawn(height);
        for(unsigned i=0;i<600&&!p.Grounded;++i)
        {
            AdvanceVoxelPlayer(p,c,{},1./120,floor);
            (void)vitals.Observe(p.Feet[1],p.Grounded,p.Grounded);
        }
        Check(p.Grounded&&!VoxelPlayerBlocked(p,c,floor),"Survival landing failed");
        Check(vitals.Health()==(height<10?17u:0u),"Integrated fall damage mismatch");
    }
    p=VoxelPlayer{{-100000000.5,0,-100000000.5}};
    for(unsigned i=0;i<10000;++i)AdvanceVoxelPlayer(p,c,{1,0,false,false},.01,floor);
    Check(std::abs(p.Feet[0]-(-100000000.5+450))<.01&&p.Grounded,"Large coordinates");
    std::cout<<"Voxel player: apex "<<apex<<" m, wall sliding, ceiling, unknown cells, fast fall, placement and 10000 large-world steps passed\n";
}
