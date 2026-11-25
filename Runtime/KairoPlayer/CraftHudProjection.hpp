#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace kairo::player::detail
{
    struct CraftHudViewport
    {
        std::uint32_t Width=960,Height=540;
    };
    struct CraftHudPixelRect
    {
        std::int32_t X,Y;
        std::uint32_t Width,Height;
    };
    // Quantize shared edges, rather than rounding sizes independently.
    // At tiny viewport sizes, collapsed rectangles are valid no-ops.
    inline CraftHudPixelRect QuantizeCraftHudRect(CraftHudViewport viewport,
        double x,double y,double w,double h,bool bottomAligned)
    {
        if(!viewport.Width||!viewport.Height||viewport.Width>32768||viewport.Height>32768||
            !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(w)||!std::isfinite(h)||
            x<0||y<0||w<=0||h<=0||x+w>960||y+h>540)
            throw std::invalid_argument("Invalid HUD pixel rectangle.");
        const double scale=std::min(viewport.Width/960.,viewport.Height/540.);
        const double ox=(viewport.Width-960*scale)*.5;
        const double oy=bottomAligned?viewport.Height-540*scale:(viewport.Height-540*scale)*.5;
        const auto x0=static_cast<std::int32_t>(std::lround(ox+x*scale));
        const auto y0=static_cast<std::int32_t>(std::lround(oy+y*scale));
        const auto x1=static_cast<std::int32_t>(std::lround(ox+(x+w)*scale));
        const auto y1=static_cast<std::int32_t>(std::lround(oy+(y+h)*scale));
        return {x0,y0,static_cast<std::uint32_t>(x1-x0),static_cast<std::uint32_t>(y1-y0)};
    }
    struct CraftHudPlaneRect
    {
        double X,Y,Width,Height;
    };
    // Keep the logical HUD uniform on portrait, wide and high-DPI targets.
    // Crosshair coordinates are centered; the hotbar is anchored to the bottom.
    // Return offsets and dimensions in the camera plane at the supplied depth.
    inline CraftHudPlaneRect ProjectCraftHudRect(CraftHudViewport viewport,
        double x,double y,double w,double h,double verticalFov,double depth,bool bottomAligned)
    {
        if(!viewport.Width||!viewport.Height||!std::isfinite(x)||!std::isfinite(y)||
            !std::isfinite(w)||!std::isfinite(h)||w<=0||h<=0||x<0||y<0||x+w>960||y+h>540||
            !std::isfinite(verticalFov)||verticalFov<=0||verticalFov>=3.141592653589793||
            !std::isfinite(depth)||depth<=0)
            throw std::invalid_argument("Invalid Craft HUD projection.");
        const double scale=std::min(viewport.Width/960.,viewport.Height/540.);
        const double pixel=2*depth*std::tan(verticalFov*.5)/viewport.Height;
        const double px=(x+w*.5-480)*scale;
        const double py=bottomAligned?(540-y-h*.5)*scale-viewport.Height*.5:
            (270-y-h*.5)*scale;
        return {px*pixel,py*pixel,w*scale*pixel,h*scale*pixel};
    }
}
