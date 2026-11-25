#pragma once
#include "VoxelPlayer.hpp"
#include <bit>
#include <span>
#include <vector>

namespace kairo::player::detail
{
    // The checkpoint is session metadata, not an alternative world codec.
    // Only live player state is written; death/respawn remains a game rule.
    struct CraftCheckpoint
    {
        std::array<double,3> Feet{.5,8,.5};
        double Yaw=0,Pitch=0,DayPhase=.25;
        std::uint32_t Health=20,Hunger=20;
        bool operator==(const CraftCheckpoint&) const=default;
        [[nodiscard]] bool Valid() const noexcept
        {
            for(double v:Feet)if(!std::isfinite(v)||std::abs(v)>1e9)return false;
            return std::isfinite(Yaw)&&std::abs(Yaw)<=3.14159265358979323846&&
                std::isfinite(Pitch)&&std::abs(Pitch)<=1.45&&
                std::isfinite(DayPhase)&&DayPhase>=0&&DayPhase<1&&
                Health>=1&&Health<=20&&Hunger<=20;
        }
    };
    // A saved position is accepted only if real occupancy permits the body
    // and supplies physical support. Airborne or obstructed positions use
    // the caller's normal spawn search; no arbitrary teleport into solids.
    template<class Query>
    std::optional<VoxelPlayer> RestoreCraftCheckpoint(const CraftCheckpoint& state,
        const VoxelPlayerConfig& config,Query&& query)
    {
        if(!state.Valid())throw std::invalid_argument("Invalid Craft checkpoint.");
        VoxelPlayer body{state.Feet};
        ValidateVoxelPlayer(body,config);
        if(VoxelPlayerBlocked(body,config,query))return {};
        auto probe=body;probe.Feet[1]-=.000001;
        // Unknown cells must never be mistaken for physical support.
        if(!VoxelPlayerBlocked(probe,config,[&](VoxelCoordinate c){return query(c).value_or(VoxelAir);}))return {};
        body.Grounded=true;return body;
    }

    // v6 wraps an existing v5 equipment session. The nested world decoder
    // remains authoritative for v3/v4 and its own checksum. Explicit LE
    // byte serialization avoids host layout and alignment dependencies.
    inline constexpr std::array<std::uint8_t,8> CraftCheckpointMagic{'K','C','R','A','F','T','0','6'};
    inline constexpr std::array<std::uint8_t,8> CraftCheckpointInnerMagic{'K','C','R','A','F','T','0','5'};
    inline constexpr std::size_t CraftCheckpointOverhead=8+4+4+6*8+2+8;
    inline bool HasCraftCheckpoint(std::span<const std::uint8_t> bytes) noexcept
    {return bytes.size()>=8&&std::equal(CraftCheckpointMagic.begin(),CraftCheckpointMagic.end(),bytes.begin());}
    namespace checkpoint_codec
    {
        inline void Write(std::vector<std::uint8_t>& out,std::uint64_t v,unsigned n)
        {for(unsigned i=0;i<n;++i)out.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
        inline std::uint64_t Read(std::span<const std::uint8_t> bytes,std::size_t& pos,unsigned n)
        {
            if(pos>bytes.size()||n>bytes.size()-pos)throw std::invalid_argument("Truncated checkpoint.");
            std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint64_t(bytes[pos++])<<(8*i);return v;
        }
        inline std::uint64_t Checksum(std::span<const std::uint8_t> bytes) noexcept
        {std::uint64_t h=14695981039346656037ULL;for(auto b:bytes){h^=b;h*=1099511628211ULL;}return h;}
        inline bool IsInner(std::span<const std::uint8_t> payload) noexcept
        {return payload.size()>=8&&std::equal(CraftCheckpointInnerMagic.begin(),CraftCheckpointInnerMagic.end(),payload.begin());}
    }
    inline std::vector<std::uint8_t> EncodeCraftCheckpoint(std::span<const std::uint8_t> payload,
        const CraftCheckpoint& state,std::size_t maximum)
    {
        using namespace checkpoint_codec;
        if(!state.Valid()||!IsInner(payload)||payload.size()>maximum||payload.size()>std::numeric_limits<std::uint32_t>::max()||
            payload.size()>std::numeric_limits<std::size_t>::max()-CraftCheckpointOverhead)
            throw std::invalid_argument("Invalid Craft checkpoint payload.");
        std::vector<std::uint8_t> bytes;bytes.reserve(payload.size()+CraftCheckpointOverhead);
        bytes.insert(bytes.end(),CraftCheckpointMagic.begin(),CraftCheckpointMagic.end());
        Write(bytes,6,4);Write(bytes,payload.size(),4);
        for(double v:state.Feet)Write(bytes,std::bit_cast<std::uint64_t>(v),8);
        for(double v:{state.Yaw,state.Pitch,state.DayPhase})Write(bytes,std::bit_cast<std::uint64_t>(v),8);
        Write(bytes,state.Health,1);Write(bytes,state.Hunger,1);
        bytes.insert(bytes.end(),payload.begin(),payload.end());Write(bytes,Checksum(bytes),8);return bytes;
    }
    struct DecodedCraftCheckpoint
    {
        CraftCheckpoint State;
        std::span<const std::uint8_t> Payload;
    };
    inline DecodedCraftCheckpoint DecodeCraftCheckpoint(std::span<const std::uint8_t> bytes,std::size_t maximum)
    {
        using namespace checkpoint_codec;
        if(!HasCraftCheckpoint(bytes)||bytes.size()<CraftCheckpointOverhead)throw std::invalid_argument("Invalid checkpoint header.");
        std::size_t pos=8;const auto version=Read(bytes,pos,4),length=Read(bytes,pos,4);
        if(version!=6||length>maximum||length!=bytes.size()-CraftCheckpointOverhead)throw std::invalid_argument("Invalid checkpoint version or length.");
        CraftCheckpoint state;
        for(double& v:state.Feet)v=std::bit_cast<double>(Read(bytes,pos,8));
        state.Yaw=std::bit_cast<double>(Read(bytes,pos,8));state.Pitch=std::bit_cast<double>(Read(bytes,pos,8));state.DayPhase=std::bit_cast<double>(Read(bytes,pos,8));
        state.Health=static_cast<std::uint32_t>(Read(bytes,pos,1));state.Hunger=static_cast<std::uint32_t>(Read(bytes,pos,1));
        if(!state.Valid())throw std::invalid_argument("Invalid checkpoint state.");
        std::size_t trailer=bytes.size()-8;
        if(Read(bytes,trailer,8)!=Checksum(bytes.first(bytes.size()-8)))throw std::invalid_argument("Checkpoint checksum mismatch.");
        const auto payload=bytes.subspan(pos,static_cast<std::size_t>(length));
        if(!IsInner(payload))throw std::invalid_argument("Invalid checkpoint inner version.");
        return {state,payload};
    }
}
