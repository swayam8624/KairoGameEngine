#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace kairo::player::detail
{
    using ScreenVec=std::array<double,3>;
    struct TerrainPerspectiveView
    {
        ScreenVec Position{},Forward{0,0,-1},Up{0,1,0};
        double VerticalFovRadians=1.0471975511965976;
        double ViewportHeight=540;
        double NearPlane=0.1;
    };
    struct PreparedTerrainView
    {
        ScreenVec Position,Forward,Right,Up;
        double FocalPixels,NearPlane;
    };
    inline double ScreenDot(const ScreenVec& a,const ScreenVec& b)
    {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
    inline ScreenVec ScreenCross(const ScreenVec& a,const ScreenVec& b)
    {return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
    inline ScreenVec ScreenUnit(ScreenVec a)
    {
        const auto len=std::hypot(a[0],a[1],a[2]);
        if(!std::isfinite(len)||len<=1e-12)throw std::invalid_argument("Invalid terrain camera basis.");
        for(auto& v:a)v/=len;
        return a;
    }
    inline PreparedTerrainView PrepareTerrainView(const TerrainPerspectiveView& view)
    {
        for(double v:view.Position)if(!std::isfinite(v))throw std::invalid_argument("Invalid terrain camera position.");
        if(!std::isfinite(view.VerticalFovRadians)||view.VerticalFovRadians<=0||
            view.VerticalFovRadians>=3.141592653589793||
            !std::isfinite(view.ViewportHeight)||view.ViewportHeight<=0||
            !std::isfinite(view.NearPlane)||view.NearPlane<=0)
            throw std::invalid_argument("Invalid terrain perspective projection.");
        const auto forward=ScreenUnit(view.Forward);
        const auto right=ScreenUnit(ScreenCross(forward,ScreenUnit(view.Up)));
        const auto up=ScreenUnit(ScreenCross(right,forward));
        const double focal=view.ViewportHeight/(2*std::tan(view.VerticalFovRadians/2));
        if(!std::isfinite(focal))throw std::invalid_argument("Invalid terrain focal length.");
        return {view.Position,forward,right,up,focal,view.NearPlane};
    }

    // Given a nominal vertical discrepancy E, bound the Euclidean distance
    // between two projected points at identical world X/Z. Both heights
    // lie inside the supplied cell AABB. Near-plane/behind-camera cells
    // fail closed to infinity (full detail), rather than dividing by zero.
    // This is a double-precision analytical estimate, not a certified
    // floating-point, rasterization or visibility bound.
    inline double TerrainProjectedErrorPixels(const PreparedTerrainView& view,
        const ScreenVec& minimum,const ScreenVec& maximum,double errorMeters)
    {
        if(!std::isfinite(errorMeters)||errorMeters<0)
            throw std::invalid_argument("Invalid terrain vertical error.");
        for(unsigned i=0;i<3;++i)
            if(!std::isfinite(minimum[i])||!std::isfinite(maximum[i])||minimum[i]>maximum[i])
                throw std::invalid_argument("Invalid terrain projection bounds.");
        if(errorMeters==0)return 0;
        double depth=std::numeric_limits<double>::infinity(),radial=0;
        for(unsigned i=0;i<8;++i)
        {
            ScreenVec p{};
            for(unsigned j=0;j<3;++j)p[j]=((i&(1u<<j))?maximum[j]:minimum[j])-view.Position[j];
            depth=std::min(depth,ScreenDot(p,view.Forward));
            radial=std::max(radial,std::hypot(ScreenDot(p,view.Right),ScreenDot(p,view.Up)));
        }
        if(!std::isfinite(depth)||!std::isfinite(radial)||depth<=view.NearPlane)
            return std::numeric_limits<double>::infinity();
        // Projection difference identity: q'/d' - q/d =
        // delta_q/d' - q*delta_d/(d*d'). Bound both depths by depth.
        const double bound=view.FocalPixels*(errorMeters/depth)*
            (std::hypot(view.Right[1],view.Up[1])+radial/depth*std::abs(view.Forward[1]));
        return std::isfinite(bound)?bound:std::numeric_limits<double>::infinity();
    }
}
