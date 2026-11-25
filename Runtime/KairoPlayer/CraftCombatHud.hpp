#pragma once
#include <array>
#include <stdexcept>

namespace kairo::player::detail
{
    struct CraftCombatHud
    {
        std::array<float,3> Crosshair{.9f,.9f,.9f};
        float FillWidth=0;
    };
    // Zero means no target. Only a visible, in-range enemy is shown.
    // The bar is a fixed 96 logical pixels at full health, and the
    // crosshair turns red independently of the current mining progress.
    inline CraftCombatHud BuildCraftCombatHud(unsigned health)
    {
        if(health>12)throw std::invalid_argument("Invalid sentinel HUD health.");
        if(!health)return {};
        return {{1.f,.22f,.16f},8.f*static_cast<float>(health)};
    }
}
