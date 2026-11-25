#pragma once
#include "CraftWorld.hpp"
#include <bit>
#include <cstdint>
#include <span>
#include <vector>

namespace kairo::player::detail
{
    using CraftWorldEdits=VoxelEdits<CraftBaseline>;
    inline constexpr std::size_t CraftSaveMaximumEdits=65536;
    inline constexpr std::size_t CraftSaveHeaderBytes=24,CraftSaveRecordBytes=14,CraftSaveTrailerBytes=8;
    inline constexpr std::size_t CraftSaveMaximumBytes=CraftSaveHeaderBytes+
        CraftSaveRecordBytes*CraftSaveMaximumEdits+CraftSaveTrailerBytes;

    // Stable little-endian format: magic[8], seed u64, count u32, version u32,
    // sorted records (signed X/Y/Z i32 + material u16), FNV-1a u64.
    // The checksum detects accidental corruption, not malicious modification.
    inline std::uint64_t CraftSaveChecksum(std::span<const std::uint8_t> bytes) noexcept
    {
        std::uint64_t hash=14695981039346656037ULL;
        for(auto b:bytes){hash^=b;hash*=1099511628211ULL;}
        return hash;
    }
    inline void CraftWrite(std::vector<std::uint8_t>& bytes,std::uint64_t value,unsigned count)
    {for(unsigned i=0;i<count;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
    inline std::uint64_t CraftRead(std::span<const std::uint8_t> bytes,std::size_t& pos,unsigned count)
    {
        if(pos>bytes.size()||count>bytes.size()-pos)throw std::invalid_argument("Truncated Craft save.");
        std::uint64_t value=0;
        for(unsigned i=0;i<count;++i)value|=std::uint64_t(bytes[pos++])<<(8*i);
        return value;
    }
    inline constexpr std::array<std::uint8_t,8> CraftSaveMagic{'K','C','R','A','F','T','0','1'};

    inline std::vector<std::uint8_t> EncodeCraftSave(std::uint64_t seed,const CraftWorldEdits& world)
    {
        if(world.Overrides().size()>CraftSaveMaximumEdits)throw std::length_error("Craft save edit cap.");
        std::vector<std::uint8_t> bytes;
        bytes.reserve(CraftSaveHeaderBytes+CraftSaveRecordBytes*world.Overrides().size()+CraftSaveTrailerBytes);
        bytes.insert(bytes.end(),CraftSaveMagic.begin(),CraftSaveMagic.end());
        CraftWrite(bytes,seed,8);CraftWrite(bytes,world.Overrides().size(),4);CraftWrite(bytes,1,4);
        for(const auto& [cell,material]:world.Overrides())
        {
            if(!InCraftWorld(cell)||material>CraftPlaced||material==CraftBaseline{seed}(cell))
                throw std::invalid_argument("Noncanonical Craft edit.");
            CraftWrite(bytes,std::bit_cast<std::uint32_t>(cell.X),4);
            CraftWrite(bytes,std::bit_cast<std::uint32_t>(cell.Y),4);
            CraftWrite(bytes,std::bit_cast<std::uint32_t>(cell.Z),4);
            CraftWrite(bytes,material,2);
        }
        CraftWrite(bytes,CraftSaveChecksum(bytes),8);
        return bytes;
    }
    struct DecodedCraftSave
    {
        std::uint64_t Seed;
        CraftWorldEdits World;
    };
    inline DecodedCraftSave DecodeCraftSave(std::span<const std::uint8_t> bytes)
    {
        if(bytes.size()<CraftSaveHeaderBytes+CraftSaveTrailerBytes||bytes.size()>CraftSaveMaximumBytes||
            !std::equal(CraftSaveMagic.begin(),CraftSaveMagic.end(),bytes.begin()))
            throw std::invalid_argument("Invalid Craft save header.");
        std::size_t pos=8;
        const auto seed=CraftRead(bytes,pos,8),count=CraftRead(bytes,pos,4),version=CraftRead(bytes,pos,4);
        if(version!=1||count>CraftSaveMaximumEdits||
            bytes.size()!=CraftSaveHeaderBytes+count*CraftSaveRecordBytes+CraftSaveTrailerBytes)
            throw std::invalid_argument("Invalid Craft save version or length.");
        std::size_t trailer=bytes.size()-8;
        if(CraftRead(bytes,trailer,8)!=CraftSaveChecksum(bytes.first(bytes.size()-8)))
            throw std::invalid_argument("Craft save checksum mismatch.");
        CraftWorldEdits result(CraftBaseline{seed},CraftSaveMaximumEdits);
        std::optional<VoxelCoordinate> previous;
        for(std::uint64_t i=0;i<count;++i)
        {
            const auto x=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(CraftRead(bytes,pos,4)));
            const auto y=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(CraftRead(bytes,pos,4)));
            const auto z=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(CraftRead(bytes,pos,4)));
            const VoxelCoordinate cell{x,y,z};
            const auto material=static_cast<VoxelMaterial>(CraftRead(bytes,pos,2));
            if(!InCraftWorld(cell)||material>CraftPlaced||material==CraftBaseline{seed}(cell)||
                (previous&&!( *previous<cell))||!result.Set(cell,material))
                throw std::invalid_argument("Invalid or unordered Craft save record.");
            previous=cell;
        }
        return {seed,std::move(result)};
    }
}
