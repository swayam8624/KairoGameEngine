#pragma once
#include "CraftEquipment.hpp"
#include "CraftSave.hpp"

namespace kairo::player::detail
{
    // v5 envelope wraps the existing checked v3/v4 session payload. The
    // caller still selects the correct world decoder; never reinterpret a
    // cave baseline as a surface world. Payload is borrowed on decode.
    inline constexpr std::array<std::uint8_t,8> CraftEquipmentMagic{'K','C','R','A','F','T','0','5'};
    inline constexpr std::size_t CraftEquipmentOverhead=8+4+4+2+1+8;
    inline bool HasCraftEquipmentEnvelope(std::span<const std::uint8_t> bytes)
    {
        return bytes.size()>=8&&std::equal(CraftEquipmentMagic.begin(),CraftEquipmentMagic.end(),bytes.begin());
    }
    inline std::vector<std::uint8_t> EncodeCraftEquipmentEnvelope(std::span<const std::uint8_t> payload,
        const CraftEquipment& equipment,std::size_t maximum)
    {
        if(!equipment.Valid()||payload.empty()||payload.size()>maximum||
            payload.size()>std::numeric_limits<std::uint32_t>::max())
            throw std::invalid_argument("Invalid equipment session.");
        std::vector<std::uint8_t> bytes;
        bytes.reserve(payload.size()+CraftEquipmentOverhead);
        bytes.insert(bytes.end(),CraftEquipmentMagic.begin(),CraftEquipmentMagic.end());
        CraftWrite(bytes,5,4);CraftWrite(bytes,payload.size(),4);
        CraftWrite(bytes,equipment.Durability,2);CraftWrite(bytes,equipment.Equipped?1u:0u,1);
        bytes.insert(bytes.end(),payload.begin(),payload.end());
        CraftWrite(bytes,CraftSaveChecksum(bytes),8);
        return bytes;
    }
    struct DecodedCraftEquipmentEnvelope
    {
        CraftEquipment Equipment;
        std::span<const std::uint8_t> Payload;
    };
    inline DecodedCraftEquipmentEnvelope DecodeCraftEquipmentEnvelope(std::span<const std::uint8_t> bytes,std::size_t maximum)
    {
        if(!HasCraftEquipmentEnvelope(bytes)||bytes.size()<CraftEquipmentOverhead)
            throw std::invalid_argument("Invalid equipment session header.");
        std::size_t pos=8;
        const auto version=CraftRead(bytes,pos,4),length=CraftRead(bytes,pos,4);
        const auto durability=CraftRead(bytes,pos,2),equipped=CraftRead(bytes,pos,1);
        if(version!=5||!length||length>maximum||length!=bytes.size()-CraftEquipmentOverhead||
            durability>CraftEquipment::MaximumDurability||equipped>1||(!durability&&equipped))
            throw std::invalid_argument("Invalid equipment session state or length.");
        std::size_t trailer=bytes.size()-8;
        if(CraftRead(bytes,trailer,8)!=CraftSaveChecksum(bytes.first(bytes.size()-8)))
            throw std::invalid_argument("Equipment session checksum mismatch.");
        return {{static_cast<std::uint32_t>(durability),equipped!=0},bytes.subspan(pos,static_cast<std::size_t>(length))};
    }
}
