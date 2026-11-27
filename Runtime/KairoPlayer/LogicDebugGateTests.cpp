#include "LogicDebugGate.hpp"
#include <iostream>
#include <random>
#include <string>
using namespace kairo::player::detail;
void Check(bool value){if(!value)throw std::runtime_error("debug gate regression");}
int main(){
    LogicDebugGate gate(true);
    const LogicBreakpoint bp{"doc","rev",7};
    Check(gate.Before("doc","rev",1,bp)&&gate.Stop()==1);
    Check(!gate.Apply(2,DebugAction::Step)&&gate.Paused());
    Check(gate.Apply(1,DebugAction::Step));
    Check(gate.Before("other","rev",99,bp)&&gate.Stop()==2);
    Check(gate.Apply(2,DebugAction::Continue));
    Check(!gate.Before("doc","wrong",7,bp));
    Check(!gate.Before("other","rev",7,bp));
    Check(gate.Before("doc","rev",7,bp)&&gate.Stop()==3);
    Check(gate.Apply(3,DebugAction::Detach)&&!gate.Attached());
    Check(!gate.Before("doc","rev",7,bp));
    Check(ParseDebugCommand("3 step\n",3)==DebugAction::Step);
    Check(!ParseDebugCommand("2 step\n",3));
    Check(!ParseDebugCommand("3 step",3));
    Check(!ParseDebugCommand("3 step\nextra\n",3));
    Check(!ParseDebugCommand("3 mystery\n",3));
    LogicDebugGate capped(true);
    for(unsigned i=1;i<=4096;++i){
        Check(capped.Before("doc","rev",0,std::nullopt)&&capped.Stop()==i);
        Check(capped.Apply(i,DebugAction::Step));
    }
    Check(!capped.Before("doc","rev",0,std::nullopt)&&!capped.Attached());
    std::mt19937 rng(2026);
    for(unsigned i=0;i<10000;++i){
        const auto id=static_cast<std::uint32_t>(rng());
        Check(ParseDebugCommand(std::to_string(id)+" continue\n",id)==DebugAction::Continue);
        Check(!ParseDebugCommand(std::to_string(id)+" continue\n",id+1u));
    }
    std::cout<<"10000 token cases and breakpoint/step/limit fixtures passed\n";
}
