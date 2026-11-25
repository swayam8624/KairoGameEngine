#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace kairo::player::detail
{
    struct CraftDaylightSample
    {
        std::array<float,3> Sky{},Ambient{},SunDirection{},SunColor{};
        float AmbientIntensity=0,SunIntensity=0;
    };
    // A ten-minute cycle, sunrise at phase zero and noon at .25.
    // Rendering samples are quantized independently of the simulation clock.
    class CraftDaylight
    {
        double m_Phase=.25;
    public:
        static constexpr unsigned Steps=240;
        static constexpr double PeriodSeconds=600.;
        explicit CraftDaylight(double phase=.25):m_Phase(phase)
        {
            if(!std::isfinite(phase)||phase<0||phase>=1)throw std::invalid_argument("Invalid daylight phase.");
        }
        void Advance(double dt)
        {
            if(!std::isfinite(dt)||dt<0)throw std::invalid_argument("Invalid daylight elapsed time.");
            m_Phase+=std::min(dt,.1)/PeriodSeconds;
            if(m_Phase>=1)m_Phase-=1;
        }
        [[nodiscard]] unsigned Step() const noexcept
        {return std::min(Steps-1,static_cast<unsigned>(std::floor(m_Phase*Steps)));}
        [[nodiscard]] double Phase() const noexcept{return m_Phase;}
        [[nodiscard]] static CraftDaylightSample Sample(unsigned step)
        {
            if(step>=Steps)throw std::invalid_argument("Invalid daylight step.");
            constexpr double tau=6.2831853071795864769;
            const double angle=tau*double(step)/Steps;
            const double elevation=std::sin(angle);
            const auto smooth=[](double x){x=std::clamp(x,0.,1.);return x*x*(3-2*x);};
            const double daylight=smooth((elevation+.12)/.42);
            const double warmth=(1-smooth(std::abs(elevation)/.45))*daylight;
            const auto mix=[](std::array<float,3> a,std::array<float,3> b,double t)
            {
                for(unsigned i=0;i<3;++i)a[i]=static_cast<float>(a[i]+(b[i]-a[i])*t);
                return a;
            };
            CraftDaylightSample result;
            result.Sky=mix(mix({.012f,.019f,.055f},{.34f,.61f,.90f},daylight),{.85f,.32f,.12f},warmth*.65);
            result.Ambient=mix({.35f,.48f,.8f},{1.f,.97f,.88f},daylight);
            result.SunColor=mix({1.f,.96f,.86f},{1.f,.42f,.16f},warmth);
            result.AmbientIntensity=static_cast<float>(.10+.25*daylight);
            result.SunIntensity=static_cast<float>(3*smooth(elevation/.4));
            // Direction points toward the light, as in the existing Craft sun.
            const double norm=std::sqrt(1+.25*.25);
            result.SunDirection={static_cast<float>(std::cos(angle)/norm),static_cast<float>(elevation/norm),static_cast<float>(.25/norm)};
            return result;
        }
    };
}
