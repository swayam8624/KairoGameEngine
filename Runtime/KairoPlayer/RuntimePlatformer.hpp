#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <utility>
#include <stdexcept>
#include <string>
#include <vector>

namespace kairo::player::detail
{
    // Grid coordinates increase right and down. Positions are integer subcells
    // (16 per tile) and identify the top-left of the player's half-open box.
    // A fixed tick advances at 60 Hz; no floating-point or frame-clock dependency.
    class RuntimePlatformer
    {
    public:
        static constexpr int Tile=16, Width=10, Height=14;
        struct Input{int Axis=0;bool Jump=false;};
        struct State
        {
            int X=0,Y=0,VY=0;unsigned Coins=0,Deaths=0;
            bool Grounded=false,JumpHeld=false,Won=false;
            friend bool operator==(const State&,const State&)=default;
        };
        // Own the immutable level and mutable coin map. Validate before starting:
        // a rectangular bounded map, one spawn/goal, and a solid outer border.
        explicit RuntimePlatformer(std::vector<std::string> rows):m_Level(std::move(rows))
        {
            if(m_Level.size()<3||m_Level.size()>64||m_Level.front().size()<3||m_Level.front().size()>64)
                throw std::invalid_argument("Invalid platformer dimensions.");
            unsigned spawn=0,goal=0;
            for(std::size_t y=0;y<m_Level.size();++y)
            {
                if(m_Level[y].size()!=m_Level.front().size())throw std::invalid_argument("Ragged platformer map.");
                for(std::size_t x=0;x<m_Level[y].size();++x)
                {
                    const auto c=m_Level[y][x];
                    if(std::string_view("#.SCHG").find(c)==std::string_view::npos)throw std::invalid_argument("Unknown tile.");
                    if((!x||!y||x+1==m_Level[y].size()||y+1==m_Level.size())&&c!='#')throw std::invalid_argument("Open level boundary.");
                    if(c=='S'){++spawn;m_SpawnX=int(x)*Tile+3;m_SpawnY=int(y)*Tile+2;}
                    if(c=='G')++goal;
                    if(c=='C')++m_CoinCount;
                }
            }
            if(spawn!=1||goal!=1)throw std::invalid_argument("Expected one spawn and goal.");
            Reset();
        }
        [[nodiscard]] const State& Current() const noexcept{return m_State;}
        [[nodiscard]] const std::vector<std::string>& Rows() const noexcept{return m_Rows;}
        [[nodiscard]] unsigned CoinCount() const noexcept{return m_CoinCount;}
        // Restart the entire authored level. Death uses the same reset but
        // increments the death counter. Collectibles are reset on death.
        void Reset()
        {
            m_Rows=m_Level;m_State={};m_State.X=m_SpawnX;m_State.Y=m_SpawnY;
            m_State.Grounded=Blocked(m_State.X,m_State.Y+1);
        }
        // Unit subcell sweeps make collision independent of speed within the
        // bounded velocity. Axis separation is a deliberate simple platformer
        // policy. Jump is edge-triggered, so holding it cannot auto-bounce.
        void Step(Input input)
        {
            if(input.Axis<-1||input.Axis>1)throw std::invalid_argument("Invalid movement axis.");
            if(m_State.Won)return;
            if(input.Jump&&!m_State.JumpHeld&&m_State.Grounded)m_State.VY=-10;
            m_State.JumpHeld=input.Jump;
            m_State.VY=std::min(m_State.VY+1,10);
            Sweep(input.Axis*2,true);
            Sweep(m_State.VY,false);
            m_State.Grounded=Blocked(m_State.X,m_State.Y+1);
            // Check all occupied tiles after collision. Hazard takes priority
            // over coins and goal, avoiding an ambiguous simultaneous win/death.
            if(Touches('H'))
            {
                const auto deaths=m_State.Deaths;Reset();m_State.Deaths=deaths==UINT32_MAX?deaths:deaths+1;
                m_State.JumpHeld=input.Jump;return;
            }
            ForTiles(m_State.X,m_State.Y,[&](int x,int y){if(m_Rows[y][x]=='C'){m_Rows[y][x]='.';++m_State.Coins;}return false;});
            if(m_State.Coins==m_CoinCount&&Touches('G'))m_State.Won=true;
        }
    private:
        std::vector<std::string> m_Level,m_Rows;
        State m_State;int m_SpawnX=0,m_SpawnY=0;unsigned m_CoinCount=0;
        // Half-open box: the last included subcell is X+Width-1.
        // Outside is solid even if an invalid coordinate is supplied.
        template<class Predicate> bool ForTiles(int x,int y,Predicate predicate) const
        {
            if(x<0||y<0||x+Width>int(m_Level.front().size())*Tile||y+Height>int(m_Level.size())*Tile)return true;
            for(int j=y/Tile;j<=(y+Height-1)/Tile;++j)for(int i=x/Tile;i<=(x+Width-1)/Tile;++i)
                if(predicate(i,j))return true;
            return false;
        }
        [[nodiscard]] bool Blocked(int x,int y) const
        {return ForTiles(x,y,[&](int i,int j){return m_Rows[j][i]=='#';});}
        [[nodiscard]] bool Touches(char tile) const
        {return ForTiles(m_State.X,m_State.Y,[&](int i,int j){return m_Rows[j][i]==tile;});}
        // Sweep one unit at a time; no tunneling through a one-tile solid.
        // Collision on the vertical axis cancels vertical velocity.
        void Sweep(int amount,bool horizontal)
        {
            const int sign=amount<0?-1:1;
            for(int i=0;i<std::abs(amount);++i)
            {
                const int x=m_State.X+(horizontal?sign:0),y=m_State.Y+(horizontal?0:sign);
                if(Blocked(x,y)){if(!horizontal)m_State.VY=0;break;}
                m_State.X=x;m_State.Y=y;
            }
        }
    };
    // A small authored route: coin, one-tile hazard, second coin, exit.
    // Solid ledge/ceiling exercises collision without hiding the finish.
    [[nodiscard]] inline std::vector<std::string> PlatformerDemoLevel()
    {return {"################","#..............#","#..............#","#..............#","#S.C..H..C...G.#","################"};}
}
