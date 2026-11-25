#include "../CraftMeshBatch.hpp"
#include <chrono>
#include <iostream>
#include <thread>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    using Plan=CraftWindowPlan<2>;
    using World=VoxelEdits<StreamingCraftBaseline>;
    auto world=std::make_shared<World>(StreamingCraftBaseline{42});
    CraftMeshBatch<2> batch(4,8);
    auto plan=PlanCraftWindow<2>({}, {0,0});
    batch.Begin(plan,world);
    (void)batch.Poll();
    Check(batch.Occupied()<=8,"Queue capacity exceeded");
    // Cancel with outstanding work, then replace the immutable snapshot.
    auto edited=std::make_shared<World>(*world);
    Check(edited->Set({0,15,0},CraftPlaced),"Edit fixture failed");
    batch.Begin(plan,edited);
    const auto settle=[&]()
    {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(!batch.Poll())
        {
            Check(batch.Occupied()<=8,"Queue capacity exceeded");
            Check(std::chrono::steady_clock::now()<deadline,"Batch timed out");
            std::this_thread::yield();
        }
    };
    std::size_t meshes=0,vertices=0;
    const auto verify=[&](const Plan& p,const World& source)
    {
        settle();
        for(std::size_t i=0;i<Plan::Capacity;++i)if(p.Dirty[i])
        {
            const auto actual=batch.Take(i);
            const auto expected=BuildCraftMesh(BuildVoxelChunkSurface(StreamingCraftOrigin(*p.Next[i]),
                CraftExtent,[&](VoxelCoordinate c){return source.Get(c);}));
            Check(actual.Indices==expected.Indices&&actual.Vertices.size()==expected.Vertices.size(),"Mesh topology mismatch");
            for(std::size_t j=0;j<actual.Vertices.size();++j)
            {
                const auto& a=actual.Vertices[j];const auto& b=expected.Vertices[j];
                Check(a.Position==b.Position&&a.Normal==b.Normal&&a.Color==b.Color&&a.UV==b.UV,"Stale vertex or material");
            }
            ++meshes;vertices+=actual.Vertices.size();
        }
        batch.Cancel();
    };
    verify(plan,*edited);
    Check(batch.StaleCount()>0,"Cancelled generation not accounted");
    for(int i=1;i<=40;++i)
    {
        plan=PlanCraftWindow<2>(plan.Next,{i,i%3});
        batch.Begin(plan,edited);
        verify(plan,*edited);
    }
    std::cout<<"Craft async: "<<meshes<<" meshes, "<<vertices<<" vertices matched synchronous oracle; "
        <<batch.StaleCount()<<" stale jobs discarded; queue bound 8 passed\n";
}
