#include "../RuntimeGameplayEvents.hpp"
#include "../RuntimeSlidingDoor.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
void Check(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
int main()
{
    const auto rule=ParseDoorRules("kairo-doors 1\ndoor 40 3 2 4 locked\n").at(0);
    RuntimeSlidingDoor door(rule,{0,1,0},2);
    RuntimeGameplayEvents events("kairo-events 1\non 1 collected 30 1 once door-open 40\non 2 trigger 7 1 repeat door-lock 40\non 3 trigger 8 1 repeat door-auto 40\n");
    auto dispatch=[&](GameplayEvent event,unsigned source){
        for(const auto& action:events.Dispatch(event,source))
        {
            Check(action.Target==40,"bad binding");
            switch(action.Kind){
                case GameplayActionKind::DoorOpen:door.SetControl(DoorControl::HoldOpen);break;
                case GameplayActionKind::DoorLock:door.SetControl(DoorControl::Locked);break;
                case GameplayActionKind::DoorAutomatic:door.SetControl(DoorControl::Automatic);break;
                default:throw std::runtime_error("unexpected action");
            }
        }
    };
    const TriggerPosition near{0,1,1},far{8,1,0};
    for(int i=0;i<16;++i)(void)door.Step(near,.25);
    Check(door.Offset()==0,"locked door opened by proximity");
    dispatch(GameplayEvent::Collected,30);
    for(int i=0;i<16;++i)(void)door.Step(far,.25);
    Check(door.Offset()==3,"event did not latch open");
    dispatch(GameplayEvent::Trigger,7);
    for(int i=0;i<16;++i)(void)door.Step(near,.25);
    Check(door.Offset()==3,"lock closed gate near player");
    for(int i=0;i<16;++i)(void)door.Step(far,.25);
    Check(door.Offset()==0,"lock did not close after player left");
    dispatch(GameplayEvent::Collected,30);
    (void)door.Step(near,.25);Check(door.Offset()==0,"one-shot fired again");
    dispatch(GameplayEvent::Trigger,8);
    (void)door.Step(near,.25);Check(door.Offset()==.5,"auto did not resume");
    std::mt19937 rng(75);double expected=.5;auto mode=DoorControl::Automatic;
    for(int i=0;i<50000;++i)
    {
        const auto cmd=rng()%4;
        if(cmd==0){mode=DoorControl::Locked;door.SetControl(mode);}
        if(cmd==1){mode=DoorControl::HoldOpen;door.SetControl(mode);}
        if(cmd==2){mode=DoorControl::Automatic;door.SetControl(mode);}
        const bool nearby=rng()%2;
        const bool open=mode==DoorControl::HoldOpen||(nearby&&(mode==DoorControl::Automatic||expected>0));
        expected=std::clamp(expected+(open?.5:-.5),0.,3.);
        (void)door.Step(nearby?near:far,.25);
        Check(door.Offset()==expected,"random gate state parity");
    }
    bool rejected=false;try{(void)ParseDoorRules("kairo-doors 1\ndoor 40 3 2 4 locked extra\n");}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"invalid initial mode accepted");
    std::cout<<"50000 randomized gate control samples and event chain passed\n";
}
