#pragma once

namespace kairo::player::detail
{
    // Freestanding shared game rules: no standard-library headers, heap,
    // exceptions or OS services. Both the native C++ wrapper and WASM host
    // use this exact state transition. The caller validates map/input bounds.
    inline constexpr int PlatformerTile=16,PlatformerWidth=10,PlatformerHeight=14;
    struct PlatformerState
    {
        int X=0,Y=0,VY=0;unsigned Coins=0,Deaths=0;
        bool Grounded=false,JumpHeld=false,Won=false;
        friend bool operator==(const PlatformerState&,const PlatformerState&)=default;
    };
    inline constexpr char PlatformerDemoRows[6][17]={
        "################","#..............#","#..............#",
        "#..............#","#S.C..H..C...G.#","################"};

    // Half-open collision rectangle in integer subcells. Outside the validated
    // grid is treated as occupied. Visit each overlapped tile once, and stop
    // when the predicate reports a hit. Native/WASM share the boundary rule.
    template<class Predicate>
    bool PlatformerForTiles(int x,int y,int columns,int rows,Predicate predicate)
    {
        if(x<0||y<0||x+PlatformerWidth>columns*PlatformerTile||y+PlatformerHeight>rows*PlatformerTile)return true;
        for(int j=y/PlatformerTile;j<=(y+PlatformerHeight-1)/PlatformerTile;++j)
            for(int i=x/PlatformerTile;i<=(x+PlatformerWidth-1)/PlatformerTile;++i)
                if(predicate(i,j))return true;
        return false;
    }

    // One 60 Hz fixed tick. Blocked tests candidate positions, Touches
    // tests current overlap, Collect returns newly consumed coin count,
    // Reset restores the authored map and spawn through the host adapter.
    // All callbacks run synchronously and must refer to the same live state.
    template<class Blocked,class Touches,class Collect,class Reset>
    void StepPlatformerCore(PlatformerState& state,int axis,bool jump,unsigned coinCount,
        Blocked blocked,Touches touches,Collect collect,Reset reset)
    {
        if(state.Won)return;
        if(jump&&!state.JumpHeld&&state.Grounded)state.VY=-10;
        state.JumpHeld=jump;
        state.VY=state.VY<10?state.VY+1:10;
        // Sweep each axis in unit subcells; velocity never tunnels a solid
        // cell. Cancel vertical velocity on ceiling/floor collision only.
        const auto sweep=[&](int amount,bool horizontal)
        {
            const int sign=amount<0?-1:1,count=amount<0?-amount:amount;
            for(int i=0;i<count;++i)
            {
                const int x=state.X+(horizontal?sign:0),y=state.Y+(horizontal?0:sign);
                if(blocked(x,y)){if(!horizontal)state.VY=0;break;}
                state.X=x;state.Y=y;
            }
        };
        sweep(axis*2,true);sweep(state.VY,false);
        state.Grounded=blocked(state.X,state.Y+1);
        // Hazard has priority over coin and goal. Saturate the death count;
        // preserve held jump on respawn so a stale press cannot auto-jump.
        if(touches('H'))
        {
            const auto deaths=state.Deaths;reset();
            state.Deaths=deaths==~0u?deaths:deaths+1;
            state.JumpHeld=jump;return;
        }
        state.Coins+=collect();
        if(state.Coins==coinCount&&touches('G'))state.Won=true;
    }
}
