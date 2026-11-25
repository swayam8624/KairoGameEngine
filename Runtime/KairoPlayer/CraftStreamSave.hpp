#pragma once
#include "CraftSave.hpp"
#include "CraftStreamingWindow.hpp"

namespace kairo::player::detail
{
    using StreamCraftWorldEdits=VoxelEdits<StreamingCraftBaseline>;

    // v2 has its own magic/version. A finite v1 world must not silently
    // acquire different exterior generation semantics on load.
    inline bool InStreamCraftWorld(VoxelCoordinate cell) noexcept
    {
        if(cell.Y<0||cell.Y>=CraftExtent)return false;
        const auto owner=CraftOwner(cell);
        const auto x=static_cast<std::int64_t>(owner.X)*CraftExtent;
        const auto z=static_cast<std::int64_t>(owner.Z)*CraftExtent;
        constexpr auto lo=std::numeric_limits<std::int32_t>::min();
        constexpr auto hi=std::numeric_limits<std::int32_t>::max();
        return x-1>=lo&&z-1>=lo&&x+CraftExtent<=hi&&z+CraftExtent<=hi;
    }
    inline constexpr std::array<std::uint8_t,8> StreamCraftSaveMagic{'K','C','R','A','F','T','0','2'};

    inline std::vector<std::uint8_t> EncodeStreamCraftSave(std::uint64_t seed,const StreamCraftWorldEdits& world)
    {
        if(world.Overrides().size()>CraftSaveMaximumEdits)throw std::length_error("Craft save edit cap.");
        std::vector<std::uint8_t> bytes;
        bytes.reserve(CraftSaveHeaderBytes+CraftSaveRecordBytes*world.Overrides().size()+CraftSaveTrailerBytes);
        bytes.insert(bytes.end(),StreamCraftSaveMagic.begin(),StreamCraftSaveMagic.end());
        CraftWrite(bytes,seed,8);CraftWrite(bytes,world.Overrides().size(),4);CraftWrite(bytes,2,4);
        for(const auto& [cell,material]:world.Overrides())
        {
            if(!InStreamCraftWorld(cell)||material>CraftPlaced||material==StreamingCraftBaseline{seed}(cell))
                throw std::invalid_argument("Noncanonical Craft edit.");
            CraftWrite(bytes,std::bit_cast<std::uint32_t>(cell.X),4);
            CraftWrite(bytes,std::bit_cast<std::uint32_t>(cell.Y),4);
            CraftWrite(bytes,std::bit_cast<std::uint32_t>(cell.Z),4);
            CraftWrite(bytes,material,2);
        }
        CraftWrite(bytes,CraftSaveChecksum(bytes),8);
        return bytes;
    }
    struct DecodedStreamCraftSave
    {
        std::uint64_t Seed;
        StreamCraftWorldEdits World;
    };
    inline DecodedStreamCraftSave DecodeStreamCraftSave(std::span<const std::uint8_t> bytes)
    {
        if(bytes.size()<CraftSaveHeaderBytes+CraftSaveTrailerBytes||bytes.size()>CraftSaveMaximumBytes||
            !std::equal(StreamCraftSaveMagic.begin(),StreamCraftSaveMagic.end(),bytes.begin()))
            throw std::invalid_argument("Invalid Craft save header.");
        std::size_t pos=8;
        const auto seed=CraftRead(bytes,pos,8),count=CraftRead(bytes,pos,4),version=CraftRead(bytes,pos,4);
        if(version!=2||count>CraftSaveMaximumEdits||
            bytes.size()!=CraftSaveHeaderBytes+count*CraftSaveRecordBytes+CraftSaveTrailerBytes)
            throw std::invalid_argument("Invalid Craft save version or length.");
        std::size_t trailer=bytes.size()-8;
        if(CraftRead(bytes,trailer,8)!=CraftSaveChecksum(bytes.first(bytes.size()-8)))
            throw std::invalid_argument("Craft save checksum mismatch.");
        StreamCraftWorldEdits result(StreamingCraftBaseline{seed},CraftSaveMaximumEdits);
        std::optional<VoxelCoordinate> previous;
        for(std::uint64_t i=0;i<count;++i)
        {
            const auto x=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(CraftRead(bytes,pos,4)));
            const auto y=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(CraftRead(bytes,pos,4)));
            const auto z=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(CraftRead(bytes,pos,4)));
            const VoxelCoordinate cell{x,y,z};
            const auto material=static_cast<VoxelMaterial>(CraftRead(bytes,pos,2));
            if(!InStreamCraftWorld(cell)||material>CraftPlaced||material==StreamingCraftBaseline{seed}(cell)||
                (previous&&!( *previous<cell))||!result.Set(cell,material))
                throw std::invalid_argument("Invalid or unordered Craft save record.");
            previous=cell;
        }
        return {seed,std::move(result)};
    }
}
