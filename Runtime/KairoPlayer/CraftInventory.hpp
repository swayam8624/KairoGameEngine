#pragma once
#include "CraftStreamSave.hpp"
#include <array>

namespace kairo::player::detail
{
    // One stack per block material. No item duplication on failed native
    // edits: commit inventory only after the occupancy/mesh transaction.
    struct CraftInventory
    {
        static constexpr std::uint32_t StackLimit=999;
        std::array<std::uint32_t,4> Counts{};
        VoxelMaterial Selected=CraftGrass;
        [[nodiscard]] bool Valid() const noexcept
        {
            return Selected>=CraftGrass&&Selected<=CraftPlaced&&
                std::all_of(Counts.begin(),Counts.end(),[](auto n){return n<=StackLimit;});
        }
        [[nodiscard]] std::uint32_t Count(VoxelMaterial m) const
        {
            if(m<CraftGrass||m>CraftPlaced)throw std::invalid_argument("Invalid inventory material.");
            return Counts[m-1];
        }
    };

    template<class Query,class Edit>
    bool MineCraftBlock(CraftInventory& inventory,VoxelCoordinate cell,Query&& query,Edit&& edit)
    {
        if(!inventory.Valid())throw std::invalid_argument("Invalid Craft inventory.");
        const std::optional<VoxelMaterial> material=query(cell);
        if(!material||*material<CraftGrass||*material>CraftPlaced||
            inventory.Count(*material)>=CraftInventory::StackLimit)return false;
        if(!edit(cell,VoxelAir))return false;
        ++inventory.Counts[*material-1];
        return true;
    }
    template<class Query,class Edit,class CanPlace>
    bool PlaceCraftBlock(CraftInventory& inventory,VoxelCoordinate cell,
        Query&& query,Edit&& edit,CanPlace&& canPlace)
    {
        if(!inventory.Valid())throw std::invalid_argument("Invalid Craft inventory.");
        const std::optional<VoxelMaterial> material=query(cell);
        if(!material||*material!=VoxelAir||inventory.Count(inventory.Selected)==0||
            !canPlace(cell))return false;
        if(!edit(cell,inventory.Selected))return false;
        --inventory.Counts[inventory.Selected-1];
        return true;
    }

    // v3 envelope preserves the exact v2 world codec and adds inventory.
    // It is deliberately incompatible with old readers. Legacy v2 is
    // accepted by the game explicitly, with an empty inventory.
    inline constexpr std::array<std::uint8_t,8> CraftSessionMagic{'K','C','R','A','F','T','0','3'};
    inline constexpr std::size_t CraftSessionOverhead=8+4+4+4*4+2+8;
    inline constexpr std::size_t CraftSessionMaximumBytes=CraftSaveMaximumBytes+CraftSessionOverhead;
    inline std::vector<std::uint8_t> EncodeCraftSession(std::uint64_t seed,
        const StreamCraftWorldEdits& world,const CraftInventory& inventory)
    {
        if(!inventory.Valid())throw std::invalid_argument("Invalid Craft inventory.");
        const auto payload=EncodeStreamCraftSave(seed,world);
        std::vector<std::uint8_t> bytes;
        bytes.reserve(payload.size()+CraftSessionOverhead);
        bytes.insert(bytes.end(),CraftSessionMagic.begin(),CraftSessionMagic.end());
        CraftWrite(bytes,3,4);CraftWrite(bytes,payload.size(),4);
        for(auto count:inventory.Counts)CraftWrite(bytes,count,4);
        CraftWrite(bytes,inventory.Selected,2);
        bytes.insert(bytes.end(),payload.begin(),payload.end());
        CraftWrite(bytes,CraftSaveChecksum(bytes),8);
        return bytes;
    }
    struct DecodedCraftSession
    {
        std::uint64_t Seed;
        StreamCraftWorldEdits World;
        CraftInventory Inventory;
    };
    inline DecodedCraftSession DecodeCraftSession(std::span<const std::uint8_t> bytes)
    {
        if(bytes.size()<CraftSessionOverhead||bytes.size()>CraftSessionMaximumBytes||
            !std::equal(CraftSessionMagic.begin(),CraftSessionMagic.end(),bytes.begin()))
            throw std::invalid_argument("Invalid Craft session header.");
        std::size_t pos=8;
        const auto version=CraftRead(bytes,pos,4),length=CraftRead(bytes,pos,4);
        if(version!=3||length>CraftSaveMaximumBytes||bytes.size()!=length+CraftSessionOverhead)
            throw std::invalid_argument("Invalid Craft session version or length.");
        CraftInventory inventory;
        for(auto& count:inventory.Counts)count=static_cast<std::uint32_t>(CraftRead(bytes,pos,4));
        inventory.Selected=static_cast<VoxelMaterial>(CraftRead(bytes,pos,2));
        if(!inventory.Valid())throw std::invalid_argument("Invalid saved inventory.");
        std::size_t trailer=bytes.size()-8;
        if(CraftRead(bytes,trailer,8)!=CraftSaveChecksum(bytes.first(bytes.size()-8)))
            throw std::invalid_argument("Craft session checksum mismatch.");
        auto decoded=DecodeStreamCraftSave(bytes.subspan(pos,static_cast<std::size_t>(length)));
        return {decoded.Seed,std::move(decoded.World),inventory};
    }
}
