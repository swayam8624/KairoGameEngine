#include "../CraftSave.hpp"
#include <iostream>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
bool Reject(std::span<const std::uint8_t> bytes)
{
    try{(void)DecodeCraftSave(bytes);return false;}
    catch(const std::invalid_argument&){return true;}
}
void Rechecksum(std::vector<std::uint8_t>& bytes)
{
    bytes.resize(bytes.size()-8);
    CraftWrite(bytes,CraftSaveChecksum(bytes),8);
}
}
int main()
{
    constexpr std::uint64_t seed=0xfedcba9876543210ULL;
    CraftWorldEdits world(CraftBaseline{seed});
    std::mt19937 rng(451);
    for(unsigned i=0;i<10000;++i)
    {
        const VoxelCoordinate c{CraftFirst+int(rng()%48),int(rng()%16),CraftFirst+int(rng()%48)};
        Check(world.Set(c,static_cast<VoxelMaterial>(rng()%5)),"Edit failed");
    }
    const auto bytes=EncodeCraftSave(seed,world);
    const auto decoded=DecodeCraftSave(bytes);
    Check(decoded.Seed==seed&&decoded.World.Overrides()==world.Overrides()&&
        EncodeCraftSave(decoded.Seed,decoded.World)==bytes,"Noncanonical roundtrip");
    for(int z=CraftFirst;z<CraftLast;++z)for(int x=CraftFirst;x<CraftLast;++x)
        for(int y=0;y<16;++y)
            Check(decoded.World.Get({x,y,z})==world.Get({x,y,z}),"Restored occupancy differs");
    for(std::size_t n:{0u,1u,7u,23u,24u,31u})
        Check(Reject(std::span(bytes).first(n)),"Truncated header accepted");
    Check(Reject(std::span(bytes).first(bytes.size()-1)),"Truncated record accepted");
    for(unsigned i=0;i<100;++i)
    {
        auto bad=bytes;bad[rng()%bad.size()]^=1;
        Check(Reject(bad),"Corrupted save accepted");
    }
    auto invalid=bytes;invalid[20]=2;Rechecksum(invalid);
    Check(Reject(invalid),"Unknown version accepted");
    invalid=bytes;invalid[24+12]=255;invalid[24+13]=255;Rechecksum(invalid);
    Check(Reject(invalid),"Invalid material accepted despite valid checksum");
    invalid=bytes;
    std::copy_n(invalid.begin()+24,14,invalid.begin()+38);Rechecksum(invalid);
    Check(Reject(invalid),"Duplicate record accepted despite valid checksum");
    CraftWorldEdits empty(CraftBaseline{seed});
    const auto clean=EncodeCraftSave(seed,empty);
    Check(clean.size()==32&&DecodeCraftSave(clean).World.Overrides().empty(),"Empty save invalid");
    std::cout<<"10000 randomized edits, 36864 restored voxels, 100 corruptions, canonical roundtrip and malformed records passed; "
        <<world.Overrides().size()<<" overrides, "<<bytes.size()<<" bytes\n";
}
