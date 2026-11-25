#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace kairo::player::detail
{
    struct CraftSaveStamp
    {
        std::uint64_t WorldRevision=0;
        std::array<std::uint32_t,4> Counts{};
        std::uint32_t Selected=0,Durability=0;
        bool Equipped=false;
        bool operator==(const CraftSaveStamp&) const=default;
    };
    // A bounded active-time schedule. Continuous edits cannot postpone a
    // checkpoint forever; unsuccessful writes preserve pending changes.
    class CraftAutosave
    {
        std::optional<CraftSaveStamp> m_Saved,m_Current;
        double m_Elapsed=0;
        double m_Delay=30;
    public:
        static constexpr double Interval=30.,RetryInterval=5.;
        void Initialize(CraftSaveStamp stamp) noexcept
        {m_Saved=stamp;m_Current=stamp;m_Elapsed=0;m_Delay=Interval;}
        [[nodiscard]] bool Dirty() const noexcept{return m_Current!=m_Saved;}
        void Observe(CraftSaveStamp stamp) noexcept
        {
            const bool wasDirty=Dirty();m_Current=stamp;
            if(!wasDirty||!Dirty()){m_Elapsed=0;m_Delay=Interval;}
        }
        [[nodiscard]] bool Advance(double dt)
        {
            if(!std::isfinite(dt)||dt<0)throw std::invalid_argument("Invalid autosave elapsed time.");
            if(!Dirty())return false;
            m_Elapsed=std::min(m_Delay,m_Elapsed+std::min(dt,.1));
            return m_Elapsed+1e-9>=m_Delay;
        }
        void Saved() noexcept{m_Saved=m_Current;m_Elapsed=0;m_Delay=Interval;}
        void Failed() noexcept{m_Elapsed=0;m_Delay=RetryInterval;}
    };
}
