#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace kairo::player::detail
{
    // Health can be restored from a live checkpoint. Damage is measured in whole hit points;
    // a fall of at most three world units is safe. A streaming boundary is
    // not a physical landing and must never inflict damage.
    class CraftVitals
    {
        // Health is integer HP; peak is the highest feet Y since last landing.
        std::uint32_t m_Health=20;
        std::optional<double> m_Peak;
    public:
        // Shared maximum for checkpoint validation and HUD normalization.
        static constexpr std::uint32_t MaximumHealth=20;
        // Read current HP for presentation and nutrition recovery.
        [[nodiscard]] std::uint32_t Health() const noexcept{return m_Health;}
        // Zero HP terminates survival actions until explicit respawn.
        [[nodiscard]] bool Dead() const noexcept{return m_Health==0;}
        // Saturating subtraction avoids unsigned underflow.
        void Damage(std::uint32_t points) noexcept{m_Health-=std::min(points,m_Health);}
        // Heal only living actors, saturating at maximum HP.
        void Heal(std::uint32_t points) noexcept{if(!Dead())m_Health+=std::min(points,MaximumHealth-m_Health);}
        // Checkpoint restoration rejects dead/out-of-range state before write;
        // feetY resets fall history so loading does not synthesize fall damage.
        void Restore(std::uint32_t health,double feetY)
        {
            if(!health||health>MaximumHealth||!std::isfinite(feetY)||std::abs(feetY)>1e9)
                throw std::invalid_argument("Invalid restored vitals.");
            m_Health=health;m_Peak=feetY;
        }
        // Explicit death recovery; reset HP and peak at the spawn height.
        void Respawn(double feetY)
        {
            if(!std::isfinite(feetY))throw std::invalid_argument("Invalid respawn height.");
            m_Health=MaximumHealth;m_Peak=feetY;
        }
        // Called after character motion. Track apex while airborne, then
        // calculate ceil(drop-3) HP on genuine support only. Streaming guards
        // reset peak without damage; return actual HP removed for HUD feedback.
        std::uint32_t Observe(double feetY,bool grounded,bool physicalSupport)
        {
            if(!std::isfinite(feetY)||std::abs(feetY)>1e9)
                throw std::invalid_argument("Invalid survival height.");
            if(Dead())return 0;
            if(!m_Peak)m_Peak=feetY;
            *m_Peak=std::max(*m_Peak,feetY);
            if(!grounded)return 0;
            const double distance=*m_Peak-feetY;
            m_Peak=feetY;
            if(!physicalSupport)return 0;
            const auto damage=static_cast<std::uint32_t>(std::clamp(std::ceil(distance-3.),0.,double(m_Health)));
            m_Health-=damage;return damage;
        }
    };
}
