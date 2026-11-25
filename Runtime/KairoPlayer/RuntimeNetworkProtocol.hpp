#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace kairo::player::detail
{
    // Fixed v1 game protocol, deliberately separate from TCP/UDP. All words
    // are big endian; no C++ padding, host endianness or ABI crosses the wire.
    // Every frame is 20 bytes: magic, version, kind, two reserved bytes,
    // tick, x, y. Input uses unit axes; Snapshot uses integer world cells.
    enum class NetworkKind:std::uint8_t{Input=1,Snapshot=2};
    struct NetworkFrame
    {
        NetworkKind Kind;std::uint32_t Tick;std::int32_t X,Y;
        friend bool operator==(const NetworkFrame&,const NetworkFrame&)=default;
    };
    using NetworkBytes=std::array<std::uint8_t,20>;
    inline constexpr std::int32_t NetworkWorldLimit=1000000;
    // Validate before encoding and after decoding so malformed frames never
    // enter game state. Tick zero is reserved for the initial snapshot.
    inline void ValidateNetworkFrame(NetworkFrame f)
    {
        if(f.Kind==NetworkKind::Input)
        {
            if(!f.Tick||f.X< -1||f.X>1||f.Y< -1||f.Y>1)throw std::invalid_argument("Invalid network input.");
        }
        else if(f.Kind==NetworkKind::Snapshot)
        {
            if(f.X< -NetworkWorldLimit||f.X>NetworkWorldLimit||f.Y< -NetworkWorldLimit||f.Y>NetworkWorldLimit)
                throw std::invalid_argument("Invalid network snapshot.");
        }
        else throw std::invalid_argument("Unknown network frame kind.");
    }
    [[nodiscard]] inline NetworkBytes EncodeNetworkFrame(NetworkFrame f)
    {
        ValidateNetworkFrame(f);
        NetworkBytes bytes{'K','N','E','T',1,static_cast<std::uint8_t>(f.Kind),0,0};
        const auto put=[&](unsigned offset,std::uint32_t value)
        {for(unsigned i=0;i<4;++i)bytes[offset+i]=static_cast<std::uint8_t>(value>>(24-8*i));};
        put(8,f.Tick);put(12,std::bit_cast<std::uint32_t>(f.X));put(16,std::bit_cast<std::uint32_t>(f.Y));
        return bytes;
    }
    [[nodiscard]] inline NetworkFrame DecodeNetworkFrame(std::span<const std::uint8_t> bytes)
    {
        if(bytes.size()!=20||bytes[0]!='K'||bytes[1]!='N'||bytes[2]!='E'||bytes[3]!='T'||
            bytes[4]!=1||bytes[6]||bytes[7])throw std::invalid_argument("Invalid network frame header.");
        const auto get=[&](unsigned offset)
        {
            std::uint32_t value=0;
            for(unsigned i=0;i<4;++i)value=(value<<8)|bytes[offset+i];
            return value;
        };
        NetworkFrame f{static_cast<NetworkKind>(bytes[5]),get(8),
            std::bit_cast<std::int32_t>(get(12)),std::bit_cast<std::int32_t>(get(16))};
        ValidateNetworkFrame(f);return f;
    }
    // A narrow authoritative game model. Only validated input increments
    // the tick; the client cannot dictate position. Duplicate, stale or
    // future ticks reject without mutation. World bounds clamp movement.
    class NetworkAuthority
    {
    public:
        [[nodiscard]] NetworkFrame Snapshot() const noexcept{return m_State;}
        [[nodiscard]] NetworkFrame Apply(NetworkFrame input)
        {
            ValidateNetworkFrame(input);
            if(input.Kind!=NetworkKind::Input||m_State.Tick==UINT32_MAX||input.Tick!=m_State.Tick+1)
                throw std::invalid_argument("Unexpected input tick.");
            auto next=m_State;next.Tick=input.Tick;
            const auto clamp=[](std::int32_t p){return p< -NetworkWorldLimit?-NetworkWorldLimit:p>NetworkWorldLimit?NetworkWorldLimit:p;};
            next.X=clamp(next.X+input.X);next.Y=clamp(next.Y+input.Y);
            m_State=next;return m_State;
        }
    private:
        NetworkFrame m_State{NetworkKind::Snapshot,0,0,0};
    };
}
