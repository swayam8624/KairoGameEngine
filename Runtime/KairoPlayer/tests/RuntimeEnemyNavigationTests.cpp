#include "../RuntimeEnemyNavigation.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
void Check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int main()
{
    auto doc=ParseEnemyNav("kairo-enemy-nav 1\r\nheight 0\r\nrect -2 -2 0 2\r\nrect 0 -2 2 2\r\n");
    Check(doc.Rects.size()==2&&doc.Y==0,"valid adjacent rectangles");
    for(const auto* bad:{"bad\n","kairo-enemy-nav 1\nheight 0\n","kairo-enemy-nav 1\nheight 0\nrect 0 0 0 2\n","kairo-enemy-nav 1\nheight 0\nrect 0 0 2 2\nrect 1 1 3 3\n","kairo-enemy-nav 1\nheight 0\nrect 0 0 2 2 extra\n"})
    {bool rejected=false;try{(void)ParseEnemyNav(bad);}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"invalid mesh accepted");}
    EnemyReplanPolicy policy;
    Check(policy.Step(0,{0,0,0},false),"initial plan");
    for(int i=0;i<11;++i)Check(!policy.Step(1,{1,0,0},true),"budget bypassed");
    Check(policy.Step(1,{1,0,0},true),"retry missing");
    for(int i=0;i<12;++i)Check(!policy.Step(1,{1.1,0,0},false),"unnecessary replan");
    Check(policy.Step(1,{2,0,0},false),"moved goal ignored");
    policy.Reset();Check(policy.Step(2,{2,0,0},false),"reset ignored");
    std::mt19937 rng(92);EnemyReplanPolicy random;int last=-12;unsigned plans=0;
    for(int tick=0;tick<50000;++tick)
    {
        const bool retry=rng()%2;const double x=double(rng()%100)/10;
        if(random.Step(rng()%4,{x,0,0},retry))
        {Check(tick-last>=12,"random replan budget exceeded");last=tick;++plans;}
    }
    Check(plans>3000&&plans<=4167,"unexpected planning cadence");
    std::cout<<"50000 randomized replan ticks; "<<plans<<" plans; parser and budget passed\n";
}
