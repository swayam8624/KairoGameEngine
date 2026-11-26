#pragma once
#include "RuntimeCoopArena.hpp"
#include "RuntimeLoopbackTcp.hpp"

namespace kairo::player::detail
{
    // This dedicated loopback service has a fixed two-seat protocol. A
    // connection's slot comes from accept order, never from client input.
    // KNET is reused as the scalar frame codec; this is a distinct service
    // from the single-client lab, not a backwards-compatible handshake.
    inline constexpr auto CoopWait=std::chrono::seconds(30);
    inline void SendCoopState(const LoopbackTcp& peer,CoopState state)
    {for(const auto f:EncodeCoopState(state))peer.Send(f,CoopWait);}
    inline CoopState ReceiveCoopState(const LoopbackTcp& peer,std::uint32_t tick)
    {return DecodeCoopState({peer.Receive(CoopWait),peer.Receive(CoopWait)},tick);}
    // One writer and one reader per socket; a disconnect, invalid input or
    // timeout aborts the entire match. No partial turn is committed.
    inline CoopState RunCoopServer(const LoopbackTcp& listener)
    {
        auto first=listener.Accept();auto second=listener.Accept();
        CoopAuthority authority;
        first.Send({NetworkKind::Snapshot,0,0,0});second.Send({NetworkKind::Snapshot,0,1,0});
        SendCoopState(first,authority.State());SendCoopState(second,authority.State());
        while(!authority.State().Won()&&authority.State().Tick<CoopTickLimit)
        {
            const std::array inputs{first.Receive(CoopWait),second.Receive(CoopWait)};
            const auto state=authority.Apply(inputs);
            SendCoopState(first,state);SendCoopState(second,state);
        }
        return authority.State();
    }
    class CoopClient
    {
    public:
        explicit CoopClient(std::uint16_t port):m_Peer(LoopbackTcp::Connect(port))
        {
            const auto hello=m_Peer.Receive(CoopWait);
            if(hello.Kind!=NetworkKind::Snapshot||hello.Tick||hello.X<0||hello.X>1||hello.Y)
                throw std::invalid_argument("Invalid co-op assignment.");
            m_Slot=static_cast<unsigned>(hello.X);m_State=ReceiveCoopState(m_Peer,0);
        }
        [[nodiscard]] unsigned Slot() const noexcept{return m_Slot;}
        [[nodiscard]] CoopState State() const noexcept{return m_State;}
        // Main thread must not read State concurrently with Step. On failure
        // the socket cannot be reused. GUI caller can interrupt an in-flight
        // step with Shutdown without concurrently closing its handle.
        void Interrupt() const noexcept{m_Peer.Shutdown();}
        [[nodiscard]] CoopState Step(int x,int y)
        {
            if(m_State.Won()||m_State.Tick>=CoopTickLimit)throw std::invalid_argument("Co-op match ended.");
            const NetworkFrame input{NetworkKind::Input,m_State.Tick+1,x,y};ValidateNetworkFrame(input);
            if(std::abs(x)+std::abs(y)>1)throw std::invalid_argument("Diagonal co-op input.");
            m_Peer.Send(input,CoopWait);const auto next=ReceiveCoopState(m_Peer,input.Tick);
            // Verify the server moved each player at most one legal cell.
            // The local player may be blocked by the other player's choice.
            for(unsigned i=0;i<2;++i)
            {
                const auto a=m_State.Actors[i],b=next.Actors[i];
                if(std::abs(a.X-b.X)+std::abs(a.Y-b.Y)>1||b==m_State.Actors[1-i])
                    throw std::invalid_argument("Impossible co-op snapshot movement.");
            }
            const auto before=m_State.Actors[m_Slot],after=next.Actors[m_Slot];
            if(after!=before&&after!=CoopCell{before.X+x,before.Y+y})throw std::invalid_argument("Co-op input mismatch.");
            m_State=next;return m_State;
        }
    private:
        LoopbackTcp m_Peer;
        unsigned m_Slot=0;
        CoopState m_State;
    };
}
