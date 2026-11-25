#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace kairo::player::detail
{
    // Only these values affect the native pixel HUD. Camera motion and world
    // mesh publication are deliberately absent from the cache key.
    struct CraftHudKey
    {
        bool Enabled=false;
        std::array<std::uint32_t,4> Counts{};
        std::uint32_t Selected=0,Width=0,Height=0,Health=20,MiningProgress=0,ToolDurability=0;
        bool ToolEquipped=false;
        bool operator==(const CraftHudKey&) const=default;
    };
    class CraftHudPublication
    {
        std::optional<CraftHudKey> m_Published;
    public:
        template<class Submit>
        bool Refresh(CraftHudKey key,Submit&& submit)
        {
            // Disabled HUDs have no visible inventory or extent dependence.
            if(!key.Enabled)key={};
            if(m_Published==key)return false;
            submit(); // commit the cache only after the renderer accepts it
            m_Published=key;
            return true;
        }
    };
}
