#include "../CraftInventory.hpp"
#include <iostream>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    constexpr std::uint64_t seed=42;
    StreamCraftWorldEdits world(StreamingCraftBaseline{seed});
    CraftInventory inventory;
    const VoxelCoordinate block{0,0,0},air{0,15,0};
    const auto material=world.Get(block);
    const auto query=[&](VoxelCoordinate cell)->std::optional<VoxelMaterial>{return world.Get(cell);};
    const auto apply=[&](VoxelCoordinate c,VoxelMaterial m){return world.Set(c,m);};
    const auto reject=[](VoxelCoordinate,VoxelMaterial){return false;};
    const auto allow=[](VoxelCoordinate){return true;};
    const auto deny=[](VoxelCoordinate){return false;};
    Check(!MineCraftBlock(inventory,block,query,reject)&&inventory.Count(material)==0,"Failed mine credited inventory");
    inventory.Selected=material;
    Check(!PlaceCraftBlock(inventory,air,query,apply,allow),"Empty stack placed a block");
    Check(MineCraftBlock(inventory,block,query,apply)&&inventory.Count(material)==1,"Mine credit failed");
    Check(!PlaceCraftBlock(inventory,air,query,reject,allow)&&inventory.Count(material)==1,"Failed upload consumed item");
    Check(!PlaceCraftBlock(inventory,air,query,apply,deny)&&inventory.Count(material)==1,"Blocked placement consumed item");
    Check(!PlaceCraftBlock(inventory,{1,0,0},query,apply,allow),"Occupied cell replaced");
    const auto bytes=EncodeCraftSession(seed,world,inventory);
    auto decoded=DecodeCraftSession(bytes);
    Check(decoded.Seed==seed&&decoded.Inventory.Count(material)==1&&decoded.Inventory.Selected==material&&
        decoded.World.Overrides()==world.Overrides(),"Inventory session roundtrip failed");
    Check(EncodeCraftSession(decoded.Seed,decoded.World,decoded.Inventory)==bytes,"Session encoding not canonical");
    auto corrupted=bytes;corrupted[20]^=1;
    bool rejected=false;
    try{(void)DecodeCraftSession(corrupted);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Corrupted session accepted");
    corrupted=bytes;corrupted[16]=255;corrupted[17]=255;
    corrupted.resize(corrupted.size()-8);CraftWrite(corrupted,CraftSaveChecksum(corrupted),8);
    rejected=false;
    try{(void)DecodeCraftSession(corrupted);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Invalid stack accepted with valid checksum");
    Check(PlaceCraftBlock(inventory,block,query,apply,allow)&&world.Overrides().empty()&&inventory.Count(material)==0,
        "Restoring original block did not consume inventory");
    for(unsigned i=0;i<10000;++i)
    {
        Check(MineCraftBlock(inventory,block,query,apply),"Repeated mine failed");
        Check(PlaceCraftBlock(inventory,block,query,apply,allow),"Repeated placement failed");
        Check(inventory.Count(material)==0&&world.Overrides().empty(),"Block or item duplication");
    }
    inventory.Counts[material-1]=CraftInventory::StackLimit;
    Check(!MineCraftBlock(inventory,block,query,apply)&&world.Get(block)==material,"Stack cap failed");
    const auto unknown=[](VoxelCoordinate)->std::optional<VoxelMaterial>{return {};};
    Check(!MineCraftBlock(inventory,air,unknown,apply),"Unknown cell mined");
    std::cout<<"10000 mine/place conservation cycles, failed edit rollback, stack caps, body guard and session persistence passed\n";
}
