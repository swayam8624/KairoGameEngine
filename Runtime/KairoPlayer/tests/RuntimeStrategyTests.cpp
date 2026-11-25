#include "RuntimeStrategy.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void Invariants(const RuntimeStrategy& g)
{
    unsigned total=g.Stored();
    for(std::size_t i=0;i<g.Units().size();++i)
    {
        const auto& u=g.Units()[i];total+=u.Cargo;
        Check(u.Cargo<=RuntimeStrategy::CarryLimit&&g.Rows().at(u.Position.Y).at(u.Position.X)=='.',"Worker bounds");
        for(std::size_t j=0;j<i;++j)Check(u.Position!=g.Units()[j].Position,"Worker overlap");
    }
    for(int y=0;y<int(g.Rows().size());++y)for(int x=0;x<int(g.Rows()[y].size());++x)total+=g.Stock({x,y});
    Check(total==6,"Resource conservation");
}
int main()
{
    RuntimeStrategy game(StrategyDemoLevel());
    // Move the idle worker away from base access, then send the second
    // worker to gather two full loads through the wall's only gap.
    game.Move(0,{2,1});game.Gather(1,{8,1});
    for(unsigned i=0;i<100&&!game.Won();++i){game.Tick();Invariants(game);}
    Check(game.Won()&&game.Stored()==6,"Strategy route failed");
    const auto won=game.Units();game.Tick();Check(game.Units()==won,"Terminal tick changed state");
    std::mt19937 rng(149);
    for(unsigned trial=0;trial<100;++trial)
    {
        RuntimeStrategy a(StrategyDemoLevel()),b(StrategyDemoLevel());
        for(unsigned i=0;i<300;++i)
        {
            const auto id=rng()%2;const StrategyCell p{int(rng()%8)+1,int(rng()%4)+1};
            if(a.Rows()[p.Y][p.X]=='.'&&rng()%3==0){a.Move(id,p);b.Move(id,p);}
            if(a.Stock({8,1})&&rng()%5==0){a.Gather(id,{8,1});b.Gather(id,{8,1});}
            a.Tick();b.Tick();Invariants(a);
            Check(a.Units()==b.Units()&&a.Stored()==b.Stored(),"Strategy replay divergence");
        }
    }
    bool rejected=false;try{game.Move(0,{4,1});}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Wall order accepted");
    std::cout<<"Complete strategy route, resource conservation and 30000 replay ticks passed\n";
}
