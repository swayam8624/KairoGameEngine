#include "../RuntimeCombat.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
void Check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main()
{
    const std::string doc="kairo-combat 1\nattack 30 4 5\ntarget 9 100\ntarget 2 45\n";
    RuntimeCombat combat(doc);unsigned calls=0;
    auto hit=[&](double range){Check(range==5,"range");++calls;return 2u;};
    combat.CapturePress(true);combat.CapturePress(false);
    auto e=combat.Step(hit);Check(e.Fired&&e.Damage==30&&!e.Destroyed,"first attack");
    for(int i=0;i<3;++i){combat.CapturePress(true);Check(!combat.Step(hit).Fired,"cooldown");}
    combat.CapturePress(true);e=combat.Step(hit);Check(e.Destroyed&&e.Damage==15&&calls==2,"overkill");
    for(int i=0;i<4;++i)(void)combat.Step(hit);
    combat.CapturePress(true);combat.ClearInput();Check(!combat.Step(hit).Fired,"clear input");
    combat.CapturePress(true);e=combat.Step(hit);Check(e.Fired&&!e.Damage&&calls==3,"dead target");
    for(const auto* bad:{"bad\n","kairo-combat 1\nattack 0 4 5\n","kairo-combat 1\nattack 30 0 5\n","kairo-combat 1\nattack 30 4 101\n","kairo-combat 1\nattack 30 4 5\ntarget 2 0\n","kairo-combat 1\nattack 30 4 5\ntarget 2 45\ntarget 2 45\n"})
    {bool rejected=false;try{RuntimeCombat invalid(bad);}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"invalid configuration");}
    RuntimeCombat random(doc);std::mt19937 rng(47);unsigned hp2=45,hp9=100,cd=0;
    for(int i=0;i<50000;++i)
    {
        const bool press=rng()%2;const auto id=(rng()%3==0?2u:rng()%2==0?9u:0u);
        random.CapturePress(press);if(cd)--cd;const bool fire=press&&!cd;if(fire)cd=4;
        unsigned damage=0;bool killed=false;
        if(fire&&id){auto& hp=id==2?hp2:hp9;damage=std::min(30u,hp);hp-=damage;killed=damage&&!hp;}
        unsigned queries=0;const auto result=random.Step([&](double){++queries;return id;});
        Check(result.Fired==fire&&queries==unsigned(fire)&&result.Damage==damage&&result.Destroyed==killed&&result.Target==(damage?id:0),"random parity");
        Check(random.Targets()[0].Health==hp2&&random.Targets()[1].Health==hp9,"target state");
    }
    std::cout<<"50000 randomized combat samples and validation passed\n";
}
