#include "../CraftCombatHud.hpp"
#include <iostream>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    const auto empty=BuildCraftCombatHud(0);
    Check(empty.FillWidth==0&&empty.Crosshair==std::array{.9f,.9f,.9f},"No target state differs");
    std::mt19937 rng(719);
    for(unsigned i=0;i<100000;++i)
    {
        const unsigned health=rng()%13;
        const auto hud=BuildCraftCombatHud(health);
        Check(hud.FillWidth==8.f*health&&hud.FillWidth<=96,"Health fill differs");
        Check(hud.Crosshair==(health?std::array{1.f,.22f,.16f}:empty.Crosshair),"Crosshair state differs");
    }
    bool rejected=false;try{(void)BuildCraftCombatHud(13);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Invalid health accepted");
    std::cout<<"100000 combat HUD states, target removal and invalid health rejection passed\n";
}
