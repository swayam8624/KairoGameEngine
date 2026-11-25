#include "../CraftAutosave.hpp"
#include <iostream>
#include <limits>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
void BeforeDue(CraftAutosave& a,unsigned steps,double dt){for(unsigned i=0;i<steps;++i)Check(!a.Advance(dt),"Checkpoint due too early");}
}
int main()
{
    CraftAutosave a;CraftSaveStamp initial{};a.Initialize(initial);
    BeforeDue(a,100000,.1);Check(!a.Dirty(),"Unchanged world dirty");
    auto next=initial;next.WorldRevision=1;a.Observe(next);
    BeforeDue(a,299,.1);Check(a.Advance(.1)&&a.Dirty(),"Checkpoint missing");
    a.Failed();BeforeDue(a,49,.1);Check(a.Advance(.1)&&a.Dirty(),"Failed checkpoint lost retry");
    a.Saved();Check(!a.Dirty()&&!a.Advance(.1),"Successful checkpoint left dirty state");
    for(unsigned i=0;i<299;++i){++next.WorldRevision;a.Observe(next);Check(!a.Advance(.1),"Continuous edits advanced checkpoint");}
    ++next.WorldRevision;a.Observe(next);Check(a.Advance(.1),"Continuous edits postponed checkpoint");
    a.Saved();auto old=next;next.Selected=2;a.Observe(next);a.Observe(old);Check(!a.Dirty(),"Reverted state still dirty");
    a.Initialize(initial);next=initial;next.Equipped=true;a.Observe(next);
    Check(!a.Advance(1000),"Long stall bypassed checkpoint cap");
    bool rejected=false;try{(void)a.Advance(std::numeric_limits<double>::infinity());}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"Invalid time accepted");
    std::mt19937 rng(733);CraftAutosave b;a.Initialize(initial);b.Initialize(initial);next=initial;
    unsigned due=0;
    for(unsigned i=0;i<100000;++i)
    {
        if(rng()%13==0){++next.WorldRevision;a.Observe(next);b.Observe(next);}
        const double part=double(rng()%101)/1000.;
        const bool adue=a.Advance(.1),bfirst=b.Advance(part),blast=b.Advance(.1-part);
        Check(adue==(bfirst||blast)&&a.Dirty()==b.Dirty(),"Frame partition changed checkpoint schedule");
        if(adue){++due;if(rng()%3==0){a.Failed();b.Failed();}else{a.Saved();b.Saved();}}
    }
    Check(due>0,"Random schedule never exercised writes");
    std::cout<<"100000 idle frames, 100000 frame partitions, "<<due<<" checkpoints and retry/revert tests passed\n";
}
