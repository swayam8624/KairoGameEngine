#include "../CraftMining.hpp"
#include <iostream>
#include <random>

namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    CraftMining mining;
    unsigned grants=0;
    const CraftMiningTarget stone{{1,-8,3},3},grass{{1,-8,3},1},other{{2,-8,3},3};
    const auto grant=[&](const CraftMiningTarget&){++grants;return true;};
    for(int i=0;i<14;++i)Check(!mining.Advance(true,stone,.1,grant),"Premature stone mine");
    Check(mining.Progress()==18&&mining.Advance(true,stone,.1,grant)&&grants==1,"Stone completion");
    for(int i=0;i<14;++i)(void)mining.Advance(true,stone,.1,grant);
    Check(!mining.Advance(true,other,.1,grant)&&mining.Progress()==1,"Target switch retained progress");
    Check(!mining.Advance(false,other,.1,grant)&&mining.Progress()==0,"Release retained progress");
    for(int i=0;i<2;++i)(void)mining.Advance(true,grass,.1,grant);
    Check(!mining.Advance(true,grass,.1,[](const auto&){return false;})&&mining.Progress()==0,"Failed edit retained progress");
    Check(!mining.Advance(true,grass,.1,grant)&&grants==1,"Failed edit granted early retry");
    Check(!mining.Advance(true,std::nullopt,.1,grant)&&mining.Progress()==0,"Missing target retained progress");
    Check(!mining.Advance(true,stone,10.,grant)&&mining.Progress()==1,"Stall bypassed time cap");
    std::mt19937 rng(612);
    for(unsigned i=0;i<100000;++i)
    {
        mining.Reset();
        const CraftMiningTarget target{{static_cast<std::int32_t>(rng()%2000),-8,3},static_cast<std::uint16_t>(1+rng()%4)};
        const double required=CraftMiningSeconds(target.Material);
        double elapsed=0;
        unsigned commits=0;
        for(unsigned j=0;j<2000;++j)
        {
            const double dt=(1+rng()%100)/1000.;
            elapsed+=dt;
            const bool done=mining.Advance(true,target,dt,[&](const auto& actual){Check(actual==target,"Incorrect mined target");++commits;return true;});
            Check(done==(elapsed+1e-9>=required),"Randomized timing differs");
            if(done)break;
            Check(mining.Progress()<20,"Premature completed progress");
        }
        Check(commits==1&&mining.Progress()==0,"Incorrect transaction count");
    }
    std::cout<<"100000 randomized mining holds, target switching, release, stall cap and failed transaction passed\n";
}
