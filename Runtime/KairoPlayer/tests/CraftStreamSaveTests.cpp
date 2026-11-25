#include "../CraftStreamSave.hpp"
#include <iostream>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
bool Reject(std::span<const std::uint8_t> bytes)
{
    try{(void)DecodeStreamCraftSave(bytes);return false;}
    catch(const std::invalid_argument&){return true;}
}
}
int main()
{
    constexpr std::uint64_t seed=0x123456789abcdef0ULL;
    StreamCraftWorldEdits world(StreamingCraftBaseline{seed});
    std::mt19937 rng(345);
    for(unsigned i=0;i<10000;++i)
    {
        const VoxelCoordinate c{int(rng()%2000000001u)-1000000000,int(rng()%16),int(rng()%2000000001u)-1000000000};
        Check(world.Set(c,static_cast<VoxelMaterial>(rng()%5)),"Stream edit failed");
    }
    Check(world.Set({-1000000000,15,1000000000},CraftPlaced),"Far edit failed");
    auto bytes=EncodeStreamCraftSave(seed,world);
    auto restored=DecodeStreamCraftSave(bytes);
    Check(restored.Seed==seed&&restored.World.Overrides()==world.Overrides()&&
        EncodeStreamCraftSave(seed,restored.World)==bytes,"Stream roundtrip differs");
    Check(restored.World.Get({-1000000000,15,1000000000})==CraftPlaced,"Far edit lost");
    for(unsigned i=0;i<100;++i)
    {
        auto bad=bytes;bad[rng()%bad.size()]^=1;
        Check(Reject(bad),"Corrupt stream save accepted");
    }
    CraftWorldEdits finite(CraftBaseline{seed});
    Check(Reject(EncodeCraftSave(seed,finite)),"Finite v1 accepted as stream v2");
    bool rejected=false;
    try{(void)DecodeCraftSave(bytes);}
    catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Stream v2 accepted as finite v1");
    Check(world.Set({std::numeric_limits<std::int32_t>::max(),15,0},CraftPlaced),"Invalid fixture");
    rejected=false;
    try{(void)EncodeStreamCraftSave(seed,world);}
    catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Invalid halo-domain save accepted");
    std::cout<<"10000 streamed edits, signed billion-meter persistence, 100 corruptions and v1/v2 separation passed\n";
}
