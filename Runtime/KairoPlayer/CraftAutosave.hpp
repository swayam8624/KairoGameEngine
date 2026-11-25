#pragma once
#include "CraftCheckpoint.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace kairo::player::detail
{
    // Value-only fingerprint of persistable Craft state. WorldRevision tracks
    // world edits; inventory, equipment and optional checkpoint cover changes
    // that may occur without terrain mutation. No GPU pointers belong here.
    struct CraftSaveStamp
    {
        std::uint64_t WorldRevision=0;
        std::array<std::uint32_t,4> Counts{};
        std::uint32_t Selected=0,Durability=0;
        bool Equipped=false;
        std::optional<CraftCheckpoint> Player={};
        bool operator==(const CraftSaveStamp&) const=default;
    };
    // A bounded active-time schedule. Continuous edits cannot postpone a
    // checkpoint forever; unsuccessful writes preserve pending changes.
    class CraftAutosave
    {
        // The scheduler remembers the last successful stamp and latest observed
        // stamp, not the saved bytes. The demo owns serialization and filesystem I/O.
        std::optional<CraftSaveStamp> m_Saved,m_Current;
        double m_Elapsed=0;
        double m_Delay=30;
    public:
        // Active simulation seconds: normal write interval and shorter retry.
        static constexpr double Interval=30.,RetryInterval=5.;
        // Seed current state. When no prior file exists, keep saved stamp empty
        // so the initial checkpoint remains dirty and is scheduled normally.
        void Initialize(CraftSaveStamp stamp,bool existing=true) noexcept
        {m_Saved=existing?std::optional<CraftSaveStamp>{stamp}:std::nullopt;m_Current=stamp;m_Elapsed=0;m_Delay=Interval;}
        // Compare full semantic stamps; a previously failed save never clears dirty.
        [[nodiscard]] bool Dirty() const noexcept{return m_Current!=m_Saved;}
        // Start the countdown on clean-to-dirty transition. Subsequent edits
        // do not reset it, so continuous editing cannot starve persistence.
        void Observe(CraftSaveStamp stamp) noexcept
        {
            const bool wasDirty=Dirty();m_Current=stamp;
            if(!wasDirty||!Dirty()){m_Elapsed=0;m_Delay=Interval;}
        }
        // Accumulate bounded active elapsed time, not wall-clock sleep time.
        // Return true when dirty and due; caller must call Saved or Failed
        // after attempting I/O. This function itself never writes a file.
        [[nodiscard]] bool Advance(double dt)
        {
            if(!std::isfinite(dt)||dt<0)throw std::invalid_argument("Invalid autosave elapsed time.");
            if(!Dirty())return false;
            m_Elapsed=std::min(m_Delay,m_Elapsed+std::min(dt,.1));
            return m_Elapsed+1e-9>=m_Delay;
        }
        // Acknowledge the currently observed state only after successful I/O.
        void Saved() noexcept{m_Saved=m_Current;m_Elapsed=0;m_Delay=Interval;}
        // Preserve the last successful stamp and retry after a shorter interval.
        void Failed() noexcept{m_Elapsed=0;m_Delay=RetryInterval;}
    };
}
