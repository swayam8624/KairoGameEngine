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
        std::uint32_t m_Health=20;
        std::optional<double> m_Peak;
    public:
        static constexpr std::uint32_t MaximumHealth=20;
        [[nodiscard]] std::uint32_t Health() const noexcept{return m_Health;}
        [[nodiscard]] bool Dead() const noexcept{return m_Health==0;}
        void Damage(std::uint32_t points) noexcept{m_Health-=std::min(points,m_Health);}
        void Heal(std::uint32_t points) noexcept{if(!Dead())m_Health+=std::min(points,MaximumHealth-m_Health);}
        void Restore(std::uint32_t health,double feetY)
        {
            if(!health||health>MaximumHealth||!std::isfinite(feetY)||std::abs(feetY)>1e9)
                throw std::invalid_argument("Invalid restored vitals.");
            m_Health=health;m_Peak=feetY;
        }
        void Respawn(double feetY)
        {
            if(!std::isfinite(feetY))throw std::invalid_argument("Invalid respawn height.");
            m_Health=MaximumHealth;m_Peak=feetY;
        }
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
