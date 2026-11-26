#include "../RuntimeGameplayEvents.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
void Check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int main()
{
    const std::string doc="kairo-events 1\non 4 trigger 7 3 repeat hide 9\non 2 trigger 7 2 once show 8\non 1 begin 0 1 once show 9\non 3 collected 6 1 once hide 10\non 5 trigger 7 3 repeat show 9\n";
    RuntimeGameplayEvents events(doc);
    Check(events.Dispatch(GameplayEvent::Begin,0).size()==1,"begin");
    Check(events.Dispatch(GameplayEvent::Begin,0).empty(),"begin repeated");
    Check(events.Dispatch(GameplayEvent::Collected,6).size()==1,"collect");
    Check(events.Dispatch(GameplayEvent::Trigger,8).empty(),"wrong source");
    Check(events.Dispatch(GameplayEvent::Trigger,7).empty(),"early threshold");
    auto a=events.Dispatch(GameplayEvent::Trigger,7);
    Check(a.size()==1&&a[0].Rule==2&&a[0].Enable,"once threshold");
    a=events.Dispatch(GameplayEvent::Trigger,7);
    Check(a.size()==2&&a[0].Rule==4&&!a[0].Enable&&a[1].Rule==5&&a[1].Enable,"stable order");
    for(int i=0;i<2;++i)Check(events.Dispatch(GameplayEvent::Trigger,7).empty(),"early repeat");
    Check(events.Dispatch(GameplayEvent::Trigger,7).size()==2,"repeat threshold");
    for(const auto* invalid:{"bad\n","kairo-events 1\non 1 begin 2 1 once show 9\n","kairo-events 1\non 1 defeated 0 1 once show 9\n","kairo-events 1\non 1 trigger 7 0 once show 9\n","kairo-events 1\non 1 trigger 7 1 loop show 9\n","kairo-events 1\non 1 trigger 7 1 once spawn 9\n","kairo-events 1\non 1 trigger 7 1 once show 9 extra\n","kairo-events 1\non 1 begin 0 1 once show 9\non 1 begin 0 1 once hide 9\n"})
    {bool rejected=false;try{RuntimeGameplayEvents bad(invalid);}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"invalid accepted");}
    RuntimeGameplayEvents random(doc);std::mt19937 rng(73);unsigned seen=0;
    for(int i=0;i<50000;++i)
    {
        const auto id=rng()%2?7u:8u;if(id==7)++seen;
        const auto result=random.Dispatch(GameplayEvent::Trigger,id);
        const auto expected=id!=7?0u:seen==2?1u:seen%3==0?2u:0u;
        Check(result.size()==expected,"random dispatch count");
        if(expected==2)Check(result[0].Rule==4&&result[1].Rule==5,"random stable order");
    }
    std::cout<<"50000 randomized event dispatches and validation passed\n";
}
