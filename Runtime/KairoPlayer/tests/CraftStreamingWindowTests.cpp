#include "../CraftStreamingWindow.hpp"
#include <iostream>
#include <random>
#include <set>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    using P=CraftWindowPlan<2>;
    P::Slots slots{};
    auto first=PlanCraftWindow<2>(slots,{0,0});
    Check(first.Loaded==25&&first.Retained==0&&first.Evicted==0,"Initial window");
    auto moved=PlanCraftWindow<2>(first.Next,{1,0});
    Check(moved.Loaded==5&&moved.Retained==20&&moved.Evicted==5,"One column shift");
    auto unchanged=PlanCraftWindow<2>(moved.Next,{1,0});
    Check(unchanged.Next==moved.Next&&unchanged.Loaded==0,"Stationary plan");
    Check(CraftObserverColumn(-.01,-16)==CraftColumn{-1,-1}&&
        CraftObserverColumn(-16.01,16)==CraftColumn{-2,1},"Signed observer mapping");
    const auto dirty=StreamingCraftDirty(first.Next,{0,4,0});
    Check(std::count(dirty.begin(),dirty.end(),true)==3,"Boundary edit affected wrong count");
    for(std::size_t i=0;i<dirty.size();++i)
        if(dirty[i])Check(first.Next[i]==CraftColumn{0,0}||first.Next[i]==CraftColumn{-1,0}||
            first.Next[i]==CraftColumn{0,-1},"Diagonal invalidation");
    std::mt19937 rng(241);
    CraftColumn center{0,0};
    std::uint64_t retained=0,loaded=0;
    for(unsigned step=0;step<10000;++step)
    {
        if(step%17==0)center={int(rng()%100000)-50000,int(rng()%100000)-50000};
        else {center.X+=int(rng()%3)-1;center.Z+=int(rng()%3)-1;}
        const auto plan=PlanCraftWindow<2>(slots,center);
        std::set<CraftColumn> expected,actual;
        for(int z=-2;z<=2;++z)for(int x=-2;x<=2;++x)expected.insert({center.X+x,center.Z+z});
        for(std::size_t i=0;i<slots.size();++i)
        {
            Check(plan.Next[i].has_value(),"Vacant next slot");
            Check(actual.insert(*plan.Next[i]).second,"Duplicate next slot");
            Check(plan.Dirty[i]==(slots[i]!=plan.Next[i]),"Dirty slot mismatch");
            if(slots[i]&&expected.contains(*slots[i]))Check(plan.Next[i]==slots[i],"Overlap moved slots");
        }
        Check(actual==expected&&plan.Retained+plan.Loaded==25,"Window coverage");
        Check(plan.Evicted==static_cast<std::size_t>(std::count_if(slots.begin(),slots.end(),
            [&](auto s){return s&&!expected.contains(*s);})),"Eviction accounting");
        slots=plan.Next;retained+=plan.Retained;loaded+=plan.Loaded;
    }
    const StreamingCraftBaseline baseline{42};
    for(int z=CraftFirst;z<CraftLast;++z)for(int x=CraftFirst;x<CraftLast;++x)
        for(int y=0;y<CraftExtent;++y)
            Check(baseline({x,y,z})==CraftBaseline{42}({x,y,z}),"Finite terrain changed");
    const VoxelCoordinate far{-1000000000,4,1000000000};
    const auto owner=CraftOwner(far);
    const auto origin=StreamingCraftOrigin(owner);
    Check(far.X>=origin.X&&far.X<origin.X+16&&far.Z>=origin.Z&&far.Z<origin.Z+16,"Large chunk origin");
    const auto mesh=BuildCraftMesh(BuildVoxelChunkSurface(origin,16,baseline));
    Check(!mesh.Indices.empty(),"Large-world chunk empty");
    bool rejected=false;
    try{(void)PlanCraftWindow<2>({}, {std::numeric_limits<std::int32_t>::max(),0});}
    catch(const std::out_of_range&){rejected=true;}
    Check(rejected,"Invalid window accepted");
    std::cout<<"10000 window transitions, "<<retained<<" retained slots, "<<loaded
        <<" loads; signed mapping, halo invalidation and billion-meter chunk mesh passed\n";
}
