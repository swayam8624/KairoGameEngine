#include "../RuntimeHealth.hpp"
#include <iostream>
#include <limits>
#include <random>
using namespace kairo::player::detail;
void Check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main()
{
    const std::string doc="kairo-health 1\nhealth 100 4 -10\nhazard 2 3 1 0 1 80\nhazard 1 3 1 0 1 40\ncheckpoint 7 -3 1 0 1\n";
    RuntimeHealth h(doc,{0,1,0});
    Check(h.Step({-3,1,0}).Checkpoint==7,"checkpoint missing");
    Check(h.Step({3,1,0}).Damage==40&&h.Health()==60,"wrong hazard priority");
    for(int i=0;i<3;++i)Check(!h.Step({3,1,0}).Damage,"cooldown failed");
    Check(h.Step({3,1,0}).Damage==40&&h.Health()==20,"second damage failed");
    for(int i=0;i<3;++i)(void)h.Step({3,1,0});
    auto e=h.Step({3,1,0});Check(e.Respawn&&e.Damage==20&&e.Position==HealthPosition{-3,1,0}&&h.Health()==100,"death restore failed");
    Check(h.Step({0,-11,0}).Respawn,"fall kill ignored cooldown");
    bool bad=false;try{(void)h.Step({std::numeric_limits<double>::quiet_NaN(),1,0});}catch(const std::invalid_argument&){bad=true;}
    Check(bad&&h.Health()==100&&h.Checkpoint()==7,"invalid sample changed state");
    for(const auto* invalid:{"bad\n","kairo-health 1\nhealth 0 4 -10\n","kairo-health 1\nhealth 100 0 -10\n","kairo-health 1\nhealth 100 4 -10\nhazard 1 0 1 0 1 1\n","kairo-health 1\nhealth 100 4 -10\nhazard 1 3 1 0 1 10\ncheckpoint 2 3 1 0 1\n","kairo-health 1\nhealth 100 4 -10\ncheckpoint 1 3 -11 0 1\n"})
    {bad=false;try{RuntimeHealth reject(invalid,{0,1,0});}catch(const std::invalid_argument&){bad=true;}Check(bad,"invalid rules accepted");}
    RuntimeHealth random(doc,{0,1,0});unsigned health=100,cooldown=0;std::uint32_t checkpoint=0;
    std::mt19937 rng(42);
    for(int i=0;i<50000;++i)
    {
        const auto action=rng()%4;const HealthPosition p=action==0?HealthPosition{3,1,0}:action==1?HealthPosition{-3,1,0}:action==2?HealthPosition{0,-11,0}:HealthPosition{0,1,0};
        if(cooldown)--cooldown;
        const unsigned damage=action==2?health:action==0&&!cooldown?std::min(40u,health):0;
        health-=damage;if(damage)cooldown=4;const bool dead=!health;if(dead)health=100;
        if(!damage&&action==1)checkpoint=7;
        const auto result=random.Step(p);
        Check(result.Damage==damage&&result.Respawn==dead&&random.Health()==health&&random.Checkpoint()==checkpoint,"random state mismatch");
        if(dead)Check(result.Position==(checkpoint?HealthPosition{-3,1,0}:HealthPosition{0,1,0}),"random respawn mismatch");
    }
    std::cout<<"50000 randomized health samples and validation passed\n";
}
