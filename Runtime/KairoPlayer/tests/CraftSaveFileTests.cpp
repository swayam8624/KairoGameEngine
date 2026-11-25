#include "../CraftSaveFile.hpp"
#include "../CraftEquipmentSave.hpp"
#include "../CraftCaveSession.hpp"
#include "../CraftCheckpoint.hpp"
#include "../CraftCaveSpawn.hpp"
#include "../CraftSurfaceSpawn.hpp"
#include <chrono>
#include <iostream>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F> bool Reject(F&& fn){try{fn();return false;}catch(const std::exception&){return true;}}
struct Scratch
{
    std::filesystem::path Path;
    Scratch()
    {
        const auto base=std::filesystem::temp_directory_path();
        std::mt19937_64 rng(static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
        for(unsigned i=0;i<64;++i)
        {
            Path=base/("kairo-save-test-"+std::to_string(rng()));
            if(std::filesystem::create_directory(Path))return;
        }
        throw std::runtime_error("Could not reserve test directory.");
    }
    ~Scratch(){std::error_code ignored;std::filesystem::remove_all(Path,ignored);}
};
}
int main()
{
    Scratch scratch;
    const auto path=scratch.Path/"world.kcraft";
    auto lock=path;lock+=".write-lock";
    constexpr std::size_t limit=4096;
    std::mt19937 rng(661);
    std::vector<std::uint8_t> previous;
    for(unsigned i=0;i<256;++i)
    {
        std::vector<std::uint8_t> bytes(1+rng()%limit);
        for(auto& b:bytes)b=static_cast<std::uint8_t>(rng());
        WriteCraftSaveFile(path,bytes,limit);
        Check(ReadCraftSaveFile(path,limit)==bytes&&!std::filesystem::exists(lock),"Disk replacement differs");
        previous=std::move(bytes);
    }
    Check(Reject([&]{WriteCraftSaveFile(path,previous,previous.size()-1);}),"Oversize write accepted");
    Check(Reject([&]{(void)ReadCraftSaveFile(path,previous.size()-1);}),"Oversize read accepted");
    Check(Reject([&]{WriteCraftSaveFile(path,{},limit);}),"Empty write accepted");
    Check(std::filesystem::create_directory(lock),"Could not create competing reservation");
    {std::ofstream marker(lock/"payload");marker<<"other writer";}
    Check(Reject([&]{WriteCraftSaveFile(path,previous,limit);}),"Competing reservation ignored");
    Check(std::filesystem::exists(lock/"payload")&&ReadCraftSaveFile(path,limit)==previous,"Competing writer or prior save damaged");
    std::filesystem::remove_all(lock);
    const auto directory=scratch.Path/"directory";
    std::filesystem::create_directory(directory);
    {std::ofstream marker(directory/"keep");marker<<"preserve";}
    Check(Reject([&]{WriteCraftSaveFile(directory,previous,limit);}),"Directory replaced by save");
    auto failedLock=directory;failedLock+=".write-lock";
    Check(std::filesystem::exists(directory/"keep")&&!std::filesystem::exists(failedLock),"Failed replacement leaked reservation or damaged target");
    Check(Reject([&]{(void)ReadCraftSaveFile(directory,limit);}),"Directory read accepted");
    Check(Reject([&]{(void)ReadCraftSaveFile(scratch.Path/"missing",limit);}),"Missing save accepted");
    Check(ReadCraftSaveFile(path,limit)==previous,"Failed writes damaged previous save");
    // Exercise the real v5 envelope and both underlying world codecs on disk.
    for(std::uint64_t seed=0;seed<128;++seed)
    {
        CraftInventory inventory;inventory.Counts={1,2,3,4};
        CraftEquipment pick{static_cast<std::uint32_t>(1+seed%64),seed%2==0};
        StreamCraftWorldEdits surface(StreamingCraftBaseline{seed});
        Check(surface.Set({-1000000000,15,1000000000},CraftPlaced),"Surface edit failed");
        const auto surfaceBytes=EncodeCraftEquipmentEnvelope(EncodeCraftSession(seed,surface,inventory),pick,CraftSessionMaximumBytes);
        WriteCraftSaveFile(path,surfaceBytes,CraftSessionMaximumBytes+CraftEquipmentOverhead);
        const auto readSurface=ReadCraftSaveFile(path,CraftSessionMaximumBytes+CraftEquipmentOverhead);
        const auto surfaceOuter=DecodeCraftEquipmentEnvelope(readSurface,CraftSessionMaximumBytes);
        const auto restoredSurface=DecodeCraftSession(surfaceOuter.Payload);
        Check(restoredSurface.Seed==seed&&restoredSurface.World.Overrides()==surface.Overrides()&&
            restoredSurface.Inventory.Counts==inventory.Counts&&surfaceOuter.Equipment.Durability==pick.Durability&&
            surfaceOuter.Equipment.Equipped==pick.Equipped,"Real surface disk session differs");
        CaveCraftWorldEdits cave(CaveCraftBaseline{seed});
        Check(cave.Set({16,-64,0},VoxelAir),"Cave edit failed");
        const auto caveBytes=EncodeCraftEquipmentEnvelope(EncodeCaveCraftSession(seed,cave,inventory),pick,CaveCraftMaximumBytes);
        WriteCraftSaveFile(path,caveBytes,CaveCraftMaximumBytes+CraftEquipmentOverhead);
        const auto readCave=ReadCraftSaveFile(path,CaveCraftMaximumBytes+CraftEquipmentOverhead);
        const auto caveOuter=DecodeCraftEquipmentEnvelope(readCave,CaveCraftMaximumBytes);
        const auto restoredCave=DecodeCaveCraftSession(caveOuter.Payload);
        Check(restoredCave.Seed==seed&&restoredCave.World.Overrides()==cave.Overrides()&&
            restoredCave.Inventory.Counts==inventory.Counts&&caveOuter.Equipment.Durability==pick.Durability&&
            caveOuter.Equipment.Equipped==pick.Equipped,"Real cave disk session differs");
        Check(Reject([&]{(void)DecodeCraftSession(caveOuter.Payload);}),"Cave accepted as surface");
        // Preserve all actual nested world and equipment bytes while adding
        // the v6 player metadata. The same decoder sequence is used by the demos.
        const auto surfaceQuery=[&](VoxelCoordinate c)->std::optional<VoxelMaterial>{return surface.Get(c);};
        const auto surfaceSpawn=FindCraftSurfaceSpawn(VoxelPlayerConfig{},CraftExtent,surfaceQuery);
        Check(surfaceSpawn.has_value(),"Surface checkpoint spawn missing");
        const CraftCheckpoint surfaceState{surfaceSpawn->Feet,.125,-.5,.75,7,4};
        const auto surfaceV6=EncodeCraftCheckpoint(surfaceBytes,surfaceState,CraftSessionMaximumBytes+CraftEquipmentOverhead);
        WriteCraftSaveFile(path,surfaceV6,CraftSessionMaximumBytes+CraftEquipmentOverhead+CraftCheckpointOverhead);
        const auto surfaceReadV6=ReadCraftSaveFile(path,CraftSessionMaximumBytes+CraftEquipmentOverhead+CraftCheckpointOverhead);
        const auto surfaceCheckpoint=DecodeCraftCheckpoint(surfaceReadV6,CraftSessionMaximumBytes+CraftEquipmentOverhead);
        const auto surfacePick=DecodeCraftEquipmentEnvelope(surfaceCheckpoint.Payload,CraftSessionMaximumBytes);
        const auto surfaceWorld=DecodeCraftSession(surfacePick.Payload);
        Check(surfaceCheckpoint.State==surfaceState&&surfaceWorld.Seed==seed&&
            surfaceWorld.World.Overrides()==surface.Overrides()&&surfaceWorld.Inventory.Counts==inventory.Counts&&
            surfacePick.Equipment.Durability==pick.Durability&&surfacePick.Equipment.Equipped==pick.Equipped,
            "Real surface checkpoint differs");
        const auto surfaceRestored=RestoreCraftCheckpoint(surfaceCheckpoint.State,VoxelPlayerConfig{},
            [&](VoxelCoordinate c)->std::optional<VoxelMaterial>{return surfaceWorld.World.Get(c);});
        Check(surfaceRestored&&surfaceRestored->Feet==surfaceState.Feet,"Surface checkpoint pose did not restore");
        const auto caveQuery=[&](VoxelCoordinate c)->std::optional<VoxelMaterial>{
            if(!InCaveWorld(c))return {};
            return cave.Get(c);
        };
        const auto caveSpawn=FindCaveSpawn(VoxelPlayerConfig{},caveQuery);
        Check(caveSpawn.has_value(),"Cave checkpoint spawn missing");
        const CraftCheckpoint caveState{caveSpawn->Feet,-.25,.5,.25,11,2};
        const auto caveV6=EncodeCraftCheckpoint(caveBytes,caveState,CaveCraftMaximumBytes+CraftEquipmentOverhead);
        WriteCraftSaveFile(path,caveV6,CaveCraftMaximumBytes+CraftEquipmentOverhead+CraftCheckpointOverhead);
        const auto caveReadV6=ReadCraftSaveFile(path,CaveCraftMaximumBytes+CraftEquipmentOverhead+CraftCheckpointOverhead);
        const auto caveCheckpoint=DecodeCraftCheckpoint(caveReadV6,CaveCraftMaximumBytes+CraftEquipmentOverhead);
        const auto cavePick=DecodeCraftEquipmentEnvelope(caveCheckpoint.Payload,CaveCraftMaximumBytes);
        const auto caveWorld=DecodeCaveCraftSession(cavePick.Payload);
        Check(caveCheckpoint.State==caveState&&caveWorld.Seed==seed&&
            caveWorld.World.Overrides()==cave.Overrides()&&caveWorld.Inventory.Counts==inventory.Counts&&
            cavePick.Equipment.Durability==pick.Durability&&cavePick.Equipment.Equipped==pick.Equipped,
            "Real cave checkpoint differs");
        const auto caveRestored=RestoreCraftCheckpoint(caveCheckpoint.State,VoxelPlayerConfig{},
            [&](VoxelCoordinate c)->std::optional<VoxelMaterial>{
                if(!InCaveWorld(c))return {};
                return caveWorld.World.Get(c);
            });
        Check(caveRestored&&caveRestored->Feet==caveState.Feet,"Cave checkpoint pose did not restore");
        Check(Reject([&]{(void)DecodeCraftSession(cavePick.Payload);}),"v6 cave accepted as surface");
    }
    std::cout<<"256 v5 sessions and 256 v6 real surface/cave checkpoints persisted and decoded from disk\n";
    std::cout<<"256 actual file replacements, bounds, competing reservation, failure cleanup and prior save preservation passed\n";
}
