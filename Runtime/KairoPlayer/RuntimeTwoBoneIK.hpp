#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace kairo::player::detail
{
    // All points share one Cartesian coordinate space. The solver is a
    // pure geometric kernel; skinning adapters must convert solved positions
    // into local joint rotations before publishing a glTF animation pose.
    using IKPoint=std::array<double,3>;
    namespace ik_detail
    {
        // Small vector helpers deliberately avoid dependencies on GPU or ECS.
        inline IKPoint Add(IKPoint a,IKPoint b){return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
        inline IKPoint Sub(IKPoint a,IKPoint b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
        inline IKPoint Scale(IKPoint a,double s){return {a[0]*s,a[1]*s,a[2]*s};}
        inline double Dot(IKPoint a,IKPoint b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
        inline double Length(IKPoint a){return std::hypot(a[0],a[1],a[2]);}
        inline void Validate(IKPoint a)
        {
            for(double x:a)if(!std::isfinite(x)||std::abs(x)>1e9)
                throw std::invalid_argument("IK point must be finite and bounded.");
        }
        // Choose the coordinate axis least parallel to d, then remove its
        // projection. This deterministic fallback resolves ambiguous poles.
        inline IKPoint Perpendicular(IKPoint d)
        {
            unsigned axis=0;
            for(unsigned i=1;i<3;++i)if(std::abs(d[i])<std::abs(d[axis]))axis=i;
            IKPoint basis{};basis[axis]=1.;
            const auto v=Sub(basis,Scale(d,Dot(basis,d)));
            return Scale(v,1./Length(v));
        }
    }
    struct TwoBoneIKResult
    {
        IKPoint Joint,Tip; // Same world/asset space as the root and target.
        bool Reached;     // False when target lies outside the reachable shell.
    };
    // Analytical two-bone IK using the triangle law of cosines. Bone lengths
    // never change. The pole is a position selecting the bend plane, not an
    // angle. Clamp unreachable targets to the inner/outer spherical shell.
    // Equal-length chains can fold completely at the root; choose the pole
    // direction for the joint. Collinear poles use the deterministic fallback.
    // Positive lengths [1e-6,1e6] bound arithmetic and conditioning.
    [[nodiscard]] inline TwoBoneIKResult SolveTwoBoneIK(IKPoint root,IKPoint target,
        IKPoint pole,double upper,double lower)
    {
        using namespace ik_detail;
        Validate(root);Validate(target);Validate(pole);
        if(!std::isfinite(upper)||!std::isfinite(lower)||upper<1e-6||lower<1e-6||upper>1e6||lower>1e6)
            throw std::invalid_argument("Invalid IK bone lengths.");
        const auto offset=Sub(target,root),poleOffset=Sub(pole,root);
        const double distance=Length(offset),poleLength=Length(poleOffset);
        const double inner=std::abs(upper-lower),outer=upper+lower;
        const double reach=std::clamp(distance,inner,outer);
        const bool reached=distance>=inner&&distance<=outer;
        const IKPoint direction=distance>0?Scale(offset,1./distance):IKPoint{1,0,0};
        if(reach==0)
        {
            const auto bend=poleLength>0?Scale(poleOffset,1./poleLength):IKPoint{0,1,0};
            return {Add(root,Scale(bend,upper)),root,reached};
        }
        auto bend=Sub(poleOffset,Scale(direction,Dot(poleOffset,direction)));
        const double bendLength=Length(bend);
        bend=bendLength>1e-12*std::max(1.,poleLength)?Scale(bend,1./bendLength):Perpendicular(direction);
        // Factor the difference of squares to avoid cancellation when the
        // two lengths are nearly equal. Clamp rounding at shell boundaries.
        const double along=std::clamp((reach+(upper-lower)*(upper+lower)/reach)*.5,-upper,upper);
        const double height=std::sqrt(std::max(0.,(upper-along)*(upper+along)));
        return {Add(root,Add(Scale(direction,along),Scale(bend,height))),
            Add(root,Scale(direction,reach)),reached};
    }
}
