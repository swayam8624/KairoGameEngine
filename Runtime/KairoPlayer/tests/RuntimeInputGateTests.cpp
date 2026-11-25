#include "../RuntimeInputGate.hpp"
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
int main()
{
    RuntimeControlGate<512> keys;
    Check(!keys.Digital(32,true)&&!keys.Digital(32,false)&&keys.Digital(32,true)&&keys.Digital(32,true),"Held key rearm differs");
    keys.Reset();Check(!keys.Digital(32,true)&&!keys.Digital(512,true),"Reset or bounds differs");
    RuntimeGamepadGate pad;
    Check(pad.Axis(4,1,true)==0&&pad.Axis(4,-1,true)==0&&pad.Axis(4,1,true)==1,"Trigger startup/neutral differs");
    Check(pad.Axis(4,-1,true)==0&&pad.Axis(4,0,true)==.5f,"Trigger normalization differs");
    Check(pad.Axis(0,.8f,false)==0&&pad.Axis(0,0,false)==0&&pad.Axis(0,-.8f,false)==-.8f,"Stick rearm differs");
    pad.Reset();Check(!pad.Button(0,true)&&pad.Axis(4,1,true)==0,"Reconnect held controls accepted");
    Check(pad.Axis(4,std::numeric_limits<float>::quiet_NaN(),true)==0&&pad.Axis(4,1,true)==0,"Invalid sample armed trigger");
    Check(!pad.Button(0,false)&&pad.Button(0,true),"Button rearm differs");
    Check(NormalizeRuntimeGamepadAxis(-2,true)==0&&NormalizeRuntimeGamepadAxis(2,true)==1,"Trigger clamp differs");
    std::mt19937 rng(7481);RuntimeControlGate<16> gate;
    std::array<bool,16> seenNeutral{};unsigned accepted=0;
    for(unsigned i=0;i<100000;++i)
    {
        if(rng()%17==0){gate.Reset();seenNeutral.fill(false);}
        const auto index=rng()%16;const bool down=rng()%2;
        const bool expected=down&&seenNeutral[index];
        Check(gate.Digital(index,down)==expected,"Digital oracle differs");
        if(!down)seenNeutral[index]=true;
        if(expected)++accepted;
    }
    pad.Reset();std::array<bool,6> axisArmed{};unsigned activeAxes=0;
    for(unsigned i=0;i<100000;++i)
    {
        if(rng()%19==0){pad.Reset();axisArmed.fill(false);}
        const auto index=rng()%6;const bool trigger=index>=4;
        const float raw=(static_cast<int>(rng()%3001)-1500)/1000.f;
        const float clamped=std::max(-1.f,std::min(1.f,raw));
        const float expectedValue=trigger?(clamped+1)*.5f:clamped;
        const float expected=axisArmed[index]?expectedValue:0.f;
        Check(pad.Axis(index,raw,trigger)==expected,"Analog oracle differs");
        if(std::abs(expectedValue)<=.15f)axisArmed[index]=true;
        if(expected!=0)++activeAxes;
    }
    std::cout<<"100000 digital and 100000 analog samples passed; "<<accepted<<" held samples, "<<activeAxes<<" active axes\n";
}
