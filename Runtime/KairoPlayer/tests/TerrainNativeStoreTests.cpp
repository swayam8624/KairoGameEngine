#include "../TerrainNativeStore.hpp"
#include <iostream>
#include <set>
#include <vector>

namespace {
using namespace kairo::player::detail;
struct Request{int Coordinate;};
struct Plan{std::vector<Request> Loads,Unloads;};
struct Runtime{
    std::map<int,bool> Completed;
    void CompleteLoad(int c,bool ok){if(!Completed.emplace(c,ok).second)throw std::logic_error("Double completion");}
    void CompleteUnload(int c,bool ok){CompleteLoad(c,ok);}
};
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    TerrainNativeStore<int,unsigned> store;
    Runtime runtime;
    std::set<unsigned> live;
    unsigned next=0;
    bool detached=false,failDetach=false,failDestroy=false;
    const auto create=[&](int c){
        if(c%7==0)throw std::runtime_error("Upload failed");
        live.insert(++next);return next;
    };
    const auto detach=[&]{if(failDetach)throw std::runtime_error("Detach failed");detached=true;};
    const auto destroy=[&](unsigned handle){
        Check(detached,"Destroy occurred before detach");
        if(failDestroy&&handle%3==0)throw std::runtime_error("Retirement failed");
        Check(live.erase(handle)==1,"Stale or double destroy");
    };
    Plan plan;
    for(int i=1;i<=10000;++i)plan.Loads.push_back({i});
    auto apply=[&]{runtime.Completed.clear();detached=false;
        return ApplyTerrainNativePlan(store,runtime,plan,detach,create,destroy);};
    auto stats=apply();
    Check(stats.Loaded==8572&&stats.FailedLoads==1428&&live.size()==8572,"Upload journal mismatch");
    plan.Loads={{10001}};
    for(const auto& [cell,handle]:store.Cells()){(void)handle;plan.Unloads.push_back({cell});}
    failDetach=true;
    stats=apply();
    Check(stats.DetachFailed&&stats.FailedLoads==1&&stats.FailedUnloads==8572&&
        store.Cells().size()==8572&&live.size()==8572&&runtime.Completed.size()==8573,
        "Detach failure abandoned reservations or handles");
    failDetach=false;failDestroy=true;
    stats=apply();
    Check(stats.Unloaded==5715&&stats.FailedUnloads==2857&&stats.Loaded==1,
        "Partial retirement result differs");
    Check(store.Cells().size()==2858&&live.size()==2858,"Failed destroy lost ownership");
    failDestroy=false;failDetach=true;
    Check(store.Release(detach,destroy)==2858&&live.size()==2858,"Shutdown detached incorrectly");
    failDetach=false;
    Check(store.Release(detach,destroy)==0&&live.empty(),"Retry leaked handles");
    Check(store.Release(detach,destroy)==0,"Repeated shutdown failed");
    // A replacement must not invalidate the old submitted mesh before
    // the new upload and submission succeed. Test a bounded journal.
    TerrainNativeStore<int,unsigned> lod(2);
    lod.Load(1,create);
    const auto old=lod.Cells().at(1);
    bool rejected=false;
    try{(void)lod.Replace(1,[](int)->unsigned{throw std::runtime_error("LOD upload failed");});}
    catch(const std::runtime_error&){rejected=true;}
    Check(rejected&&lod.Cells().at(1)==old&&live.contains(old)&&lod.RetiredCount()==0,
        "Failed LOD upload changed visible ownership");
    Check(lod.Replace(1,create)&&lod.Replace(1,create),"Replacement failed");
    Check(live.size()==3&&lod.RetiredCount()==2&&live.contains(old),"Old mesh retired prematurely");
    const auto newest=lod.Cells().at(1);
    Check(!lod.Replace(1,create)&&live.size()==3&&lod.Cells().at(1)==newest,
        "Full journal allocated or replaced a mesh");
    failDetach=true;
    Check(lod.Release(detach,destroy)==3&&live.size()==3,"Failed detach lost retired ownership");
    failDetach=false;
    detach();
    Check(lod.ReleaseRetired([](unsigned){throw std::runtime_error("Retirement failed");})==2,
        "Failed retirement lost old handles");
    Check(lod.ReleaseRetired(destroy)==0&&live.size()==1&&live.contains(newest),
        "Retirement did not preserve replacement");
    for(unsigned i=0;i<10000;++i)
    {
        Check(lod.Replace(1,create),"Replacement journal exhausted");
        Check(lod.ReleaseRetired(destroy)==0&&live.size()==1,"Replacement leaked native handle");
    }
    Check(lod.Release(detach,destroy)==0&&live.empty(),"LOD shutdown leaked handles");
    std::cout<<"10000 uploads, detach rollback, 2857 failed retirements and retry passed\n";
    std::cout<<"10000 LOD replacements, failed upload, full journal and retirement retry passed\n";
}
