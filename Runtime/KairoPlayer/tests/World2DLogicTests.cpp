#include "../World2DLogic.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::logic2d;
void Assert(bool ok){if(!ok)throw std::runtime_error("World2D logic regression");}
int main()
{
    const std::string source="kairo-logic2d 1\nrules 5\nmain timer 60 add 0 2\nmain input Jump impulse 6 0 -60\nmain sensor_begin 8 set 1 5\nmain ui pause sound 0\nmain begin print \"ready\"\n";
    const auto doc=Parse(source);
    Assert(doc.Rules.size()==5&&doc.Rules[1].Actor==6&&doc.Rules[1].Y==-60);
    Assert(Matches(doc.Rules[1],"main",{Event::Input,"Jump"},0));
    Assert(!Matches(doc.Rules[1],"main",{Event::Input,"Move"},0));
    Assert(Matches(doc.Rules[2],"main",{Event::SensorBegin,{},8},0));
    Assert(!Matches(doc.Rules[2],"main",{Event::SensorEnd,{},8},0));
    Assert(Matches(doc.Rules[3],"main",{Event::UI,"pause"},0));
    State state;
    for(unsigned tick=0;tick<=600;++tick)
        if(Matches(doc.Rules[0],"main",{Event::Timer},tick))state.Apply(doc.Rules[0]);
    Assert(state.Value(0)==20);state.Apply(doc.Rules[2]);Assert(state.Value(1)==5);
    state.Clear();Assert(state.Value(0)==0&&state.Value(1)==0);
    const auto reject=[](std::string_view bytes){try{(void)Parse(bytes);}catch(const std::invalid_argument&){return;}throw std::runtime_error("Malformed script accepted");};
    reject(source+"junk");reject("kairo-logic2d 1\nrules 1\nmain timer 0 add 0 1\n");
    reject("kairo-logic2d 1\nrules 1\nmain tick add 16 1\n");
    reject("kairo-logic2d 1\nrules 1\nmain tick add 0 1.5\n");
    reject("kairo-logic2d 1\nrules 1\nmain begin sound 3\n");
    reject("kairo-logic2d 1\nrules 1\nmain input ../bad sound 0\n");
    reject("kairo-logic2d 1\nrules 1\nmain begin impulse 6 nan 0\n");
    std::mt19937 rng(2026);
    for(unsigned i=0;i<10000;++i)
    {
        const unsigned period=1+rng()%36000,slot=rng()%16,tick=rng()%100000;
        const int value=int(rng()%2000001)-1000000;
        const auto bytes="kairo-logic2d 1\nrules 1\nmain timer "+std::to_string(period)+" add "+std::to_string(slot)+" "+std::to_string(value)+"\n";
        const auto r=Parse(bytes).Rules.front();
        Assert(Matches(r,"main",{Event::Timer},tick)==(tick!=0&&tick%period==0));
        Assert(!Matches(r,"climb",{Event::Timer},tick));
        const long long expected=static_cast<long long>(state.Value(slot))+value;
        state.Apply(r);Assert(state.Value(slot)==(expected<-1000000?-1000000:expected>1000000?1000000:expected));
    }
    std::cout<<"10000 event/timer/variable cases plus malformed rule and dispatch fixtures passed\n";
}
