#include "../CraftCheckpoint.hpp"
#include "../CraftVitals.hpp"
#include "../CraftNutrition.hpp"
#include "../CraftAutosave.hpp"
#include <iostream>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
template<class F> bool Reject(F&& f){try{f();return false;}catch(const std::invalid_argument&){return true;}}
}
int main()
{
    const auto flat=[](VoxelCoordinate c)->std::optional<VoxelMaterial>{return c.Y<0?1:0;};
    CraftCheckpoint state;state.Feet={.5,0,.5};
    auto restored=RestoreCraftCheckpoint(state,VoxelPlayerConfig{},flat);
    Check(restored&&restored->Grounded&&restored->Feet==state.Feet,"Valid spawn not restored");
    state.Feet[1]=1;Check(!RestoreCraftCheckpoint(state,VoxelPlayerConfig{},flat),"Airborne restore accepted");
    state.Feet[1]=0;
    Check(!RestoreCraftCheckpoint(state,VoxelPlayerConfig{},[](VoxelCoordinate c)->std::optional<VoxelMaterial>{return c.Y<0?std::nullopt:std::optional<VoxelMaterial>{0};}),"Unknown floor accepted");
    Check(!RestoreCraftCheckpoint(state,VoxelPlayerConfig{},[](VoxelCoordinate)->std::optional<VoxelMaterial>{return 1;}),"Blocked restore accepted");
    CraftVitals vitals;CraftNutrition nutrition;
    vitals.Restore(7,0);nutrition.Restore(4);
    Check(vitals.Health()==7&&nutrition.Hunger()==4&&!nutrition.CanSprint(),"Survival restore differs");
    Check(vitals.Observe(0,true,true)==0,"Restore created false fall damage");
    Check(Reject([&]{vitals.Restore(0,0);})&&vitals.Health()==7,"Invalid health mutated state");
    Check(Reject([&]{nutrition.Restore(21);})&&nutrition.Hunger()==4,"Invalid hunger mutated state");
    CraftAutosave autosave;CraftSaveStamp stamp{};autosave.Initialize(stamp);
    stamp.Player=state;autosave.Observe(stamp);Check(autosave.Dirty(),"Legacy upgrade not dirty");
    autosave.Saved();stamp.Player->Health=7;autosave.Observe(stamp);Check(autosave.Dirty(),"Health change not dirty");
    autosave.Saved();stamp.Player->Feet[0]+=1;autosave.Observe(stamp);Check(autosave.Dirty(),"Pose change not dirty");
    std::mt19937 rng(573);
    // Synthetic v5-tagged bytes test the outer codec. Full nested v5/v3/v4
    // world validation belongs to the caller and its own codec tests.
    std::vector<std::uint8_t> payload(CraftCheckpointInnerMagic.begin(),CraftCheckpointInnerMagic.end());
    payload.resize(100);
    for(unsigned trial=0;trial<10000;++trial)
    {
        state.Feet={double(int(rng()%1999999800u)-999999900)+.5,0,double(int(rng()%1999999800u)-999999900)+.5};
        state.Yaw=(double(rng()%6001)-3000)/1000;state.Pitch=(double(rng()%2901)-1450)/1000;
        state.DayPhase=double(rng()%100000)/100000;state.Health=1+rng()%20;state.Hunger=rng()%21;
        for(std::size_t i=8;i<payload.size();++i)payload[i]=static_cast<std::uint8_t>(rng());
        const auto bytes=EncodeCraftCheckpoint(payload,state,100);
        const auto result=DecodeCraftCheckpoint(bytes,100);
        Check(result.State==state&&std::equal(result.Payload.begin(),result.Payload.end(),payload.begin(),payload.end()),"Roundtrip differs");
        Check(EncodeCraftCheckpoint(result.Payload,result.State,100)==bytes,"Noncanonical encoding");
        auto broken=bytes;broken[rng()%broken.size()]^=1;
        Check(Reject([&]{(void)DecodeCraftCheckpoint(broken,100);}),"Corruption accepted");
        Check(Reject([&]{(void)DecodeCraftCheckpoint(std::span(bytes).first(bytes.size()-1),100);}),"Truncation accepted");
        Check(Reject([&]{(void)DecodeCraftCheckpoint(bytes,99);}),"Limit ignored");
        auto invalid=state;invalid.Health=0;
        Check(Reject([&]{(void)EncodeCraftCheckpoint(payload,invalid,100);}),"Dead state accepted");
        restored=RestoreCraftCheckpoint(state,VoxelPlayerConfig{},flat);
        Check(restored&&restored->Feet==state.Feet&&!VoxelPlayerBlocked(*restored,VoxelPlayerConfig{},flat),"Signed position restore differs");
    }
    state.DayPhase=1;Check(Reject([&]{(void)EncodeCraftCheckpoint(payload,state,100);}),"Invalid phase accepted");
    state.DayPhase=.25;state.Feet[0]=std::numeric_limits<double>::quiet_NaN();
    Check(Reject([&]{(void)EncodeCraftCheckpoint(payload,state,100);}),"Nonfinite position accepted");
    std::cout<<"10000 checkpoint roundtrips and signed restores; 40000 rejection cases; blocked, airborne and unknown-floor fallback passed\n";
}
