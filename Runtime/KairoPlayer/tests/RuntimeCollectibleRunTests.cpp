#include "../RuntimeCollectibleRun.hpp"
#include <iostream>
#include <limits>
#include <random>
using kairo::player::detail::RuntimeCollectibleRun;
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main()
{
    std::mt19937 rng(72);
    for(unsigned trial=0;trial<1000;++trial)
    {
        RuntimeCollectibleRun game;
        const unsigned count=1+rng()%64;
        for(unsigned i=0;i<count;++i)game.Add(i+1,{double(i)*2.,1.,0.});
        Check(game.Step({-10,1,0},.05).empty(),"Distant collection");
        for(unsigned i=0;i<count;++i)
        {
            const RuntimeCollectibleRun::Position p{double(i)*2.,1.,0.};
            const auto ids=game.Step(p,.1);
            Check(ids.size()==1&&ids.front()==i+1,"Wrong goal collected");
            Check(game.Step(p,.1).empty(),"Duplicate collection");
            Check(game.Score()==i+1,"Score differs");
        }
        Check(game.Won(),"Win missing");
        const auto elapsed=game.Elapsed();
        Check(game.Step({0,1,0},10.).empty()&&game.Elapsed()==elapsed,"Win state changed");
    }
    RuntimeCollectibleRun game;game.Add(1,{0,0,0});
    bool rejected=false;try{game.Add(1,{1,0,0});}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected&&game.Total()==1,"Duplicate goal accepted");
    rejected=false;try{(void)game.Step({0,0,0},std::numeric_limits<double>::infinity());}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected&&!game.Won()&&game.Score()==0,"Invalid elapsed changed state");
    std::cout<<"1000 seeded collectible runs passed\n";
}
