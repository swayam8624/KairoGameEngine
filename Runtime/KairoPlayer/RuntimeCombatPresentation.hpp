#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace kairo::player::detail
{
    struct CombatPresentationFrame{bool AttackActive,RestartAttack;std::size_t VisibleFlights;};
    // Presentation consumes the accepted-launch serial, never raw input.
    // Multiple accepted launches between frames coalesce into one restart.
    class RuntimeCombatPresentation
    {
        std::uint64_t m_LastLaunch=0;
        double m_Remaining=0,m_Duration;
        std::size_t m_Capacity;
    public:
        RuntimeCombatPresentation(std::size_t capacity,double duration=.35)
            :m_Duration(duration),m_Capacity(capacity)
        {
            if(capacity>512||!std::isfinite(duration)||duration<=0||duration>10)
                throw std::invalid_argument("Invalid combat presentation budget.");
        }
        CombatPresentationFrame Step(std::uint64_t launch,std::size_t flights,double dt)
        {
            if(launch<m_LastLaunch||flights>512||!std::isfinite(dt)||dt<0||dt>1)
                throw std::invalid_argument("Invalid combat presentation sample.");
            const bool restart=launch!=m_LastLaunch;
            m_LastLaunch=launch;
            m_Remaining=restart?m_Duration:std::max(0.,m_Remaining-dt);
            return {m_Remaining>0,restart,std::min(flights,m_Capacity)};
        }
    };
}
