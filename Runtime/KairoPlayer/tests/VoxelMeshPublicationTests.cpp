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
    // Stage 3 dirty chunks over three updates, keeping the committed
    // scene intact even after a partial staged upload.
    auto old=p.Active();
    Check(p.BeginStaging(dirty,submit,destroy),"Begin staging");
    Check(!p.Stage(1,create)&&p.Active()==old&&visible==old&&live.size()==10,"First staged upload");
    p.AbortStaging(destroy);
    Check(!p.Staging()&&live.size()==9&&visible==old,"Stale staged upload leaked");
    Check(p.BeginStaging(dirty,submit,destroy),"Restart staging");
    Check(!p.Stage(1,create)&&!p.Stage(1,create)&&p.Stage(1,create),"Incremental budget");
    Check(p.Active()==old&&visible==old&&live.size()==12,"Staging prematurely published");
    failSubmit=true;threw=false;
    try{p.CommitStaging(submit,destroy);}catch(const std::runtime_error&){threw=true;}
    Check(threw&&p.NeedsRestore()&&p.Active()==old&&live.size()==12,"Partial staged submit lost ownership");
    failSubmit=false;
    Check(p.Recover(submit,destroy)&&visible==old&&live.size()==9,"Staged submit recovery");
    for(unsigned i=0;i<10000;++i)
    {
        old=p.Active();
        Check(p.BeginStaging(dirty,submit,destroy),"Repeated staged begin");
        Check(!p.Stage(2,create)&&p.Active()==old&&visible==old,"Staging cap or visibility changed");
        Check(p.Stage(2,create),"Staging did not finish");
        p.CommitStaging(submit,destroy);
        Check(p.Active()==visible&&live.size()==9,"Staging leaked resources");
    }
    Check(p.BeginStaging(dirty,submit,destroy),"Stage failure begin");
    failCreate=3;threw=false;
    try{(void)p.Stage(3,create);}catch(const std::runtime_error&){threw=true;}
    Check(threw&&live.size()==10,"Failed staged upload lost partial ownership");
    failCreate=-1;failDestroy=true;p.AbortStaging(destroy);
    Check(p.RetiredCount()==1&&live.size()==10,"Abort failure lost staged ownership");
    failDestroy=false;
    Check(p.Recover(submit,destroy)&&live.size()==9,"Staged abort recovery");
    std::cout<<"10000 incremental staging transactions, cancellation, upload cap and partial-submit recovery passed\n";
    Check(p.Apply(dirty,[](std::size_t)->std::optional<unsigned>{return {};},submit,destroy)&&live.size()==6,
        "Empty chunk publication failed");
    Check(p.Release(submit,destroy)==0&&live.empty(),"Cleanup leaked");
    std::cout<<"10000 three-chunk edit transactions, upload rollback, partial publication recovery, bounded retirement and empty chunks passed\n";
}
