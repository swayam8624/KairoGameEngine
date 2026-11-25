#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace kairo::player::detail
{
    // One craftable stone pick. Material IDs 3/4 are stone/building blocks.
    // The inventory transaction spends two dirt and three stone only when
    // no pick exists. A successful eligible mine consumes exactly one use.
    struct CraftEquipment
    {
        static constexpr std::uint32_t MaximumDurability=64;
        std::uint32_t Durability=0;
        bool Equipped=false;
        [[nodiscard]] bool Valid() const noexcept
        {return Durability<=MaximumDurability&&(!Equipped||Durability!=0);}
        bool Craft(std::array<std::uint32_t,4>& counts)
        {
            if(!Valid()||std::any_of(counts.begin(),counts.end(),[](auto n){return n>999;}))
                throw std::invalid_argument("Invalid equipment or inventory.");
            if(Durability||counts[1]<2||counts[2]<3)return false;
            counts[1]-=2;counts[2]-=3;Durability=MaximumDurability;Equipped=true;
            return true;
        }
        bool Toggle() noexcept
        {
            if(!Durability)return false;
            Equipped=!Equipped;return true;
        }
        [[nodiscard]] double Speed(std::uint16_t material) const noexcept
        {return Equipped&&Durability&&(material==3||material==4)?2.:1.;}
        void SuccessfulMine(std::uint16_t material) noexcept
        {
            if(Speed(material)==1.)return;
            if(--Durability==0)Equipped=false;
        }
    };
}
