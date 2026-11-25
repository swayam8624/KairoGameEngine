#include "../VoxelMeshPublication.hpp"
#include <iostream>
#include <set>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    using P=VoxelMeshPublication<unsigned,9>;
    P p;
    P::Slots visible{};
    unsigned next=0,creates=0;
    std::set<unsigned> live;
    bool failSubmit=false,failDestroy=false;
    int failCreate=-1;
    const auto create=[&](std::size_t i)->std::optional<unsigned>{
        if(static_cast<int>(i)==failCreate)throw std::runtime_error("injected upload");
        ++creates;live.insert(++next);return next;
    };
    const auto submit=[&](const P::Slots& s){
        visible=s; // intentionally partial success before an exception
        if(failSubmit)throw std::runtime_error("injected publication");
    };
    const auto destroy=[&](unsigned h){
        for(auto v:visible)Check(v!=h,"Destroyed submitted handle");
        if(failDestroy)throw std::runtime_error("injected retirement");
        Check(live.erase(h)==1,"Unknown/double destruction");
    };
    std::array<bool,9> all;all.fill(true);
    Check(p.Apply(all,create,submit,destroy)&&live.size()==9,"Initial publish");
    const auto original=p.Active();
    std::array<bool,9> dirty{};dirty[1]=dirty[3]=dirty[4]=true;
    failCreate=3;
    bool threw=false;
    try{(void)p.Apply(dirty,create,submit,destroy);}catch(const std::runtime_error&){threw=true;}
    Check(threw&&p.Active()==original&&visible==original&&live.size()==9,"Upload failure changed scene");
    failCreate=-1;failSubmit=true;threw=false;
    try{(void)p.Apply(dirty,create,submit,destroy);}catch(const std::runtime_error&){threw=true;}
    Check(threw&&p.NeedsRestore()&&p.Active()==original&&live.size()==12&&p.RetiredCount()==3,
        "Partial publication did not preserve both generations");
    failSubmit=false;failDestroy=true;
    Check(!p.Recover(submit,destroy)&&visible==original&&p.RetiredCount()==3,
        "Recovery discarded failed destruction");
    const auto before=creates;
    Check(!p.Apply(dirty,create,submit,destroy)&&creates==before,"Unbounded retirement");
    failDestroy=false;
    Check(p.Recover(submit,destroy)&&live.size()==9,"Recovery leaked resources");
    for(unsigned i=0;i<10000;++i)
    {
        Check(p.Apply(dirty,create,submit,destroy)&&live.size()==9&&p.Active()==visible,
            "Repeated edit did not maintain ownership");
        for(unsigned j:{0u,2u,5u,6u,7u,8u})
            Check(p.Active()[j]==original[j],"Unaffected chunk replaced");
    }
    Check(p.Apply(dirty,[](std::size_t)->std::optional<unsigned>{return {};},submit,destroy)&&live.size()==6,
        "Empty chunk publication failed");
    Check(p.Release(submit,destroy)==0&&live.empty(),"Cleanup leaked");
    std::cout<<"10000 three-chunk edit transactions, upload rollback, partial publication recovery, bounded retirement and empty chunks passed\n";
}
