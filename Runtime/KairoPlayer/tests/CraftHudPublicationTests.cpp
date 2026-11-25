#include "../CraftHudPublication.hpp"
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
using namespace kairo::player::detail;
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main()
{
    CraftHudPublication cache;
    CraftHudKey key{true,{1,2,3,4},1,960,540};
    unsigned submitted=0;
    const auto submit=[&](){++submitted;};
    Check(cache.Refresh(key,submit),"Initial publication missing");
    for(unsigned i=0;i<100000;++i)Check(!cache.Refresh(key,submit),"Idle HUD republished");
    Check(submitted==1,"Idle submission count");
    auto changed=key;changed.Counts[0]++;
    bool failed=false;
    try{cache.Refresh(changed,[]{throw std::runtime_error("Injected submit failure");});}
    catch(const std::runtime_error&){failed=true;}
    Check(failed&&!cache.Refresh(key,submit),"Failure replaced previous key");
    Check(cache.Refresh(changed,submit),"Failed update was not retried");
    std::optional<CraftHudKey> expected=changed;
    std::mt19937 rng(672);
    unsigned changes=0;
    for(unsigned i=0;i<100000;++i)
    {
        if(i%7==0)key.Enabled=!key.Enabled;
        if(i%3==0)key.Counts[rng()%4]=rng()%1000;
        if(i%11==0)key.Selected=1+rng()%4;
        if(i%13==0){key.Width=1+rng()%8192;key.Height=1+rng()%8192;}
        const auto normalized=key.Enabled?key:CraftHudKey{};
        const bool dirty=expected!=normalized;
        Check(cache.Refresh(key,submit)==dirty,"Cache differs from reference");
        if(dirty){expected=normalized;++changes;}
    }
    Check(submitted==changes+2,"Unexpected publication count");
    CraftHudPublication healthCache;
    auto healthKey=CraftHudKey{true,{1,2,3,4},1,960,540};
    unsigned healthUpdates=0;
    const auto healthSubmit=[&](){++healthUpdates;};
    Check(healthCache.Refresh(healthKey,healthSubmit),"Missing initial health");
    healthKey.Health=19;
    Check(healthCache.Refresh(healthKey,healthSubmit)&&!healthCache.Refresh(healthKey,healthSubmit),
        "Health-only change did not publish exactly once");
    Check(healthUpdates==2,"Health publication count");
    std::cout<<"100000 idle frames, 100000 state transitions and failure retry passed; "<<changes<<" state publications\n";
}
