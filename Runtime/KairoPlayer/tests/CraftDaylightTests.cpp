#include "../CraftDaylight.hpp"
#include <iostream>
#include <limits>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    const auto noon=CraftDaylight::Sample(60),midnight=CraftDaylight::Sample(180);
    Check(noon.SunIntensity==3&&midnight.SunIntensity==0,"Sun intensity endpoints");
    Check(noon.Sky[2]>midnight.Sky[2]&&noon.AmbientIntensity>midnight.AmbientIntensity,"Day/night contrast");
    for(unsigned i=0;i<CraftDaylight::Steps;++i)
    {
        const auto s=CraftDaylight::Sample(i);
        const auto next=CraftDaylight::Sample((i+1)%CraftDaylight::Steps);
        for(const auto& color:{s.Sky,s.Ambient,s.SunColor})for(float c:color)
            Check(std::isfinite(c)&&c>=0&&c<=1,"Invalid color");
        double length=0;for(float c:s.SunDirection)length+=double(c)*c;
        Check(std::abs(length-1)<1e-6,"Sun direction not normalized");
        Check(s.SunIntensity>=0&&s.SunIntensity<=3&&s.AmbientIntensity>=.099f&&s.AmbientIntensity<=.351f,"Invalid light intensity");
        Check(std::abs(s.SunIntensity-next.SunIntensity)<.4f,"Discontinuous sun intensity");
        for(unsigned j=0;j<3;++j)Check(std::abs(s.Sky[j]-next.Sky[j])<.15f,"Discontinuous sky");
    }
    std::mt19937 rng(237);CraftDaylight a(.9999),b(.9999);
    for(unsigned i=0;i<100000;++i)
    {
        const double part=double(rng()%101)/1000.;
        a.Advance(.1);b.Advance(part);b.Advance(.1-part);
        Check(std::abs(a.Phase()-b.Phase())<1e-10,"Frame partition changed clock");
        Check(a.Phase()>=0&&a.Phase()<1&&a.Step()<CraftDaylight::Steps,"Clock out of range");
    }
    CraftDaylight stalled,normal;stalled.Advance(100);normal.Advance(.1);
    Check(stalled.Phase()==normal.Phase(),"Stall cap ignored");
    bool rejected=false;try{normal.Advance(std::numeric_limits<double>::infinity());}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Nonfinite time accepted");
    rejected=false;try{(void)CraftDaylight::Sample(240);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Invalid sample accepted");
    std::cout<<"240 lighting samples and 100000 frame partition comparisons passed\n";
}
