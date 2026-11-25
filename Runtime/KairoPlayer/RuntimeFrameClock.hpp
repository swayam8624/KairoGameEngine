#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace kairo::player::detail
{
    // A monotonic wall-clock sample is not necessarily simulation time.
    // Never replay an inactive interval, and cap active stalls to 100 ms.
    // Call once per polled frame, including frames that will not draw.
    // Owned once per native frame loop in main.cpp and the Craft demos.
    // RuntimeFrameClockTests exercise monotonicity, minimized intervals and
    // the cap. Physics still chooses its own fixed-step subdivisions.
    class RuntimeFrameClock
    {
        std::optional<double> m_Previous;
        bool m_WasActive=false;
    public:
        // Seconds: discard excess wall time rather than simulating a long
        // backlog after an OS stall. This is not a fixed physics timestep.
        static constexpr double MaximumStep=.1;
        // Input is a monotonic elapsed timestamp, not a frame delta. Commit
        // the timestamp even while inactive to prevent catch-up on resume.
        // Invalid timestamps throw before either state member changes.
        double Sample(double seconds,bool active)
        {
            if(!std::isfinite(seconds)||seconds<0||
                (m_Previous&&seconds<*m_Previous))
                throw std::invalid_argument("Invalid monotonic frame clock sample.");
            const double elapsed=active&&m_WasActive&&m_Previous?
                std::min(MaximumStep,seconds-*m_Previous):0.;
            m_Previous=seconds;m_WasActive=active;
            return elapsed;
        }
    };
}
