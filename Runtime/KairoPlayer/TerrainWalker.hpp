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
        double FootY=0,VerticalVelocity=0;
        bool Initialized=false,Grounded=false;
    };
    struct TerrainWalkInput
    {
        double Forward=0,Right=0,YawDelta=0,PitchDelta=0;
        bool Sprint=false,JumpPressed=false;
    };
    struct TerrainWalkConfig
    {
        double Speed=6,SprintMultiplier=2,MaximumGrade=1.5,EyeHeight=1.7;
        double Gravity=9.81,JumpSpeed=5.0;
    };
    template<class Ground>
    bool AdvanceTerrainWalker(TerrainWalker& state,const TerrainWalkInput& input,
        double dt,const TerrainWalkConfig& config,Ground&& ground)
    {
        for(double value:{state.X,state.Z,state.GroundY,state.Yaw,state.Pitch,state.FootY,state.VerticalVelocity,
            input.Forward,input.Right,input.YawDelta,input.PitchDelta,dt,
            config.Speed,config.SprintMultiplier,config.MaximumGrade,config.EyeHeight,config.Gravity,config.JumpSpeed})
            if(!std::isfinite(value))throw std::invalid_argument("Nonfinite terrain walker input.");
        if(dt<0||config.Speed<=0||config.Speed>100||config.SprintMultiplier<1||
            config.SprintMultiplier>4||config.MaximumGrade<0||config.EyeHeight<=0||config.Gravity<=0||config.Gravity>100||
            config.JumpSpeed<=0||config.JumpSpeed>30)
            throw std::invalid_argument("Invalid terrain walker configuration.");
        auto next=state;
        next.Yaw=std::remainder(state.Yaw+std::remainder(input.YawDelta,6.283185307179586),6.283185307179586);
        next.Pitch=std::clamp(state.Pitch+input.PitchDelta,-1.45,1.45);
        auto current=ground(state.X,state.Z);
        if(!current||!std::isfinite(*current))
        {state.Yaw=next.Yaw;state.Pitch=next.Pitch;return false;}
        next.GroundY=*current;
        if(!next.Initialized||next.Grounded)
        {next.FootY=*current;next.VerticalVelocity=0;next.Grounded=true;next.Initialized=true;}
        if(input.JumpPressed&&next.Grounded)
        {next.Grounded=false;next.VerticalVelocity=config.JumpSpeed;}
        const double forward=std::clamp(input.Forward,-1.0,1.0),right=std::clamp(input.Right,-1.0,1.0);
        const double length=std::max(1.0,std::hypot(forward,right));
        const double elapsed=std::min(dt,0.1);
        const double distance=config.Speed*(input.Sprint?config.SprintMultiplier:1.0)*elapsed;
        const double dx=(std::sin(next.Yaw)*forward+std::cos(next.Yaw)*right)*distance/length;
        const double dz=(-std::cos(next.Yaw)*forward+std::sin(next.Yaw)*right)*distance/length;
        const double total=std::hypot(dx,dz);
        // Bound both spatial and temporal substeps. Movement and gravity
        // advance even when horizontal motion is blocked.
        const unsigned steps=std::max({1u,static_cast<unsigned>(std::ceil(total/0.25)),
            static_cast<unsigned>(std::ceil(elapsed*120.0))});
        const double stepTime=elapsed/steps,stepDistance=total/steps;
        bool moved=false;
        for(unsigned i=0;i<steps;++i)
        {
            if(!next.Grounded)
            {
                next.FootY+=next.VerticalVelocity*stepTime-.5*config.Gravity*stepTime*stepTime;
                next.VerticalVelocity-=config.Gravity*stepTime;
                if(next.FootY<=next.GroundY&&next.VerticalVelocity<=0)
                {next.FootY=next.GroundY;next.VerticalVelocity=0;next.Grounded=true;}
            }
            if(stepDistance==0)continue;
            const double x=next.X+dx/steps,z=next.Z+dz/steps;
            const auto h=ground(x,z);
            if(!h||!std::isfinite(*h))continue;
            if(next.Grounded)
            {
                if(std::abs(*h-next.GroundY)>config.MaximumGrade*stepDistance+1e-9)continue;
                next.FootY=*h;
            }
            else if(*h>next.FootY)continue;
            next.X=x;next.Z=z;next.GroundY=*h;moved=true;
        }
        state=next;
        return moved;
    }
}
