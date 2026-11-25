#pragma once
#include "VoxelInteraction.hpp"

namespace kairo::player::detail
{
    // Upright axis-aligned player box, centered horizontally at the feet.
    // Contacts use half-open voxel cells and strict volume overlap.
    struct VoxelPlayer
    {
        std::array<double,3> Feet{0.5,8,0.5};
        double VerticalVelocity=0;
        bool Grounded=false;
    };
    struct VoxelPlayerConfig
    {
        double Radius=.30,Height=1.80,EyeHeight=1.62;
        double Speed=4.5,SprintMultiplier=1.5,Gravity=9.81,JumpSpeed=5.0;
    };
    struct VoxelPlayerInput
    {
        double X=0,Z=0;
        bool JumpPressed=false,Sprint=false;
    };
    inline void ValidateVoxelPlayer(const VoxelPlayer& player,const VoxelPlayerConfig& c)
    {
        for(auto v:player.Feet)if(!std::isfinite(v)||std::abs(v)>1e9)
            throw std::invalid_argument("Voxel player position out of range.");
        for(double v:{player.VerticalVelocity,c.Radius,c.Height,c.EyeHeight,c.Speed,
            c.SprintMultiplier,c.Gravity,c.JumpSpeed})
            if(!std::isfinite(v))throw std::invalid_argument("Nonfinite voxel player configuration.");
        if(std::abs(player.VerticalVelocity)>100||c.Radius<=0||c.Radius>.49||
            c.Height<=0||c.Height>4||c.EyeHeight<=0||c.EyeHeight>=c.Height||
            c.Speed<=0||c.Speed>30||c.SprintMultiplier<1||c.SprintMultiplier>3||
            c.Gravity<=0||c.Gravity>100||c.JumpSpeed<=0||c.JumpSpeed>30)
            throw std::invalid_argument("Invalid voxel player dimensions or motion.");
    }
    inline std::array<double,3> PlayerMinimum(const VoxelPlayer& p,const VoxelPlayerConfig& c)
    {return {p.Feet[0]-c.Radius,p.Feet[1],p.Feet[2]-c.Radius};}
    inline std::array<double,3> PlayerMaximum(const VoxelPlayer& p,const VoxelPlayerConfig& c)
    {return {p.Feet[0]+c.Radius,p.Feet[1]+c.Height,p.Feet[2]+c.Radius};}
    inline bool PlayerIntersectsBlock(const VoxelPlayer& p,const VoxelPlayerConfig& c,VoxelCoordinate cell)
    {
        const auto lo=PlayerMinimum(p,c),hi=PlayerMaximum(p,c);
        const std::array<double,3> b{double(cell.X),double(cell.Y),double(cell.Z)};
        for(unsigned i=0;i<3;++i)if(hi[i]<=b[i]||lo[i]>=b[i]+1)return false;
        return true;
    }
    template<class Query>
    bool VoxelPlayerBlocked(const VoxelPlayer& p,const VoxelPlayerConfig& c,Query&& query)
    {
        const auto lo=PlayerMinimum(p,c),hi=PlayerMaximum(p,c);
        std::array<std::int32_t,3> first{},last{};
        for(unsigned i=0;i<3;++i)
        {
            first[i]=static_cast<std::int32_t>(std::floor(lo[i]));
            last[i]=static_cast<std::int32_t>(std::ceil(hi[i])-1);
        }
        for(auto z=first[2];z<=last[2];++z)
            for(auto y=first[1];y<=last[1];++y)
                for(auto x=first[0];x<=last[0];++x)
                {
                    const std::optional<VoxelMaterial> value=query({x,y,z});
                    if(!value||*value!=VoxelAir)return true;
                }
        return false;
    }
    // Each individual axis movement is <= 1/8 voxel. A solid unit voxel
    // cannot be entered and exited in that interval. On contact, bisect
    // the admissible segment, retaining a nonpenetrating endpoint.
    template<class Query>
    bool MoveVoxelPlayerAxis(VoxelPlayer& p,const VoxelPlayerConfig& c,
        unsigned axis,double delta,Query&& query)
    {
        if(axis>=3||!std::isfinite(delta)||std::abs(delta)>.1250000001)
            throw std::invalid_argument("Voxel axis step exceeds contract.");
        if(delta==0)return false;
        auto next=p;next.Feet[axis]+=delta;
        if(!VoxelPlayerBlocked(next,c,query)){p=next;return false;}
        double safe=0,blocked=1;
        for(unsigned i=0;i<32;++i)
        {
            const double mid=(safe+blocked)*.5;
            next=p;next.Feet[axis]+=delta*mid;
            if(VoxelPlayerBlocked(next,c,query))blocked=mid;else safe=mid;
        }
        p.Feet[axis]+=delta*safe;
        return true;
    }

    template<class Query>
    void AdvanceVoxelPlayer(VoxelPlayer& player,const VoxelPlayerConfig& c,
        VoxelPlayerInput input,double dt,Query&& query)
    {
        ValidateVoxelPlayer(player,c);
        if(!std::isfinite(dt)||dt<0||!std::isfinite(input.X)||!std::isfinite(input.Z))
            throw std::invalid_argument("Invalid voxel movement input.");
        if(VoxelPlayerBlocked(player,c,query))throw std::logic_error("Player begins inside blocked voxel.");
        auto p=player;
        auto below=p;below.Feet[1]-=.000001;
        p.Grounded=VoxelPlayerBlocked(below,c,query)&&p.VerticalVelocity<=0;
        if(p.Grounded){p.VerticalVelocity=0;if(input.JumpPressed){p.VerticalVelocity=c.JumpSpeed;p.Grounded=false;}}
        const double elapsed=std::min(dt,.1);
        input.X=std::clamp(input.X,-1.,1.);input.Z=std::clamp(input.Z,-1.,1.);
        const double scale=c.Speed*(input.Sprint?c.SprintMultiplier:1.)/
            std::max(1.,std::hypot(input.X,input.Z));
        const double maxSpeed=std::max({std::abs(input.X*scale),std::abs(input.Z*scale),
            std::abs(p.VerticalVelocity)+c.Gravity*elapsed});
        const unsigned steps=std::max({1u,static_cast<unsigned>(std::ceil(elapsed*120)),
            static_cast<unsigned>(std::ceil(elapsed*maxSpeed/.125))});
        const double h=elapsed/steps;
        for(unsigned i=0;i<steps;++i)
        {
            (void)MoveVoxelPlayerAxis(p,c,0,input.X*scale*h,query);
            (void)MoveVoxelPlayerAxis(p,c,2,input.Z*scale*h,query);
            const double dy=p.VerticalVelocity*h-.5*c.Gravity*h*h;
            p.VerticalVelocity=std::max(-100.,p.VerticalVelocity-c.Gravity*h);
            if(MoveVoxelPlayerAxis(p,c,1,dy,query))p.VerticalVelocity=0;
            below=p;below.Feet[1]-=.000001;
            p.Grounded=p.VerticalVelocity<=0&&VoxelPlayerBlocked(below,c,query);
            if(p.Grounded)p.VerticalVelocity=0;
        }
        ValidateVoxelPlayer(p,c);
        player=p;
    }
}
