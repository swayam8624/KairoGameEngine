#pragma once
#include "RuntimeCoopArena.hpp"
#include "RuntimeLoopbackTcp.hpp"
#include <cstdlib>
#include <optional>
#include <string_view>


namespace kairo::player::detail
{
    // This dedicated loopback service has a fixed two-seat protocol. A
    // Protected connections have ticket-bound slots; open labs use accept order.
    // KNET is reused as the scalar frame codec; this is a distinct service
    // from the single-client lab, not a backwards-compatible handshake.
    inline constexpr auto CoopWait=std::chrono::seconds(30);
    // The optional protected local lab uses two independent 256-bit bearer
    // credentials. They are passed by environment, never printed or put in
    // command arguments. This is not encryption or an Internet auth protocol.
    using CoopTicket=std::array<std::uint32_t,8>;
    using CoopTickets=std::array<CoopTicket,2>;
    inline CoopTicket ParseCoopTicket(std::string_view value)
    {
        if(value.size()!=64)throw std::invalid_argument("Invalid co-op ticket.");
        CoopTicket result{};
        for(unsigned i=0;i<64;++i)
        {
            const char c=value[i];
            unsigned digit;
            if(c>='0'&&c<='9')digit=static_cast<unsigned>(c-'0');
            else if(c>='a'&&c<='f')digit=static_cast<unsigned>(c-'a'+10);
            else throw std::invalid_argument("Invalid co-op ticket.");
            result[i/8]=(result[i/8]<<4)|digit;
        }
        return result;
    }
    inline std::optional<CoopTickets> CoopServerTickets()
    {
        const auto* a=std::getenv("KAIRO_COOP_SEAT0");
        const auto* b=std::getenv("KAIRO_COOP_SEAT1");
        if(!a&&!b)return std::nullopt;
        if(!a||!b)throw std::invalid_argument("Both co-op seat tickets required.");
        CoopTickets tickets{ParseCoopTicket(a),ParseCoopTicket(b)};
        if(tickets[0]==tickets[1])throw std::invalid_argument("Co-op seat tickets must differ.");
        return tickets;
    }
    inline void SendCoopTicket(const LoopbackTcp& peer, const CoopTicket& ticket)
    {
        for(unsigned i=0;i<ticket.size();++i)
            peer.Send({NetworkKind::Snapshot,ticket[i],static_cast<int>(i),7291});
    }
    inline CoopTicket ReceiveCoopTicket(const LoopbackTcp& peer)
    {
        CoopTicket ticket{};
        // One absolute handshake deadline, not eight independent waits.
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        for(unsigned i=0;i<ticket.size();++i)
        {
            const auto left=std::chrono::ceil<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now());
            if(left.count()<=0)throw std::runtime_error("Co-op admission timeout.");
            const auto f=peer.Receive(left);
            if(f.Kind!=NetworkKind::Snapshot||f.X!=static_cast<int>(i)||f.Y!=7291)
                throw std::invalid_argument("Malformed co-op admission.");
            ticket[i]=f.Tick;
        }
        return ticket;
    }
    inline bool SameCoopTicket(const CoopTicket& a,const CoopTicket& b) noexcept
    {
        std::uint32_t diff=0;
        for(unsigned i=0;i<a.size();++i)diff|=a[i]^b[i];
        return diff==0;
    }
    inline std::array<LoopbackTcp,2> AdmitCoopPeers(const LoopbackTcp& listener,const std::optional<CoopTickets>& tickets)
    {
        if(!tickets)return {listener.Accept(),listener.Accept()};
        std::array<std::optional<LoopbackTcp>,2> seats;
        // Reject malformed, incorrect or already-used tickets without
        // consuming a seat. Cap attempts to bound local resource exhaustion.
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
        for(unsigned attempt=0;attempt<8;++attempt)
        {
            const auto remaining=std::chrono::ceil<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now());
            if(remaining.count()<=0)throw std::runtime_error("Co-op admission deadline exceeded.");
            auto peer=listener.Accept(remaining);
            try
            {
                const auto ticket=ReceiveCoopTicket(peer);
                const bool first=SameCoopTicket(ticket,(*tickets)[0]);
                const bool second=SameCoopTicket(ticket,(*tickets)[1]);
                if(first&&!seats[0])seats[0].emplace(std::move(peer));
                else if(second&&!seats[1])seats[1].emplace(std::move(peer));
            }
            catch(const std::invalid_argument&){} // Reject only this connection.
            catch(const std::runtime_error&){}
            if(seats[0]&&seats[1])return {std::move(*seats[0]),std::move(*seats[1])};
        }
        throw std::runtime_error("Co-op admission attempt limit exceeded.");
    }
    inline void SendCoopState(const LoopbackTcp& peer,CoopState state)
    {for(const auto f:EncodeCoopState(state))peer.Send(f,CoopWait);}
    inline CoopState ReceiveCoopState(const LoopbackTcp& peer,std::uint32_t tick)
    {return DecodeCoopState({peer.Receive(CoopWait),peer.Receive(CoopWait)},tick);}
    // One writer and one reader per socket; a disconnect, invalid input or
    // timeout aborts the entire match. No partial turn is committed.
    inline CoopState RunCoopServer(const LoopbackTcp& listener)
    {
        auto peers=AdmitCoopPeers(listener,CoopServerTickets());
        auto& first=peers[0];auto& second=peers[1];
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
            if(const auto* ticket=std::getenv("KAIRO_COOP_TICKET"))
                SendCoopTicket(m_Peer,ParseCoopTicket(ticket));
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
