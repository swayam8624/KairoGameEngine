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
    TerrainWalker jumper;
    (void)AdvanceTerrainWalker(jumper,{},0,config,flat);
    TerrainWalkInput jump;jump.JumpPressed=true;
    double peak=0;
    for(unsigned i=0;i<240;++i)
    {
        (void)AdvanceTerrainWalker(jumper,i==0?jump:TerrainWalkInput{},1.0/120,config,flat);
        peak=std::max(peak,jumper.FootY-3);
        Check(jumper.FootY>=3,"Jump penetrated surface");
        if(i==59)
            Check(!jumper.Grounded&&std::abs(jumper.FootY-(3+2.5-.5*9.81*.25))<1e-10,
                "Ballistic trajectory differs");
    }
    Check(jumper.Grounded&&jumper.FootY==3&&jumper.VerticalVelocity==0,"Landing failed");
    Check(std::abs(peak-25/(2*9.81))<.0002,"Jump apex differs");
    TerrainWalker air;
    (void)AdvanceTerrainWalker(air,{},0,config,flat);
    (void)AdvanceTerrainWalker(air,jump,.1,config,flat);
    const double velocity=air.VerticalVelocity;
    (void)AdvanceTerrainWalker(air,jump,.1,config,flat);
    Check(air.VerticalVelocity<velocity,"Double jump accepted");
    TerrainWalker moving;
    (void)AdvanceTerrainWalker(moving,{},0,config,missing);
    jump.Right=1;
    (void)AdvanceTerrainWalker(moving,jump,.1,config,missing);
    Check(moving.X==16&&!moving.Grounded&&moving.FootY>0,"Air movement entered unloaded cell");
    const auto step=[](double x,double)->std::optional<double>{return x>16?10.0:0.0;};
    TerrainWalker wall;
    (void)AdvanceTerrainWalker(wall,jump,.1,config,step);
    Check(wall.X==16&&wall.FootY<10,"Air movement penetrated tall step");
    std::cout<<"10000 grounded large-world steps, normalized diagonal speed, missing cells and grade rejection passed\n";
    std::cout<<"Ballistic apex "<<peak<<" m, landing, no double jump and airborne barriers passed\n";
}
