#include "../RuntimeHealth.hpp"
#include <iostream>
using namespace kairo::player::detail;
void Check(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
int main(){
    RuntimeHealth h("kairo-health 1\nhealth 100 3 -10\nhazard 1 3 0 0 1 20\n",{0,0,0});
    Check(h.Step({0,0,0},40).Damage==40&&h.Health()==60,"enemy damage");
    for(int i=0;i<2;++i)Check(!h.Step({0,0,0},40).Damage,"shared cooldown");
    Check(h.Step({3,0,0},40).Damage==20&&h.Health()==40,"hazard priority");
    for(int i=0;i<2;++i)(void)h.Step({0,0,0});
    const auto e=h.Step({0,0,0},10000);Check(e.Respawn&&h.Health()==100,"enemy death");
    std::cout<<"Enemy damage, hazard priority, cooldown and respawn passed\n";
}
