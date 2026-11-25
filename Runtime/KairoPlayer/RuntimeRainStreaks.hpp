#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace kairo::player::detail
{
    // Camera-local precipitation samples in absolute world coordinates.
    // The renderer reuses one unit cube for each streak; no mesh allocation
    // per drop. This visual model has no collision or water accumulation.
    using RainPoint=std::array<double,3>;
    [[nodiscard]] inline std::vector<RainPoint> RuntimeRainStreaks(std::uint64_t seed,
        unsigned intensity,std::uint64_t tick,RainPoint center)
    {
        if(intensity>16)throw std::invalid_argument("Invalid rain intensity.");
        for(double v:center)if(!std::isfinite(v)||std::abs(v)>1.1e9)
            throw std::invalid_argument("Invalid rain center.");
        std::vector<RainPoint> result;result.reserve(intensity*4);
        // Fixed integer avalanche yields stable spawn offsets. The animation
        // is a 16-tick sawtooth at 10 Hz; drops wrap from bottom to top.
        const auto hash=[](std::uint64_t x)
        {
            x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;
            x=(x^(x>>27))*0x94d049bb133111ebULL;return x^(x>>31);
        };
        for(unsigned i=0;i<intensity*4;++i)
        {
            const auto h=hash(seed+0x9e3779b97f4a7c15ULL*(i+1));
            const double x=double(h&65535)/65536.*8.-4.;
            const double z=double((h>>16)&65535)/65536.*8.-4.;
            const auto phase=((h>>32)+tick)%16;
            result.push_back({center[0]+x,center[1]+4.-double(phase)*.5,center[2]+z});
        }
        return result;
    }
}
