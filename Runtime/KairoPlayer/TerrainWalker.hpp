#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace kairo::player::detail
{
    // Minimal point-grounded exploration controller. This is deliberately
    // not a capsule, swept collider, rigid body or character motor.
    struct TerrainWalker
    {
        double X=16,Z=16,GroundY=0,Yaw=0,Pitch=-0.15;
    };
    struct TerrainWalkInput
    {
        double Forward=0,Right=0,YawDelta=0,PitchDelta=0;
        bool Sprint=false;
    };
    struct TerrainWalkConfig
    {
        double Speed=6,SprintMultiplier=2,MaximumGrade=1.5,EyeHeight=1.7;
    };
    template<class Ground>
    bool AdvanceTerrainWalker(TerrainWalker& state,const TerrainWalkInput& input,
        double dt,const TerrainWalkConfig& config,Ground&& ground)
    {
        for(double value:{state.X,state.Z,state.GroundY,state.Yaw,state.Pitch,
            input.Forward,input.Right,input.YawDelta,input.PitchDelta,dt,
            config.Speed,config.SprintMultiplier,config.MaximumGrade,config.EyeHeight})
            if(!std::isfinite(value))throw std::invalid_argument("Nonfinite terrain walker input.");
        if(dt<0||config.Speed<=0||config.Speed>100||config.SprintMultiplier<1||
            config.SprintMultiplier>4||config.MaximumGrade<0||config.EyeHeight<=0)
            throw std::invalid_argument("Invalid terrain walker configuration.");
        auto next=state;
        next.Yaw=std::remainder(state.Yaw+std::remainder(input.YawDelta,6.283185307179586),6.283185307179586);
        next.Pitch=std::clamp(state.Pitch+input.PitchDelta,-1.45,1.45);
        auto current=ground(state.X,state.Z);
        if(!current||!std::isfinite(*current))
        {state.Yaw=next.Yaw;state.Pitch=next.Pitch;return false;}
        next.GroundY=*current;
        const double forward=std::clamp(input.Forward,-1.0,1.0),right=std::clamp(input.Right,-1.0,1.0);
        const double length=std::max(1.0,std::hypot(forward,right));
        const double distance=config.Speed*(input.Sprint?config.SprintMultiplier:1.0)*std::min(dt,0.1);
        const double dx=(std::sin(next.Yaw)*forward+std::cos(next.Yaw)*right)*distance/length;
        const double dz=(-std::cos(next.Yaw)*forward+std::sin(next.Yaw)*right)*distance/length;
        const double total=std::hypot(dx,dz);
        const unsigned steps=std::max(1u,static_cast<unsigned>(std::ceil(total/0.25)));
        bool moved=false;
        for(unsigned i=0;i<steps&&total>0;++i)
        {
            const double x=next.X+dx/steps,z=next.Z+dz/steps;
            const auto h=ground(x,z);
            if(!h||!std::isfinite(*h)||std::abs(*h-next.GroundY)>config.MaximumGrade*total/steps)
                break;
            next.X=x;next.Z=z;next.GroundY=*h;moved=true;
        }
        state=next;
        return moved;
    }
}
