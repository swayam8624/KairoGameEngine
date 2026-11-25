#include "RuntimeRainStreaks.hpp"
#include <iostream>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()
{
    const RainPoint center{1e9,24,-1e9};
    for(std::uint64_t seed=0;seed<100;++seed)
        for(unsigned intensity=0;intensity<=16;++intensity)
            for(std::uint64_t tick=0;tick<16;++tick)
            {
                const auto a=RuntimeRainStreaks(seed,intensity,tick,center);
                const auto b=RuntimeRainStreaks(seed,intensity,tick+1,center);
                Check(a.size()==intensity*4&&a==RuntimeRainStreaks(seed,intensity,tick,center),"Count/determinism");
                for(std::size_t i=0;i<a.size();++i)
                {
                    Check(std::abs(a[i][0]-center[0])<=4&&std::abs(a[i][2]-center[2])<=4&&std::abs(a[i][1]-center[1])<=4,"Rain escaped volume");
                    Check(a[i][0]==b[i][0]&&a[i][2]==b[i][2]&&(b[i][1]-a[i][1]==-.5||b[i][1]-a[i][1]==7.5),"Incorrect fall/wrap");
                }
            }
    bool rejected=false;try{(void)RuntimeRainStreaks(0,17,0,center);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Bad intensity accepted");
    std::cout<<"27200 bounded rain frame cases passed\n";
}
