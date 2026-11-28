#include "../RuntimeEnemy.hpp"
#include <iostream>
#include <limits>
#include <random>
using namespace kairo::player::detail;
void Check(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
int main()
{
    RuntimeEnemy enemy({2,8,1.5,3,4,10},{{0,0,0},{4,0,0}});
    auto e=enemy.Step({0,0,0},{20,0,0},true,.1);
    Check(e.Mode==EnemyMode::Patrol&&e.Velocity[0]==2,"patrol");
    e=enemy.Step({0,0,0},{3,0,0},true,.1);
    Check(e.Mode==EnemyMode::Pursue&&e.Velocity[0]==2,"pursuit");
    e=enemy.Step({0,0,0},{1,0,0},false,.1);
    Check(e.Mode==EnemyMode::Search&&!e.Damage,"occlusion");
    e=enemy.Step({0,0,0},{1,0,0},true,.1);
    Check(e.Mode==EnemyMode::Attack&&e.Damage==10,"attack");
    for(int i=0;i<3;++i)Check(!enemy.Step({0,0,0},{1,0,0},true,.1).Damage,"cooldown");
    Check(enemy.Step({0,0,0},{1,0,0},true,.1).Damage==10,"attack repeats");
    for(int i=0;i<2;++i)Check(enemy.Step({0,0,0},{1,0,0},false,.1).Mode==EnemyMode::Search,"memory");
    Check(enemy.Step({0,0,0},{1,0,0},false,.1).Mode==EnemyMode::Patrol,"memory expiry");
    bool bad=false;try{(void)enemy.Step({0,0,0},{1,0,0},true,0);}catch(const std::invalid_argument&){bad=true;}
    Check(bad,"invalid dt");
    std::mt19937 rng(91);std::uniform_real_distribution<double> pos(-12,12);
    RuntimeEnemy random({2,8,1.5,3,4,10},{{0,0,0},{4,0,0}});
    unsigned memory=0,cooldown=0;
    for(int i=0;i<50000;++i)
    {
        const double x=pos(rng);const bool visible=rng()%2;
        if(memory)--memory;
        if(cooldown)--cooldown;
        EnemyMode expected;unsigned damage=0;
        if(visible&&std::abs(x)<=8){memory=3;expected=std::abs(x)<=1.5?EnemyMode::Attack:EnemyMode::Pursue;}
        else expected=memory?EnemyMode::Search:EnemyMode::Patrol;
        if(expected==EnemyMode::Attack&&!cooldown){damage=10;cooldown=4;}
        const auto r=random.Step({0,0,0},{x,0,0},visible,.1);
        Check(r.Mode==expected&&r.Damage==damage&&std::hypot(r.Velocity[0],r.Velocity[2])<=2.00000001,"random state parity");
    }
    std::cout<<"50000 randomized enemy samples and transition tests passed\n";
}
