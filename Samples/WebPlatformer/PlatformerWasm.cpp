#include "RuntimePlatformerCore.hpp"

// This freestanding adapter exports a stable scalar C ABI. JavaScript owns
// keyboard/Canvas presentation, never modifies simulation memory directly.
// The fixed map is the exact native authored sample; all rules are shared.
namespace
{
    using namespace kairo::player::detail;
    PlatformerState state;
    char rows[6][16];
    unsigned coinCount=0;
    bool Blocked(int x,int y)
    {return PlatformerForTiles(x,y,16,6,[](int i,int j){return rows[j][i]=='#';});}
    bool Touches(char tile)
    {return PlatformerForTiles(state.X,state.Y,16,6,[&](int i,int j){return rows[j][i]==tile;});}
    unsigned Collect()
    {
        unsigned count=0;
        PlatformerForTiles(state.X,state.Y,16,6,[&](int i,int j){if(rows[j][i]=='C'){rows[j][i]='.';++count;}return false;});
        return count;
    }
    // Reset the mutable tile map and same spawn position as native.
    void Reset()
    {
        coinCount=0;
        for(int y=0;y<6;++y)for(int x=0;x<16;++x)
        {
            const char tile=PlatformerDemoRows[y][x];rows[y][x]=tile;
            if(tile=='S'){state.X=x*PlatformerTile+3;state.Y=y*PlatformerTile+2;}
            if(tile=='C')++coinCount;
        }
        state.VY=0;state.Coins=0;state.Deaths=0;
        state.JumpHeld=false;state.Won=false;state.Grounded=Blocked(state.X,state.Y+1);
    }
}
extern "C"
{
    void game_reset(){Reset();}
    int game_step(int axis,int jump)
    {
        if(axis<-1||axis>1||(jump!=0&&jump!=1))return 0;
        StepPlatformerCore(state,axis,jump!=0,coinCount,Blocked,Touches,Collect,Reset);return 1;
    }
    int game_tile(int x,int y){return x>=0&&x<16&&y>=0&&y<6?rows[y][x]:0;}
    int game_x(){return state.X;}
    int game_y(){return state.Y;}
    unsigned game_coins(){return state.Coins;}
    unsigned game_deaths(){return state.Deaths;}
    int game_won(){return state.Won;}
}
