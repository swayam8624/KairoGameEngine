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
    class RuntimeFrameClock
    {
        std::optional<double> m_Previous;
        bool m_WasActive=false;
    public:
        static constexpr double MaximumStep=.1;
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
