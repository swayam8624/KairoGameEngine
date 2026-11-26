#pragma once
#include "RuntimeCoopSession.hpp"
#include <future>

namespace kairo::player::detail
{
    // Main-thread facade: the worker alone touches the live client while
    // a turn is outstanding. The renderer reads only the last complete copy.
    // Shutdown wakes blocked I/O; join before destroying the socket owner.
    class CoopWorker
    {
    public:
        explicit CoopWorker(std::uint16_t port):m_Client(port),m_Slot(m_Client.Slot()),m_Visible(m_Client.State()){}
        ~CoopWorker()
        {
            if(m_Pending.valid()){m_Client.Interrupt();m_Pending.wait();}
        }
        CoopWorker(const CoopWorker&)=delete;
        CoopWorker& operator=(const CoopWorker&)=delete;
        [[nodiscard]] unsigned Slot() const noexcept{return m_Slot;}
        [[nodiscard]] CoopState Visible() const noexcept{return m_Visible;}
        [[nodiscard]] bool Busy() const noexcept{return m_Pending.valid();}
        [[nodiscard]] bool Failed() const noexcept{return m_Failed;}
        // At most one queued turn. Discard a second key press instead of
        // creating an unbounded backlog or sending a duplicate tick.
        bool Submit(int x,int y)
        {
            if(Busy()||m_Failed||m_Visible.Won()||m_Visible.Tick>=CoopTickLimit)return false;
            ValidateNetworkFrame({NetworkKind::Input,m_Visible.Tick+1,x,y});
            if(std::abs(x)+std::abs(y)>1)throw std::invalid_argument("Diagonal co-op input.");
            m_Pending=std::async(std::launch::async,[this,x,y]{return m_Client.Step(x,y);});return true;
        }
        // A zero-duration poll never waits for the peer. A failed receive
        // permanently latches failure, preserving the last accepted frame.
        bool Poll()
        {
            if(!Busy()||m_Pending.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
            try{m_Visible=m_Pending.get();return true;}
            catch(...){m_Failed=true;throw;}
        }
    private:
        CoopClient m_Client;
        const unsigned m_Slot;
        CoopState m_Visible;
        std::future<CoopState> m_Pending;
        bool m_Failed=false;
    };
}
