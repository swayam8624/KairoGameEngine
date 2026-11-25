#pragma once
#include "CraftCaveWorld.hpp"
#include "CraftInventory.hpp"

namespace kairo::player::detail
{
    using CaveCraftWorldEdits=VoxelEdits<CaveCraftBaseline>;
    // Cave generation differs from every older world format. Never interpret
    // a flat-world edit log against this baseline, even when its seed matches.
    inline constexpr std::array<std::uint8_t,8> CaveCraftSessionMagic{'K','C','R','A','F','T','0','4'};
    inline constexpr std::size_t CaveCraftHeaderBytes=CraftSaveHeaderBytes+4*4+2;
    inline constexpr std::size_t CaveCraftMaximumBytes=CaveCraftHeaderBytes+
        CraftSaveRecordBytes*CraftSaveMaximumEdits+CraftSaveTrailerBytes;

    inline std::vector<std::uint8_t> EncodeCaveCraftSession(std::uint64_t seed,
        const CaveCraftWorldEdits& world,const CraftInventory& inventory)
    {
        if(!inventory.Valid())throw std::invalid_argument("Invalid cave inventory.");
        const auto& edits=world.Overrides();
        if(edits.size()>CraftSaveMaximumEdits)throw std::length_error("Cave edit cap.");
        std::vector<std::uint8_t> bytes;
        bytes.reserve(CaveCraftHeaderBytes+CraftSaveRecordBytes*edits.size()+CraftSaveTrailerBytes);
        bytes.insert(bytes.end(),CaveCraftSessionMagic.begin(),CaveCraftSessionMagic.end());
        CraftWrite(bytes,seed,8);CraftWrite(bytes,edits.size(),4);CraftWrite(bytes,4,4);
        for(auto count:inventory.Counts)CraftWrite(bytes,count,4);
        CraftWrite(bytes,inventory.Selected,2);
        for(const auto& [cell,material]:edits)
        {
            if(!InCaveWorld(cell)||material>CraftPlaced||material==CaveCraftBaseline{seed}(cell))
                throw std::invalid_argument("Noncanonical cave edit.");
            CraftWrite(bytes,std::bit_cast<std::uint32_t>(cell.X),4);
            CraftWrite(bytes,std::bit_cast<std::uint32_t>(cell.Y),4);
            CraftWrite(bytes,std::bit_cast<std::uint32_t>(cell.Z),4);
            CraftWrite(bytes,material,2);
        }
        CraftWrite(bytes,CraftSaveChecksum(bytes),8);
        return bytes;
    }
    struct DecodedCaveCraftSession
    {
        std::uint64_t Seed;
        CaveCraftWorldEdits World;
        CraftInventory Inventory;
    };
    inline DecodedCaveCraftSession DecodeCaveCraftSession(std::span<const std::uint8_t> bytes)
    {
        if(bytes.size()<CaveCraftHeaderBytes+CraftSaveTrailerBytes||bytes.size()>CaveCraftMaximumBytes||
            !std::equal(CaveCraftSessionMagic.begin(),CaveCraftSessionMagic.end(),bytes.begin()))
            throw std::invalid_argument("Invalid cave session header.");
        std::size_t pos=8;
        const auto seed=CraftRead(bytes,pos,8),count=CraftRead(bytes,pos,4),version=CraftRead(bytes,pos,4);
        if(version!=4||count>CraftSaveMaximumEdits||
            bytes.size()!=CaveCraftHeaderBytes+count*CraftSaveRecordBytes+CraftSaveTrailerBytes)
            throw std::invalid_argument("Invalid cave session version or length.");
        std::size_t trailer=bytes.size()-8;
        if(CraftRead(bytes,trailer,8)!=CraftSaveChecksum(bytes.first(bytes.size()-8)))
            throw std::invalid_argument("Cave session checksum mismatch.");
        CraftInventory inventory;
        for(auto& n:inventory.Counts)n=static_cast<std::uint32_t>(CraftRead(bytes,pos,4));
        inventory.Selected=static_cast<VoxelMaterial>(CraftRead(bytes,pos,2));
        if(!inventory.Valid())throw std::invalid_argument("Invalid saved cave inventory.");
        CaveCraftWorldEdits world(CaveCraftBaseline{seed},CraftSaveMaximumEdits);
        std::optional<VoxelCoordinate> previous;
        for(std::uint64_t i=0;i<count;++i)
        {
            const auto x=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(CraftRead(bytes,pos,4)));
            const auto y=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(CraftRead(bytes,pos,4)));
            const auto z=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(CraftRead(bytes,pos,4)));
            const VoxelCoordinate cell{x,y,z};
            const auto material=static_cast<VoxelMaterial>(CraftRead(bytes,pos,2));
            if(!InCaveWorld(cell)||material>CraftPlaced||material==CaveCraftBaseline{seed}(cell)||
                (previous&& !(*previous<cell))||!world.Set(cell,material))
                throw std::invalid_argument("Invalid or unordered cave record.");
            previous=cell;
        }
        return {seed,std::move(world),inventory};
    }
}
