#pragma once
#include "RuntimeNetworkProtocol.hpp"
#include <algorithm>
#include <limits>

namespace kairo::player::detail
{
    // Client-side prediction with a bounded unacknowledged input journal.
    // Server snapshots are authoritative; after accepting an acknowledgment,
    // replay only the later inputs. The caller owns transport and retransmit
    // scheduling. This class never sends a packet or changes server state.
    class NetworkPrediction
    {
    public:
        static constexpr std::size_t MaximumPending=256;
        // Predicted state is for responsive presentation, not authoritative
        // collision or score decisions. Acknowledged state is server truth.
        [[nodiscard]] NetworkFrame Predicted() const noexcept{return m_Predicted;}
        [[nodiscard]] NetworkFrame Acknowledged() const noexcept{return m_Acknowledged;}
        // Borrowed journal: invalidated by prediction or reconciliation.
        [[nodiscard]] const std::vector<NetworkFrame>& Pending() const noexcept{return m_Pending;}
        // Validate one local input, append it before publishing prediction.
        // Backpressure is explicit when the journal reaches its limit.
        [[nodiscard]] NetworkFrame Predict(std::int32_t x,std::int32_t y)
        {
            if(m_Pending.size()>=MaximumPending||m_Predicted.Tick==std::numeric_limits<std::uint32_t>::max())
                throw std::length_error("Network prediction journal is full.");
            const NetworkFrame input{NetworkKind::Input,m_Predicted.Tick+1,x,y};
            ValidateNetworkFrame(input);
            const auto next=Advance(m_Predicted,input);
            m_Pending.push_back(input);m_Predicted=next;return input;
        }
        // Return false for an old/repeated acknowledgment. Reject snapshots
        // claiming an unsent tick. Validate everything before erasing history.
        // On correction, rebase prediction on the authoritative position and
        // replay remaining inputs exactly once in tick order.
        bool Reconcile(NetworkFrame snapshot)
        {
            ValidateNetworkFrame(snapshot);
            if(snapshot.Kind!=NetworkKind::Snapshot||snapshot.Tick>m_Predicted.Tick)
                throw std::invalid_argument("Invalid prediction acknowledgment.");
            if(snapshot.Tick<=m_Acknowledged.Tick)return false;
            auto first=std::upper_bound(m_Pending.begin(),m_Pending.end(),snapshot.Tick,
                [](std::uint32_t tick,const NetworkFrame& input){return tick<input.Tick;});
            auto corrected=snapshot;
            for(auto it=first;it!=m_Pending.end();++it)corrected=Advance(corrected,*it);
            m_Pending.erase(m_Pending.begin(),first);
            m_Acknowledged=snapshot;m_Predicted=corrected;return true;
        }
    private:
        // Same bounded integer movement rule as NetworkAuthority. A real
        // game must share its deterministic physics step with the server.
        static NetworkFrame Advance(NetworkFrame state,NetworkFrame input) noexcept
        {
            state.Tick=input.Tick;
            state.X=std::clamp(state.X+input.X,-NetworkWorldLimit,NetworkWorldLimit);
            state.Y=std::clamp(state.Y+input.Y,-NetworkWorldLimit,NetworkWorldLimit);
            return state;
        }
        NetworkFrame m_Acknowledged{NetworkKind::Snapshot,0,0,0};
        NetworkFrame m_Predicted=m_Acknowledged;
        std::vector<NetworkFrame> m_Pending;
    };
}
