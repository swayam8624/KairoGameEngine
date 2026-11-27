#include "RuntimeLogicSchedule.hpp"
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace kairo::player::detail;
static void Check(bool ok){if(!ok)throw std::runtime_error("Logic schedule regression");}
int main()
{
    const auto parsed=ParseLogicSchedule("kairo-logic-schedule 1\r\ntick 2 3\r\ntick 1 60\r\n");
    Check(parsed.size()==2&&parsed.at(1)==60&&parsed.at(2)==3);
    for(const auto* bad:{"bad", "kairo-logic-schedule 1\ntick 0 1", "kairo-logic-schedule 1\ntick 1 0", "kairo-logic-schedule 1\ntick 1 61", "kairo-logic-schedule 1\ntick 1 1\ntick 1 2", "kairo-logic-schedule 1\ntick 1 1 extra", "kairo-logic-schedule 1\ntick -1 2", "kairo-logic-schedule 1\ntick 4294967296 2"})
    {try{(void)ParseLogicSchedule(bad);Check(false);}catch(const std::invalid_argument&){}}
    std::mt19937 rng(2026);
    for(unsigned trial=0;trial<1000;++trial)
    {
        const unsigned interval=1u+unsigned(rng()%60u);
        LogicLifecycle life(interval);
        unsigned count=0,begins=0;double elapsed=0;
        for(unsigned step=0;step<500;++step)
        {
            if(rng()%5==0){life.Suspend();count=0;elapsed=0;continue;}
            if(life.NeedsBegin()){++begins;life.MarkBegun();}
            const double dt=double(1+rng()%100)/10000.;
            elapsed+=dt;++count;
            const auto result=life.Tick(dt);
            Check(bool(result)==(count==interval));
            if(result){Check(std::abs(*result-elapsed)<1e-12);count=0;elapsed=0;}
        }
        Check(begins==1&&!life.NeedsBegin());
    }
    LogicLifecycle life(2);
    Check(!life.Tick(.1));
    try{(void)life.Tick(-1);Check(false);}catch(const std::invalid_argument&){}
    Check(std::abs(*life.Tick(.2)-.3)<1e-12);
    std::cout<<"1000 seeded schedules / 500000 lifecycle steps and parser rejection fixtures passed\n";
}
