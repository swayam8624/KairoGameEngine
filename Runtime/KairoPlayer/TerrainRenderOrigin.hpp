#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace kairo::player::detail
{
    // The streaming grid and terrain seed stay in absolute world space.
    // Only the renderer's coordinate system moves, snapped to a cell origin.
    // This helper deliberately does not change physics or authored scene state.
    // Observer coordinates and cellExtent are world-space meters. Return the
    // cell's absolute origin, not a local camera position; floor rather than
    // truncation keeps negative coordinates in the correct streaming cell.
    // TerrainDemo and CraftStreamDemo subtract this origin when drawing.
    [[nodiscard]] inline std::array<double,3> ChooseTerrainRenderOrigin(
        double x,double z,double cellExtent)
    {
        if(!std::isfinite(x)||!std::isfinite(z)||!std::isfinite(cellExtent)||
            cellExtent<0.001||cellExtent>1024000.0)
            throw std::invalid_argument("Invalid terrain render-origin coordinates or extent.");
        // Validate the quotient before narrowing to signed integer. Conversion
        // of an out-of-range floating value to an integer is not safe in C++.
        const auto cell=[&](double position)
        {
            const double coordinate=std::floor(position/cellExtent);
            if(coordinate<std::numeric_limits<std::int32_t>::min()||
                coordinate>std::numeric_limits<std::int32_t>::max())
                throw std::out_of_range("Terrain render origin exceeds signed cell coordinates.");
            return static_cast<std::int32_t>(coordinate);
        };
        return {cell(x)*cellExtent,0.0,cell(z)*cellExtent};
    }

    // Subtract in double precision BEFORE narrowing to the GPU scalar.
    // GPU positions are commonly float; subtraction in double preserves small
    // offsets even around billion-meter coordinates. This does not make distant
    // geometry accurate if the relative offset itself is very large.
    [[nodiscard]] inline float TerrainRelativeFloat(double world,double origin)
    {
        const double relative=world-origin;
        if(!std::isfinite(relative)||std::abs(relative)>std::numeric_limits<float>::max())
            throw std::overflow_error("Terrain render-relative coordinate exceeds float range.");
        return static_cast<float>(relative);
    }
}
