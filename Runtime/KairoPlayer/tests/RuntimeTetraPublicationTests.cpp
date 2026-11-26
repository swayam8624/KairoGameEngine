#include "VoxelMeshPublication.hpp"
#include <iostream>
#include <set>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()
{
    // Single-slot specialization used by SimulationLab. Model a renderer
    // that can publish the replacement before reporting an exception.
    using P=VoxelMeshPublication<unsigned,1>;
    P publication;P::Slots visible{};std::set<unsigned> live;
    unsigned next=0;bool failSubmit=false,failDestroy=false;
    const auto create=[&](std::size_t){live.insert(++next);return std::optional<unsigned>{next};};
    const auto submit=[&](const P::Slots& slots){visible=slots;if(failSubmit)throw std::runtime_error("Partial submit");};
    const auto destroy=[&](unsigned handle)
    {
        Check(visible[0]!=handle,"Attempted to destroy referenced mesh");
        if(failDestroy)throw std::runtime_error("Retirement failed");
        Check(live.erase(handle)==1,"Duplicate retirement");
    };
    Check(publication.Apply({true},create,submit,destroy)&&live.size()==1,"Initial mesh missing");
    for(unsigned tick=0;tick<10000;++tick)
    {
        const auto old=publication.Active();failSubmit=true;
        bool rejected=false;try{(void)publication.Apply({true},create,submit,destroy);}catch(const std::runtime_error&){rejected=true;}
        Check(rejected&&publication.NeedsRestore()&&publication.Active()==old&&live.size()==2,"Partial submit lost ownership");
        failSubmit=false;failDestroy=true;
        Check(!publication.Recover(submit,destroy)&&visible==old&&live.size()==2,"Restore/retirement contract");
        const auto before=next;
        Check(!publication.Apply({true},create,submit,destroy)&&next==before,"Unbounded mesh allocations");
        failDestroy=false;
        Check(publication.Recover(submit,destroy)&&live.size()==1,"Retry leaked mesh");
        Check(publication.Apply({true},create,submit,destroy)&&live.size()==1&&publication.Active()==visible,"Replacement failed");
    }
    failSubmit=true;Check(publication.Release(submit,destroy)==1&&live.size()==1,"Failed unbind destroyed mesh");
    failSubmit=false;Check(publication.Release(submit,destroy)==0&&live.empty()&&!visible[0],"Terminal cleanup failed");
    std::cout<<"10000 single-slot partial-submit/retirement recovery cycles passed\n";
}
