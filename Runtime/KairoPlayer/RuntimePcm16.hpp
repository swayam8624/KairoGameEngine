#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace kairo::player::detail
{
    // PCM16 has one extra negative level. Preserve both exact endpoints,
    // round to nearest, clip overload and silence invalid float samples.
    // Convert one normalized linear audio sample to signed 16-bit PCM.
    // Multiplying the negative side by 32768 and the positive side by 32767
    // respects the asymmetric two's-complement range. No allocation or I/O.
    inline std::int16_t RuntimeFloatToPcm16(float sample) noexcept
    {
        if(!std::isfinite(sample))return 0;
        const double value=std::clamp(static_cast<double>(sample),-1.,1.);
        return static_cast<std::int16_t>(std::lround(value*(value<0?32768.:32767.)));
    }
    // Device adapters supply equally sized non-owning spans; the input remains
    // unchanged and the caller owns output memory. Conversion is O(samples).
    // Validate size before writing so an invalid request cannot partially publish.
    inline void ConvertRuntimePcm16(std::span<const float> input,std::span<std::int16_t> output)
    {
        if(input.size()!=output.size())throw std::invalid_argument("PCM16 conversion size mismatch.");
        for(std::size_t i=0;i<input.size();++i)output[i]=RuntimeFloatToPcm16(input[i]);
    }
}
