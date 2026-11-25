#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace kairo::player::detail
{
    struct CraftLookDelta { double Yaw=0,Pitch=0; };
    class CraftMouseLook
    {
        std::optional<std::array<double,2>> m_Previous;
    public:
        static constexpr double Sensitivity=.0025;
        void Reset() noexcept {m_Previous.reset();}
        // First sample after focus/capture change is a baseline, not motion.
        // Bound anomalous cursor warps and discard nonfinite platform data.
        CraftLookDelta Observe(bool active,double x,double y) noexcept
        {
            if(!active||!std::isfinite(x)||!std::isfinite(y)){Reset();return {};}
            const std::array next{x,y};
            if(!m_Previous){m_Previous=next;return {};}
            const auto old=*m_Previous;m_Previous=next;
            const auto dx=std::clamp(x-old[0],-400.,400.);
            const auto dy=std::clamp(y-old[1],-400.,400.);
            return {dx*Sensitivity,-dy*Sensitivity};
        }
    };
    // Mouse deltas are already per sample; only keyboard angular velocity
    // is multiplied by elapsed time. Keep a valid camera basis at the poles.
    inline void ApplyCraftLook(double& yaw,double& pitch,CraftLookDelta mouse,
        double keyboardYaw,double keyboardPitch,double dt)
    {
        if(!std::isfinite(yaw)||!std::isfinite(pitch)||!std::isfinite(mouse.Yaw)||
            !std::isfinite(mouse.Pitch)||!std::isfinite(keyboardYaw)||!std::isfinite(keyboardPitch)||
            !std::isfinite(dt)||dt<0)throw std::invalid_argument("Invalid Craft camera input.");
        const double elapsed=std::min(dt,.1);
        yaw=std::remainder(yaw+mouse.Yaw+std::clamp(keyboardYaw,-1.,1.)*1.8*elapsed,6.283185307179586);
        pitch=std::clamp(pitch+mouse.Pitch+std::clamp(keyboardPitch,-1.,1.)*1.4*elapsed,-1.45,1.45);
    }
}
