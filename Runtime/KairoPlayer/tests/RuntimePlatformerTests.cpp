#include "RuntimePlatformer.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()
{
    RuntimePlatformer game(PlatformerDemoLevel());
    // Walk directly into the hazard, verifying death and coin reset.
    for(unsigned i=0;i<50&&game.Current().Deaths==0;++i)game.Step({1,false});
    Check(game.Current().Deaths==1&&game.Current().Coins==0,"Hazard reset failed");
    // Walk to the first coin, jump across the hazard, collect the second
    // coin, then reach the goal. This is an actual complete level trajectory.
    for(unsigned i=0;i<31;++i)game.Step({1,false});
    Check(game.Current().Coins==1&&game.Current().Grounded,"First coin not collected");
    for(unsigned i=0;i<17;++i)game.Step({1,true});
    for(unsigned i=0;i<80&&!game.Current().Won;++i)game.Step({1,false});
    Check(game.Current().Won&&game.Current().Coins==2&&game.Current().Deaths==1,"Level completion failed");
    const auto won=game.Current();game.Step({-1,true});Check(game.Current()==won,"Won state mutated");
    // Independent deterministic replay: random controls remain within map,
    // never overlap a solid cell and reproduce every tick and collectible.
    std::mt19937 rng(812);
    for(unsigned trial=0;trial<100;++trial)
    {
        RuntimePlatformer a(PlatformerDemoLevel()),b(PlatformerDemoLevel());
        for(unsigned tick=0;tick<500;++tick)
        {
            const RuntimePlatformer::Input input{int(rng()%3)-1,rng()%7==0};
            a.Step(input);b.Step(input);const auto& s=a.Current();
            Check(s==b.Current()&&a.Rows()==b.Rows(),"Replay diverged");
            for(int y=s.Y/16;y<=(s.Y+RuntimePlatformer::Height-1)/16;++y)
                for(int x=s.X/16;x<=(s.X+RuntimePlatformer::Width-1)/16;++x)
                    Check(a.Rows().at(y).at(x)!='#',"Solid penetration");
        }
    }
    bool rejected=false;try{RuntimePlatformer invalid({"###","#S#","###"});}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Missing goal accepted");
    std::cout<<"Complete level, hazard respawn and 50000 deterministic platformer ticks passed\n";
}
