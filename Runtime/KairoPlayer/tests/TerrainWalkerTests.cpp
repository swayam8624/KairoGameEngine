#include "../TerrainWalker.hpp"
#include <iostream>
#include <cmath>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
int main()
{
    TerrainWalkConfig config;
    const auto flat=[](double,double)->std::optional<double>{return 3.0;};
    TerrainWalker a,b;
    TerrainWalkInput forward;forward.Forward=1;
    TerrainWalkInput diagonal;diagonal.Forward=1;diagonal.Right=1;
    Check(AdvanceTerrainWalker(a,forward,.1,config,flat),"Forward movement failed");
    Check(AdvanceTerrainWalker(b,diagonal,.1,config,flat),"Diagonal movement failed");
    Check(std::abs(std::hypot(a.X-16,a.Z-16)-std::hypot(b.X-16,b.Z-16))<1e-12,"Diagonal speed differs");
    Check(a.GroundY==3&&b.GroundY==3,"Grounding failed");
    TerrainWalker blocked;
    const auto cliff=[](double x,double)->std::optional<double>{return x>16?100.0:0.0;};
    TerrainWalkInput right;right.Right=1;
    Check(!AdvanceTerrainWalker(blocked,right,.1,config,cliff)&&blocked.X==16,"Cliff crossed");
    const auto missing=[](double x,double)->std::optional<double>{if(x>16)return {};return 0.;};
    Check(!AdvanceTerrainWalker(blocked,right,.1,config,missing)&&blocked.X==16,"Unloaded terrain crossed");
    TerrainWalker walker;
    walker.X=1e9;walker.Z=-1e9;
    walker.Yaw=1.5707963267948966;
    const auto hill=[](double x,double)->std::optional<double>{return (x-1e9)*.1;};
    for(unsigned i=0;i<10000;++i)
        Check(AdvanceTerrainWalker(walker,forward,.01,config,hill),"Large-world walking failed");
    Check(std::abs(walker.X-(1e9+600.0))<.002&&std::abs(walker.GroundY-60)<.001,
        "Grounded displacement differs");
    TerrainWalkInput look;look.PitchDelta=100;look.YawDelta=100;
    (void)AdvanceTerrainWalker(walker,look,0,config,hill);
    Check(walker.Pitch==1.45&&std::abs(walker.Yaw)<=3.141592653589793,"Invalid camera angles");
    std::cout<<"10000 grounded large-world steps, normalized diagonal speed, missing cells and grade rejection passed\n";
}
