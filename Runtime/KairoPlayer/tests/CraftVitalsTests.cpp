#include "../CraftVitals.hpp"
#include <iostream>
#include <random>

namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
int main()
{
    CraftVitals v;
    v.Respawn(0);
    Check(v.Observe(1.3,false,false)==0&&v.Observe(0,true,true)==0,"Normal jump damaged player");
    Check(v.Observe(-5,false,false)==0&&v.Observe(-5,true,true)==2&&v.Health()==18,"Fall damage differs");
    Check(v.Observe(-5,true,true)==0,"Landing charged repeatedly");
    Check(v.Observe(-20,true,false)==0&&v.Health()==18,"Streaming boundary caused damage");
    Check(v.Observe(-60,true,true)==18&&v.Dead(),"Lethal damage did not saturate");
    Check(v.Observe(-100,true,true)==0,"Dead player took further damage");
    v.Respawn(12);Check(v.Health()==20&&!v.Dead()&&v.Observe(12,true,true)==0,"Respawn did not reset fall state");
    std::mt19937 rng(871);
    for(unsigned i=0;i<100000;++i)
    {
        const double start=double(int(rng()%2000)-1000),drop=double(rng()%6000)/100.;
        v.Respawn(start);
        const bool supported=i%5!=0;
        for(unsigned j=1;j<10;++j)Check(v.Observe(start-drop*j/10.,false,false)==0,"Airborne damage");
        const auto expected=supported?static_cast<std::uint32_t>(std::clamp(std::ceil(start-(start-drop)-3.),0.,20.)):0u;
        Check(v.Observe(start-drop,true,supported)==expected&&v.Health()==20-expected,"Fall reference mismatch");
        Check(v.Observe(start-drop,true,supported)==0,"Duplicate damage");
    }
    std::cout<<"100000 randomized falls, normal jump, virtual boundary, lethal damage and respawn passed\n";
}
