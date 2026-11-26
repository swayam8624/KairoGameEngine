#include "../RuntimeSlidingDoor.hpp"
#include <iostream>
#include <random>
#include <limits>
using namespace kairo::player::detail;
void Check(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
int main()
{
    const auto rules=ParseDoorRules("kairo-doors 1\r\ndoor 5 3 2 4\r\ndoor 1 2 1 3\r\n");
    Check(rules.size()==2&&rules[0].Target==1,"Rule order");
    RuntimeSlidingDoor door(rules[1],{2,1,3},2);
    for(int i=0;i<120;++i)(void)door.Step({2,100,3},1./60);
    Check(door.Offset()==3&&door.Position()[1]==4,"Full opening");
    for(int i=0;i<120;++i)(void)door.Step({7,1,3},1./60);
    Check(door.Offset()==0,"Full closing");
    (void)door.Step({2,1,3},.25);
    Check(door.Offset()==.5,"Partial opening");
    bool bad=false;try{(void)door.Step({0,0,0},std::numeric_limits<double>::infinity());}catch(const std::invalid_argument&){bad=true;}
    Check(bad&&door.Offset()==.5,"Invalid sample mutated state");
    for(const auto* doc:{"bad\n","kairo-doors 1\ndoor 0 3 2 4\n","kairo-doors 1\ndoor 5 0 2 4\n","kairo-doors 1\ndoor 5 3 11 4\n","kairo-doors 1\ndoor 5 3 2 4 extra\n","kairo-doors 1\ndoor 5 3 2 4\ndoor 5 3 2 4\n"})
    {
        bad=false;try{(void)ParseDoorRules(doc);}catch(const std::invalid_argument&){bad=true;}
        Check(bad,"Invalid document accepted");
    }
    bad=false;try{RuntimeSlidingDoor invalid(rules[1],{0,0,0},4);}catch(const std::invalid_argument&){bad=true;}
    Check(bad,"Insufficient clearance accepted");
    std::mt19937 rng(42);std::uniform_real_distribution<double> x(-8,8),dt(0,.25);
    RuntimeSlidingDoor random(rules[1],{0,0,0},2);double expected=0;
    for(int i=0;i<50000;++i)
    {
        const double p=x(rng),t=dt(rng);
        expected=std::clamp(expected+(std::abs(p)<=4?2*t:-2*t),0.,3.);
        const auto pose=random.Step({p,0,0},t);
        Check(std::abs(pose[1]-expected)<1e-12&&pose[0]==0&&pose[2]==0,"Random travel parity");
    }
    std::cout<<"50000 randomized fixed door samples and validation passed\n";
}
