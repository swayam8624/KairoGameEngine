#include "../TerrainErrorProfile.hpp"
#include <iostream>
#include <random>
#include <algorithm>
namespace {
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
int main()
{
    using namespace kairo::player::detail;
    std::size_t calls=0;
    const auto nonmonotone=BuildTerrainErrorProfile(8,[&](unsigned stride){
        ++calls;return stride==2?1000ull:stride==4?200ull:3000ull;
    });
    Check(calls==3&&nonmonotone.Choose(8,250)==4&&nonmonotone.Choose(2,250)==1,
        "Nonmonotone error selection incorrect");
    std::mt19937_64 rng(7461);
    for(unsigned cell=0;cell<10000;++cell)
    {
        const auto profile=BuildTerrainErrorProfile(1024,[&](unsigned){++calls;return rng()%10000;});
        for(unsigned requested=1;requested<=1024;requested*=2)
        {
            const auto budget=rng()%10000;
            unsigned expected=1;
            for(unsigned stride=1;stride<=requested;stride*=2)
                if(profile.Error(stride)<=budget)expected=stride;
            Check(profile.Choose(requested,budget)==expected,"Profile differs from exhaustive oracle");
        }
    }
    Check(calls==100003,"Selection repeated expensive measurement");
    bool rejected=false;
    try{(void)nonmonotone.Choose(16,100);}
    catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Unmeasured stride accepted");
    std::cout<<"10000 error profiles, 110000 cached selections and nonmonotone oracle passed\n";
}
