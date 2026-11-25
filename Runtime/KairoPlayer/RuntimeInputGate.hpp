#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace kairo::player::detail
{
    // Held controls remain neutral after startup, focus loss or disconnect
    // until the player releases them. Once armed, held state is preserved.
    template<std::size_t Count>
    class RuntimeControlGate
    {
        std::array<bool,Count> m_Armed{};
    public:
        void Reset() noexcept { m_Armed.fill(false); }
        bool Digital(std::size_t index,bool down) noexcept
        {
            if(index>=Count)return false;
            if(!down)m_Armed[index]=true;
            return down&&m_Armed[index];
        }
        float Analog(std::size_t index,float value,float neutral=.15f) noexcept
        {
            if(index>=Count||!std::isfinite(value))return 0.f;
            value=std::clamp(value,-1.f,1.f);
            if(!m_Armed[index])
            {
                if(std::abs(value)<=neutral)m_Armed[index]=true;
                return 0.f;
            }
            return value;
        }
    };

    // GLFW reports sticks in [-1,1], but also reports released triggers as
    // -1. Engine action maps expect a neutral zero for all released controls.
    inline float NormalizeRuntimeGamepadAxis(float raw,bool trigger) noexcept
    {
        if(!std::isfinite(raw))return 0.f;
        raw=std::clamp(raw,-1.f,1.f);
        return trigger?(raw+1.f)*.5f:raw;
    }

    class RuntimeGamepadGate
    {
        RuntimeControlGate<32> m_Buttons;
        RuntimeControlGate<8> m_Axes;
    public:
        void Reset() noexcept {m_Buttons.Reset();m_Axes.Reset();}
        bool Button(std::size_t index,bool down) noexcept {return m_Buttons.Digital(index,down);}
        float Axis(std::size_t index,float raw,bool trigger) noexcept
        {
            // Invalid samples do not count as observed releases.
            if(!std::isfinite(raw))return 0.f;
            return m_Axes.Analog(index,NormalizeRuntimeGamepadAxis(raw,trigger));
        }
    };
}
