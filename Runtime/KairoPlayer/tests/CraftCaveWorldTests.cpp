#include "../CraftCaveWorld.hpp"
#include "../CraftMeshBatch.hpp"
#include <iostream>
#include <set>
#include <random>
#include <chrono>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    using P=CaveWindowPlan<1>;
    const CaveCraftBaseline baseline{42};
    // Galleries and shafts cross all signed chunk boundaries.
    for(int x=-128;x<=128;++x)for(int y:{-56,-40,-24,-8})
        Check(baseline({x,y,8})==VoxelAir,"Broken horizontal gallery");
    for(int y=-63;y<0;++y)Check(baseline({-8,y,8})==VoxelAir,"Broken shaft");
    Check(baseline({8,-64,8})==CraftStone,"Missing bedrock");
    auto p=PlanCaveWindow<1>({}, {0,0,0});
    Check(p.Loaded==27,"Initial cave window");
    auto down=PlanCaveWindow<1>(p.Next,{0,-1,0});
    Check(down.Loaded==9&&down.Evicted==9&&down.Retained==18,"Vertical overlap");
    auto dirty=CaveDirty(down.Next,{0,-16,0});
    Check(std::count(dirty.begin(),dirty.end(),true)==4,"XYZ corner face invalidation");
    std::mt19937 rng(411);
    std::uint64_t retained=0,loaded=0;
    for(unsigned i=0;i<10000;++i)
    {
        const CaveChunk center{int(rng()%17)-8,-int(rng()%4),int(rng()%17)-8};
        const auto next=PlanCaveWindow<1>(p.Next,center);
        std::set<CaveChunk> found;
        for(std::size_t j=0;j<P::Capacity;++j)
        {
            Check(next.Next[j].has_value()&&found.insert(*next.Next[j]).second,"Duplicate or missing slot");
            Check(next.Dirty[j]==(p.Next[j]!=next.Next[j]),"Dirty slot differs");
            const auto c=*next.Next[j];
            Check(std::abs(c.X-center.X)<=1&&std::abs(c.Y-center.Y)<=1&&std::abs(c.Z-center.Z)<=1,"Incorrect window");
        }
        retained+=next.Retained;loaded+=next.Loaded;p=next;
    }
    using W=VoxelEdits<CaveCraftBaseline>;
    auto w=std::make_shared<W>(baseline);
    const auto far=PlanCaveWindow<1>({},CaveObserver(1e9,-24,-1e9));
    CraftMeshBatch<1,P,CaveCraftBaseline,CaveChunk> batch(4);
    batch.Begin(far,w);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(!batch.Poll())
    {
        Check(batch.Occupied()<=8&&std::chrono::steady_clock::now()<deadline,"Cave workers timed out");
        std::this_thread::yield();
    }
    std::size_t triangles=0;
    for(std::size_t i=0;i<P::Capacity;++i)
    {
        const auto mesh=batch.Take(i);
        const auto oracle=BuildCraftMesh(BuildVoxelChunkSurface(StreamingCraftOrigin(*far.Next[i]),16,baseline));
        Check(mesh.Indices==oracle.Indices&&mesh.Vertices.size()==oracle.Vertices.size(),"Cave worker geometry differs");
        for(std::size_t j=0;j<mesh.Vertices.size();++j)
            Check(mesh.Vertices[j].Position==oracle.Vertices[j].Position&&mesh.Vertices[j].Normal==oracle.Vertices[j].Normal&&
                mesh.Vertices[j].Color==oracle.Vertices[j].Color,"Cave vertex differs");
        triangles+=mesh.Indices.size()/3;
    }
    Check(triangles>0,"Empty underground world");
    std::cout<<"10000 XYZ transitions, "<<retained<<" retained, "<<loaded<<" loaded; connected galleries, vertical invalidation and "<<triangles<<" worker triangles passed\n";
}
