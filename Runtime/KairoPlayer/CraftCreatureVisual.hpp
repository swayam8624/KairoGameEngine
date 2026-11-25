#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace kairo::player::detail
{
    // Fixed-tick gait: actual collision-resolved displacement drives the
    // animation, so blocked actors cannot walk in place.
    struct CraftCreatureVisual
    {
        double Yaw=0,Phase=0,Stride=0,Attack=0;
        bool Hit=false;
        bool operator==(const CraftCreatureVisual&) const=default;
        void Observe(double dx,double dz) noexcept
        {
            const double travel=std::hypot(dx,dz);
            Stride=std::clamp(travel/.1,0.,1.);
            if(travel>1e-7)
            {
                Yaw=std::atan2(dx,dz);
                Phase=std::fmod(Phase+travel*(2*std::numbers::pi/.65),2*std::numbers::pi);
            }
        }
    };
    struct CraftRigBox
    {
        // Map a shared unit cube to Origin + Axes[0]*x + Axes[1]*y + Axes[2]*z.
        std::array<double,3> Origin;
        std::array<std::array<double,3>,3> Axes;
        std::array<float,3> Color;
    };
    inline std::array<CraftRigBox,7> BuildCraftCreatureRig(const CraftCreatureVisual& pose)
    {
        const double cy=std::cos(pose.Yaw),sy=std::sin(pose.Yaw);
        const auto yaw=[&](std::array<double,3> p)
        {return std::array<double,3>{cy*p[0]+sy*p[2],p[1],-sy*p[0]+cy*p[2]};};
        const auto box=[&](std::array<double,3> pivot,std::array<double,3> offset,
            std::array<double,3> size,double angle,std::array<float,3> color)
        {
            const double c=std::cos(angle),s=std::sin(angle);
            auto origin=yaw({pivot[0]+offset[0],pivot[1]+c*offset[1]-s*offset[2],
                pivot[2]+s*offset[1]+c*offset[2]});
            std::array<std::array<double,3>,3> axes{yaw({size[0],0,0}),
                yaw({0,c*size[1],s*size[1]}),yaw({0,-s*size[2],c*size[2]})};
            for(unsigned i=0;i<3;++i)for(const auto& a:axes)origin[i]-=.5*a[i];
            if(pose.Hit)color={1.f,.42f,.22f};
            return CraftRigBox{origin,axes,color};
        };
        const double swing=std::sin(pose.Phase)*pose.Stride*.32;
        // All pieces are bounded by the authoritative 0.72 x 1.2 collider.
        return {
            box({0,.68,0},{0,0,0},{.40,.42,.27},0,{.58f,.20f,.13f}),
            box({0,1.02,0},{0,0,0},{.34,.32,.30},0,{.76f,.34f,.20f}),
            box({0,1.07,.155},{0,0,0},{.24,.055,.025},0,{1.f,.72f,.18f}),
            box({-.115,.47,0},{0,-.20,0},{.17,.40,.18},swing,{.36f,.14f,.10f}),
            box({ .115,.47,0},{0,-.20,0},{.17,.40,.18},-swing,{.36f,.14f,.10f}),
            box({-.23,.86,0},{0,-.16,0},{.10,.34,.13},-swing-pose.Attack*.20,{.66f,.24f,.14f}),
            box({ .23,.86,0},{0,-.16,0},{.10,.34,.13},swing-pose.Attack*.20,{.66f,.24f,.14f})
        };
    }
}
