#include "../TerrainRenderOrigin.hpp"
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
int main()
{
    using namespace kairo::player::detail;
    std::mt19937_64 rng(12781);
    std::uniform_int_distribution<std::int64_t> cells(-2000000000LL,2000000000LL);
    std::uniform_int_distribution<int> offsets(0,255);
    for(unsigned i=0;i<100000;++i)
    {
        // Extent 32 m, dyadic offsets 0.125 m: all coordinates are exactly
        // representable in double, even at the signed-cell limits.
        const auto x=cells(rng),z=cells(rng);
        const double dx=offsets(rng)/8.0,dz=offsets(rng)/8.0;
        const double wx=static_cast<double>(x)*32+dx,wz=static_cast<double>(z)*32+dz;
        const auto origin=ChooseTerrainRenderOrigin(wx,wz,32);
        Check(origin[0]==static_cast<double>(x)*32&&origin[2]==static_cast<double>(z)*32,"Wrong signed cell origin");
        Check(TerrainRelativeFloat(wx,origin[0])==dx&&TerrainRelativeFloat(wz,origin[2])==dz,"Lost local position");
        Check(TerrainRelativeFloat(wx,origin[0]+32)==dx-32,"Rebase changed relative position");
    }
    const auto o=ChooseTerrainRenderOrigin(1e9+.125,-1e9-.125,32);
    Check(TerrainRelativeFloat(1e9+.125,o[0])-TerrainRelativeFloat(1e9,o[0])==.125f,"Submeter motion lost");
    Check(static_cast<float>(1e9+.125)==static_cast<float>(1e9),"Absolute-float comparison invalid");
    Check(ChooseTerrainRenderOrigin(-.125,-32,32)[0]==-32,"Negative coordinate rounded toward zero");
    bool rejected=false;
    try{(void)ChooseTerrainRenderOrigin(1e20,0,32);}
    catch(const std::out_of_range&){rejected=true;}
    Check(rejected,"Out-of-range world origin accepted");
    rejected=false;
    try{(void)TerrainRelativeFloat(1e100,0);}
    catch(const std::overflow_error&){rejected=true;}
    Check(rejected,"Overflowing GPU coordinate accepted");
    std::cout<<"100000 large-coordinate rebases, exact eighth-meter offsets and range checks passed\n";
}
