#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace kairo::player::detail
{
    struct CraftMiningTarget
    {
        std::array<std::int32_t,3> Cell{};
        std::uint16_t Material=0;
        bool operator==(const CraftMiningTarget&) const=default;
    };
    // Material IDs match CraftWorld: grass, dirt, stone, building block.
    // Time is seconds of uninterrupted hold, independent of frame count.
    inline double CraftMiningSeconds(std::uint16_t material)
    {
        constexpr std::array<double,4> seconds{.30,.45,1.50,.90};
        if(material<1||material>seconds.size())throw std::invalid_argument("Invalid mining material.");
        return seconds[material-1];
    }
    class CraftMining
    {
        std::optional<CraftMiningTarget> m_Target;
        double m_Elapsed=0;
    public:
        void Reset() noexcept {m_Target.reset();m_Elapsed=0;}
        [[nodiscard]] unsigned Progress() const
        {
            if(!m_Target)return 0;
            return static_cast<unsigned>(std::clamp(std::floor(20*m_Elapsed/CraftMiningSeconds(m_Target->Material)),0.,20.));
        }
        // Commit owns the inventory/world transaction. A refused edit does
        // not grant anything, and another attempt requires a full hold.
        // No excess time is carried across blocks or failed transactions.
        template<class Commit>
        bool Advance(bool held,std::optional<CraftMiningTarget> target,double dt,Commit&& commit)
        {
            if(!std::isfinite(dt)||dt<0)throw std::invalid_argument("Invalid mining elapsed time.");
            if(!held||!target){Reset();return false;}
            const double required=CraftMiningSeconds(target->Material);
            if(m_Target!=target){m_Target=target;m_Elapsed=0;}
            m_Elapsed=std::min(required,m_Elapsed+std::min(dt,.1));
            if(m_Elapsed+1e-9<required)return false;
            const auto completed=*m_Target;
            Reset();
            return commit(completed);
        }
    };
}
