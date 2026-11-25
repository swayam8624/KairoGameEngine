#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>

namespace kairo::player::detail
{
    // Immutable per-cell profile, generated once on initial CPU loading.
    // Every supported power-of-two stride has an independent error. Do
    // not clamp to a single maximum stride: errors need not be monotone.
    struct TerrainErrorProfile final
    {
        std::array<std::uint64_t,11> ErrorMillimeters{};
        std::uint32_t MaximumStride=1;

        [[nodiscard]] std::uint64_t Error(std::uint32_t stride) const
        {
            if(stride==0||(stride&(stride-1u))!=0||stride>MaximumStride)
                throw std::invalid_argument("Unmeasured terrain stride.");
            std::size_t index=0;
            while(stride>1){stride/=2;++index;}
            return ErrorMillimeters.at(index);
        }

        [[nodiscard]] std::uint32_t Choose(std::uint32_t requested,
            std::uint64_t budget) const
        {
            // Reject invalid requested stride, including above profile.
            (void)Error(requested);
            for(auto stride=requested;stride>1;stride/=2)
                if(Error(stride)<=budget)return stride;
            return 1;
        }
    };

    template<class Measure>
    [[nodiscard]] TerrainErrorProfile BuildTerrainErrorProfile(
        std::uint32_t maximumStride,Measure&& measure)
    {
        if(maximumStride==0||maximumStride>1024||
            (maximumStride&(maximumStride-1u))!=0)
            throw std::invalid_argument("Invalid terrain error profile maximum stride.");
        TerrainErrorProfile profile;
        profile.MaximumStride=maximumStride;
        std::size_t index=1;
        for(std::uint32_t stride=2;stride<=maximumStride;stride*=2)
            profile.ErrorMillimeters[index++]=measure(stride);
        return profile;
    }
}
