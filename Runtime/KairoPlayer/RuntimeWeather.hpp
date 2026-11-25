#pragma once
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <stdexcept>

namespace kairo::player::detail
{
    // Quantized weather parameters are renderer-neutral: cloud and rain are
    // integer fractions of sixteen. One key can be compared before resubmitting
    // a scene, without rebuilding its meshes or depending on GPU timing.
    struct RuntimeWeatherSample
    {
        unsigned Cloud=0,Rain=0;
        friend bool operator==(const RuntimeWeatherSample&,const RuntimeWeatherSample&)=default;
    };
    // Stateless, seeded weather timeline. Each 30-second epoch transitions
    // from the preceding target over its first eight seconds. The repeating
    // clear/overcast/rain/clear pattern guarantees a short playable demo;
    // seed changes intensity, not the sequence. No global RNG state is owned.
    class RuntimeWeather
    {
    public:
        static constexpr std::uint64_t EpochSeconds=30,TransitionSeconds=8;
        [[nodiscard]] static RuntimeWeatherSample At(std::uint64_t seed,std::uint64_t seconds) noexcept
        {
            const auto epoch=seconds/EpochSeconds,local=seconds%EpochSeconds;
            const auto before=epoch?Target(seed,epoch-1):RuntimeWeatherSample{};
            const auto after=Target(seed,epoch);
            const auto t=static_cast<unsigned>(std::min(local,TransitionSeconds));
            const auto blend=[t](unsigned a,unsigned b){return (a*(8-t)+b*t+4)/8;};
            return {blend(before.Cloud,after.Cloud),blend(before.Rain,after.Rain)};
        }
        // Apply the weather modulation to a daylight sample by value. The
        // sample contract matches CraftDaylightSample: linear RGB sky/ambient
        // plus sun and ambient intensity. This changes actual scene lighting;
        // Rain is a simulation parameter, not a claim of visible precipitation.
        template<class Lighting>
        [[nodiscard]] static Lighting Apply(Lighting light,RuntimeWeatherSample weather)
        {
            if(weather.Cloud>16||weather.Rain>weather.Cloud)throw std::invalid_argument("Invalid weather sample.");
            const float cloud=float(weather.Cloud)/16.f,rain=float(weather.Rain)/16.f;
            const float grey=(light.Sky[0]+light.Sky[1]+light.Sky[2])/3.f;
            for(auto& v:light.Sky)v=(v+(grey-v)*cloud*.8f)*(1-.35f*rain);
            light.SunIntensity*=1-.85f*cloud;
            light.AmbientIntensity*=1-.25f*rain;
            return light;
        }
    private:
        // SplitMix-style integer avalanche: reproducible across platforms,
        // including unsigned overflow, without library RNG distribution rules.
        [[nodiscard]] static RuntimeWeatherSample Target(std::uint64_t seed,std::uint64_t epoch) noexcept
        {
            auto x=seed+epoch*0x9e3779b97f4a7c15ULL;
            x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;
            x=(x^(x>>27))*0x94d049bb133111ebULL;x^=x>>31;
            switch(epoch%4)
            {
                case 1:return {10u+unsigned(x%5),0};
                case 2:return {16,8u+unsigned(x%9)};
                default:return {};
            }
        }
    };
}
