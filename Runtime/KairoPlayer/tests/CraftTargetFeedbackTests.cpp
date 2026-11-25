#include "../CraftTargetFeedback.hpp"
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    CraftTargetPublication cache;
    unsigned publications=0;
    const auto submit=[&](const auto&){++publications;};
    Check(cache.Refresh(std::nullopt,submit)&&!cache.Refresh(std::nullopt,submit),"Empty target republished");
    CraftTargetKey key{{1000000000,7,-1000000000},{999999984,0,-1000000016},true,true,CraftTargetCell{1000000001,7,-1000000000}};
    Check(cache.Refresh(key,submit),"Target not published");
    for(unsigned i=0;i<100000;++i)Check(!cache.Refresh(key,submit),"Idle target republished");
    Check(publications==2,"Idle upload count differs");
    auto changed=key;changed.Placeable=false;
    bool failed=false;
    try{cache.Refresh(changed,[](const auto&){throw std::runtime_error("Injected submission failure");});}catch(const std::runtime_error&){failed=true;}
    Check(failed&&!cache.Refresh(key,submit)&&cache.Refresh(changed,submit),"Failed publication lost old state");
    const auto blocked=CraftBuildTargetBoxes(changed),valid=CraftBuildTargetBoxes(key);
    Check(blocked.Placement&&valid.Placement&&blocked.Placement->Color!=valid.Placement->Color,"Placement colors identical");
    Check(cache.Refresh(std::nullopt,submit)&&!cache.Refresh(std::nullopt,submit),"Lost target not cleared");
    std::mt19937 rng(117);
    for(unsigned i=0;i<100000;++i)
    {
        CraftTargetCell origin{},cell{};
        for(unsigned j=0;j<3;++j){origin[j]=int(rng()%2000000001u)-1000000000;cell[j]=origin[j]+int(rng()%129)-64;}
        const auto box=CraftFeedbackBox(cell,origin,{1,1,1});
        for(unsigned j=0;j<3;++j)
        {
            const double p=double(std::int64_t(cell[j])-origin[j]);
            Check(std::abs(box.Min[j]-(p-.015))<1e-5&&std::abs(box.Max[j]-(p+1.015))<1e-5,"Rebased box differs");
        }
    }
    std::cout<<"100000 idle target frames, 100000 rebased boxes, color state and submission rollback passed\n";
}
