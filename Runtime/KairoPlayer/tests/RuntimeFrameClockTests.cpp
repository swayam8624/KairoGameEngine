#include "../RuntimeFrameClock.hpp"
#include <iostream>
#include <limits>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F> bool Reject(F&& f){try{(void)f();return false;}catch(const std::invalid_argument&){return true;}}
}
int main()
{
    RuntimeFrameClock clock;
    Check(clock.Sample(0,true)==0&&clock.Sample(.02,true)==.02,"Startup time differs");
    Check(clock.Sample(120,true)==.1,"Stall was not capped");
    Check(clock.Sample(121,false)==0&&clock.Sample(240,false)==0&&clock.Sample(241,true)==0,"Inactive time replayed");
    Check(clock.Sample(241.015,true)>0,"Resume clock not advancing");
    Check(Reject([&]{return clock.Sample(200,true);})&&Reject([&]{return clock.Sample(std::numeric_limits<double>::infinity(),true);}),"Invalid samples accepted");
    Check(std::abs(clock.Sample(241.025,true)-.01)<1e-12,"Invalid sample mutated clock");
    RuntimeFrameClock randomClock;std::mt19937 rng(718);double time=0,previous=0,total=0,reference=0;
    bool wasActive=false;unsigned stalls=0,resumes=0;
    for(unsigned i=0;i<100000;++i)
    {
        const bool active=rng()%5!=0;
        time+=rng()%17==0?10.:double(rng()%31)/1000.;
        const double expected=active&&wasActive?std::min(.1,time-previous):0.;
        const double actual=randomClock.Sample(time,active);
        Check(actual==expected&&actual>=0&&actual<=.1,"Clock oracle differs");
        if(actual==.1)++stalls;
        if(active&&!wasActive)++resumes;
        total+=actual;reference+=expected;previous=time;wasActive=active;
    }
    Check(total==reference&&stalls&&resumes,"Random coverage missing");
    std::cout<<"100000 clock samples, "<<stalls<<" capped stalls, "<<resumes<<" resumes passed\n";
}
