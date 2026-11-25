#include "../CraftCaveSession.hpp"
#include "../CraftCaveSpawn.hpp"
#include "../CraftRecipesHud.hpp"
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
    // Spawn must remain valid after arbitrary saved edits and seed changes.
    const VoxelPlayerConfig config;
    const auto checkSpawn=[&](auto&& read)
    {
        const auto spawn=FindCaveSpawn(config,read);
        Check(spawn.has_value(),"No cave spawn");
        Check(!VoxelPlayerBlocked(*spawn,config,read)&&spawn->Grounded,"Blocked cave spawn");
        auto below=*spawn;below.Feet[1]-=.000001;
        Check(VoxelPlayerBlocked(below,config,read),"Unsupported cave spawn");
        return *spawn;
    };
    for(std::uint64_t seed=0;seed<1024;++seed)
    {
        const auto read=[&](VoxelCoordinate c)->std::optional<VoxelMaterial>{return CaveCraftBaseline{seed}(c);};
        const auto first=checkSpawn(read),second=checkSpawn(read);
        Check(first.Feet==second.Feet,"Nondeterministic spawn");
    }
    const auto shaft=[](VoxelCoordinate c)->std::optional<VoxelMaterial>{
        return (c.X==8&&c.Z==8)||c.Y>=4?VoxelAir:CraftStone;
    };
    const auto safe=checkSpawn(shaft);
    Check(safe.Feet[0]!=8.5||safe.Feet[2]!=8.5,"Spawn selected unsupported shaft");
    Check(!FindCaveSpawn(config,[](VoxelCoordinate)->std::optional<VoxelMaterial>{return VoxelAir;}),"Empty world spawn");
    Check(!FindCaveSpawn(config,[](VoxelCoordinate)->std::optional<VoxelMaterial>{return CraftStone;}),"Solid world spawn");
    Check(!FindCaveSpawn(config,[](VoxelCoordinate)->std::optional<VoxelMaterial>{return {};}),"Unavailable world spawn");
    CaveCraftWorldEdits blockedColumn(baseline,CraftSaveMaximumEdits);
    const auto original=checkSpawn([&](VoxelCoordinate c)->std::optional<VoxelMaterial>{return blockedColumn.Get(c);});
    const int sx=static_cast<int>(std::floor(original.Feet[0])),sz=static_cast<int>(std::floor(original.Feet[2]));
    for(int y=-1;y<CaveTop;++y)
        if(blockedColumn.Get({sx,y,sz})!=CraftStone)
            Check(blockedColumn.Set({sx,y,sz},CraftStone),"Could not block previous spawn");
    const auto reloaded=DecodeCaveCraftSession(EncodeCaveCraftSession(42,blockedColumn,CraftInventory{}));
    const auto relocated=checkSpawn([&](VoxelCoordinate c)->std::optional<VoxelMaterial>{return reloaded.World.Get(c);});
    Check(relocated.Feet[0]!=original.Feet[0]||relocated.Feet[2]!=original.Feet[2],"Saved solid column did not relocate spawn");
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
    // The cave codec preserves edits at negative heights and large signed
    // coordinates, without importing incompatible flat-world generation.
    CaveCraftWorldEdits saved(baseline,CraftSaveMaximumEdits);
    CraftInventory inventory;
    const auto read=[&](VoxelCoordinate v)->std::optional<VoxelMaterial>{return saved.Get(v);};
    const auto write=[&](VoxelCoordinate v,VoxelMaterial m){return InCaveWorld(v)&&saved.Set(v,m);};
    for(int i=0;i<10000;++i)
    {
        const VoxelCoordinate cell{16*i,-16,0};
        const auto old=saved.Get(cell);
        Check(old==CraftStone,"Unexpected cave fixture");
        inventory.Selected=old;
        Check(MineCraftBlock(inventory,cell,read,write),"Cave mining failed");
        Check(inventory.Count(old)==1&&saved.Get(cell)==VoxelAir,"Cave mine conservation");
        Check(!PlaceCraftBlock(inventory,cell,read,write,[](auto){return false;}),"Body rejection failed");
        Check(inventory.Count(old)==1&&saved.Get(cell)==VoxelAir,"Rejected placement changed state");
        Check(PlaceCraftBlock(inventory,cell,read,write,[](auto){return true;}),"Cave placement failed");
        Check(inventory.Count(old)==0&&saved.Overrides().empty(),"Cave restore conservation");
    }
    for(int i=0;i<10000;++i)
    {
        const VoxelCoordinate cell{1000000000+16*i,-16,-1000000000};
        Check(saved.Set(cell,VoxelAir),"Cave far edit failed");
    }
    inventory.Counts={0,1,2,0};
    Check(CraftApplyRecipe(inventory,CraftRecipes[0]),"Cave recipe failed");
    const auto bytes=EncodeCaveCraftSession(42,saved,inventory);
    const auto decoded=DecodeCaveCraftSession(bytes);
    Check(decoded.Seed==42&&decoded.World.Overrides()==saved.Overrides()&&
        decoded.Inventory.Counts==inventory.Counts&&decoded.Inventory.Selected==inventory.Selected,
        "Cave session mismatch");
    Check(EncodeCaveCraftSession(decoded.Seed,decoded.World,decoded.Inventory)==bytes,"Noncanonical cave session");
    const auto rejects=[](const std::vector<std::uint8_t>& data)
    {
        try{(void)DecodeCaveCraftSession(data);return false;}
        catch(const std::invalid_argument&){return true;}
    };
    for(unsigned i=0;i<100;++i)
    {
        auto corrupt=bytes;corrupt[rng()%corrupt.size()]^=1;
        Check(rejects(corrupt),"Cave corruption accepted");
    }
    auto truncated=bytes;truncated.pop_back();
    Check(rejects(truncated),"Truncated cave session accepted");
    StreamCraftWorldEdits flat(StreamingCraftBaseline{42});
    Check(rejects(EncodeCraftSession(42,flat,inventory)),"Flat session accepted as cave");
    auto malformed=bytes;
    // Invalid selected slot with valid checksum must still be rejected.
    malformed[40]=0;malformed[41]=0;
    malformed.resize(malformed.size()-8);
    CraftWrite(malformed,CraftSaveChecksum(malformed),8);
    Check(rejects(malformed),"Malformed inventory accepted");
    std::cout<<"10000 cave inventory cycles, 10000 far edits and 100 corrupt saves passed\n";
    std::cout<<"10000 XYZ transitions, "<<retained<<" retained, "<<loaded<<" loaded; connected galleries, vertical invalidation and "<<triangles<<" worker triangles passed\n";
}
