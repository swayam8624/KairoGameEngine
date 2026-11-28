#include "../RuntimeProximityTriggers.hpp"
#include <iostream>
#include <limits>
#include <random>
using namespace kairo::player::detail;
void Check(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
int main()
{
    const std::string doc="kairo-triggers 1\ntrigger 3 0 0 0 1 once 9 hide\ntrigger 2 0 0 0 1 exit 8 hide\ntrigger 1 0 0 0 1 enter 8 show\n";
    RuntimeProximityTriggers t(doc);
    Check(t.Size()==3&&t.Step({2,0,0}).empty(),"initial outside");
    auto e=t.Step({1,0,0});
    Check(e.size()==2&&e[0].ID==1&&e[0].Enable&&e[1].ID==3&&!e[1].Enable,"stable boundary entry");
    Check(t.Step({0,0,0}).empty(),"repeated inside");
    bool bad=false;try{(void)t.Step({std::numeric_limits<double>::quiet_NaN(),0,0});}catch(const std::invalid_argument&){bad=true;}
    Check(bad,"invalid sample accepted");
    e=t.Step({2,0,0});Check(e.size()==1&&e[0].ID==2&&!e[0].Enable,"exit");
    e=t.Step({0,0,0});Check(e.size()==1&&e[0].ID==1,"once repeated");
    for(const auto* input:{"bad\n","kairo-triggers 1\ntrigger -1 0 0 0 1 enter 1 show\n",
        "kairo-triggers 1\ntrigger 1 0 0 0 0 enter 1 show\n",
        "kairo-triggers 1\ntrigger 1 0 0 0 1 enter 0 show\n",
        "kairo-triggers 1\ntrigger 1 0 0 0 1 enter 1 show extra\n",
        "kairo-triggers 1\ntrigger 1 0 0 0 1 enter 1 show\ntrigger 1 0 0 0 1 exit 1 hide\n"})
    {
        bad=false;try{(void)ParseTriggerRules(input);}catch(const std::invalid_argument&){bad=true;}
        Check(bad,"invalid rule accepted");
    }
    std::mt19937 rng(92);std::uniform_real_distribution<double> pos(-2,2);
    RuntimeProximityTriggers random(doc);
    bool inside=false,once=false;
    for(int i=0;i<50000;++i)
    {
        const double x=pos(rng);const bool now=std::abs(x)<=1;
        const auto events=random.Step({x,0,0});
        const std::size_t expected=now&&!inside?1+static_cast<unsigned>(!once):!now&&inside?1:0;
        Check(events.size()==expected,"random edge parity");
        if(now&&!inside){Check(events[0].ID==1,"enter mismatch");once=true;}
        if(!now&&inside)Check(events[0].ID==2,"exit mismatch");
        inside=now;
    }
    std::cout<<"50000 randomized proximity samples and validation passed\n";
}
