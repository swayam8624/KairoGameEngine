#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace kairo::player::world2d
{
    constexpr int Tile=16;
    constexpr std::size_t MaxBytes=256*1024,MaxActors=128,MaxSprites=64;
    enum class Mode { TopDown,Platform };
    enum class Kind { Player,Solid,Collect,Hazard,Goal,Patrol,Decor };
    struct Box {int X=0,Y=0,W=0,H=0;friend bool operator==(const Box&,const Box&)=default;};
    struct Sprite {int Width=4,Height=4,Period=1;std::vector<std::string> Frames;};
    struct TileType {unsigned Color=0;bool Solid=false;};
    struct Actor {unsigned ID=0;Kind Type=Kind::Decor;Box Bounds;std::string SpriteName;int Range=0;};
    struct World
    {
        int Width=0,Height=0,ViewW=0,ViewH=0,Speed=0,Gravity=0,Jump=0,Parallax=0;
        Mode Movement=Mode::TopDown;
        std::array<unsigned,16> Colors{0x15191f,0x384657,0x52748d,0x5cae82,0xe4bc58,0xd76565,0x4f8bdf,0xf5f1e8,
            0x282e38,0x7e91a2,0xb780d9,0x46b9b0,0xec9557,0x8fbd60,0xa16d57,0xffffff};
        std::map<char,TileType> Tiles;
        std::map<std::string,Sprite> Sprites;
        std::vector<std::string> Back,Map;
        std::vector<Actor> Actors;
    };
    inline void Require(bool ok,const char* message){if(!ok)throw std::invalid_argument(message);}
    inline bool Overlap(Box a,Box b) noexcept
    {return a.X<b.X+b.W&&b.X<a.X+a.W&&a.Y<b.Y+b.H&&b.Y<a.Y+a.H;}
    inline bool Name(std::string_view s)
    {
        if(s.empty()||s.size()>32)return false;
        for(char c:s)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return false;
        return true;
    }
    inline int Hex(char c)
    {
        if(c>='0'&&c<='9')return c-'0';
        if(c>='a'&&c<='f')return c-'a'+10;
        if(c>='A'&&c<='F')return c-'A'+10;
        return -1;
    }
    inline Kind ParseKind(std::string_view s)
    {
        if(s=="player")return Kind::Player;
        if(s=="solid")return Kind::Solid;
        if(s=="collect")return Kind::Collect;
        if(s=="hazard")return Kind::Hazard;
        if(s=="goal")return Kind::Goal;
        if(s=="patrol")return Kind::Patrol;
        if(s=="decor")return Kind::Decor;
        throw std::invalid_argument("Unknown actor kind.");
    }
    inline bool MapBlocked(const World& w,Box b)
    {
        if(b.X<0||b.Y<0||b.X+b.W>w.Width*Tile||b.Y+b.H>w.Height*Tile)return true;
        for(int y=b.Y/Tile;y<=(b.Y+b.H-1)/Tile;++y)
            for(int x=b.X/Tile;x<=(b.X+b.W-1)/Tile;++x)
                if(w.Tiles.at(w.Map[y][x]).Solid)return true;
        return false;
    }
    // Strict token grammar; maps have exactly Height rows, sprites have
    // exactly N indexed-color frames, and zero sprite pixels are transparent.
    // Version 1 is retained for the original 4x4 sprite grammar.
    inline World Parse(std::string_view bytes)
    {
        Require(bytes.size()<=MaxBytes,"2D world exceeds byte budget.");
        std::istringstream in{std::string(bytes)};
        std::string tag;int version=0;
        Require(bool(in>>tag>>version)&&tag=="kairo-world2d"&&(version==1||version==2),"Unsupported 2D world.");
        World w;
        Require(bool(in>>tag>>w.Width>>w.Height)&&tag=="size"&&w.Width>=3&&w.Width<=128&&w.Height>=3&&w.Height<=128,"Invalid world size.");
        std::string mode;
        Require(bool(in>>tag>>mode)&&tag=="mode"&&(mode=="topdown"||mode=="platform"),"Invalid movement mode.");
        w.Movement=mode=="topdown"?Mode::TopDown:Mode::Platform;
        Require(bool(in>>tag>>w.ViewW>>w.ViewH>>w.Parallax)&&tag=="camera"&&w.ViewW>=3&&w.ViewW<=24&&w.ViewW<=w.Width&&
            w.ViewH>=3&&w.ViewH<=18&&w.ViewH<=w.Height&&w.Parallax>=0&&w.Parallax<=100,"Invalid camera.");
        Require(bool(in>>tag>>w.Speed>>w.Gravity>>w.Jump)&&tag=="motion"&&w.Speed>=1&&w.Speed<=16&&
            w.Gravity>=1&&w.Gravity<=8&&w.Jump>=1&&w.Jump<=32,"Invalid motion.");
        int count=0;
        Require(bool(in>>tag>>count)&&tag=="colors"&&count>=0&&count<=16,"Invalid palette.");
        std::array<bool,16> assigned{};
        for(int i=0;i<count;++i)
        {
            int index=0;std::string rgb;
            Require(bool(in>>index>>rgb)&&index>=0&&index<16&&!assigned[index]&&rgb.size()==6,"Invalid palette entry.");
            unsigned value=0;
            for(char c:rgb){const int d=Hex(c);Require(d>=0,"Invalid color.");value=value*16+static_cast<unsigned>(d);}
            assigned[index]=true;w.Colors[index]=value;
        }
        Require(bool(in>>tag>>count)&&tag=="tiles"&&count>=1&&count<=64,"Invalid tile definitions.");
        for(int i=0;i<count;++i)
        {
            std::string key;int color=0,solid=0;
            Require(bool(in>>key>>color>>solid)&&key.size()==1&&key[0]>=33&&key[0]<=126&&
                color>=0&&color<16&&(solid==0||solid==1),"Invalid tile definition.");
            Require(w.Tiles.emplace(key[0],TileType{static_cast<unsigned>(color),solid!=0}).second,"Duplicate tile.");
        }
        Require(w.Tiles.contains('.')&&!w.Tiles.at('.').Solid,"Missing empty tile.");
        Require(bool(in>>tag>>count)&&tag=="sprites"&&count>=1&&count<=static_cast<int>(MaxSprites),"Invalid sprite count.");
        for(int i=0;i<count;++i)
        {
            std::string name;int period=0,frames=0;
            Sprite sprite;
            Require(bool(in>>name)&&Name(name),"Invalid sprite name.");
            if(version==2)Require(bool(in>>sprite.Width>>sprite.Height)&&sprite.Width>=1&&sprite.Width<=32&&sprite.Height>=1&&sprite.Height<=32,"Invalid sprite dimensions.");
            Require(bool(in>>period>>frames)&&period>=1&&period<=600&&frames>=1&&frames<=16,"Invalid sprite definition.");
            sprite.Period=period;
            for(int j=0;j<frames;++j)
            {
                std::string frame;
                Require(bool(in>>frame)&&frame.size()==static_cast<std::size_t>(sprite.Width*sprite.Height),"Invalid sprite frame.");
                for(char c:frame)Require(Hex(c)>=0,"Invalid sprite pixel.");
                sprite.Frames.push_back(std::move(frame));
            }
            Require(w.Sprites.emplace(name,std::move(sprite)).second,"Duplicate sprite.");
        }
        const auto layer=[&](const char* name,std::vector<std::string>& rows)
        {
            Require(bool(in>>tag)&&tag==name,"Missing tile layer.");
            for(int y=0;y<w.Height;++y)
            {
                std::string row;Require(bool(in>>row)&&row.size()==static_cast<std::size_t>(w.Width),"Ragged tile layer.");
                for(char c:row)Require(w.Tiles.contains(c),"Unknown tile.");
                rows.push_back(std::move(row));
            }
        };
        layer("background",w.Back);layer("map",w.Map);
        Require(bool(in>>tag>>count)&&tag=="actors"&&count>=1&&count<=static_cast<int>(MaxActors),"Invalid actor count.");
        unsigned players=0,goals=0;
        for(int i=0;i<count;++i)
        {
            Actor a;std::string kind;long long id=0;
            Require(bool(in>>id>>kind>>a.Bounds.X>>a.Bounds.Y>>a.Bounds.W>>a.Bounds.H>>a.SpriteName>>a.Range),"Invalid actor record.");
            Require(id>0&&id<=0xffffffffll,"Invalid actor ID.");
            a.ID=static_cast<unsigned>(id);
            a.Type=ParseKind(kind);
            const auto b=a.Bounds;
            Require(a.ID>0&&b.X>=0&&b.Y>=0&&b.W>=4&&b.W<=64&&b.H>=4&&b.H<=64&&
                b.X<=w.Width*Tile-b.W&&b.Y<=w.Height*Tile-b.H&&w.Sprites.contains(a.SpriteName)&&
                a.Range>=0&&a.Range<=512&&(a.Type==Kind::Patrol||a.Range==0),"Invalid actor bounds or sprite.");
            Require(a.Type!=Kind::Patrol||(b.X>=a.Range&&b.X+b.W+a.Range<=w.Width*Tile),"Patrol range outside world.");
            Require(std::none_of(w.Actors.begin(),w.Actors.end(),[&](const Actor& other){return other.ID==a.ID;}),"Duplicate actor ID.");
            if(a.Type==Kind::Player)++players;
            if(a.Type==Kind::Goal)++goals;
            if(a.Type!=Kind::Decor)Require(!MapBlocked(w,b),"Actor overlaps collision tile.");
            w.Actors.push_back(std::move(a));
        }
        Require(players==1&&goals>=1&&!(in>>tag),"Expected one player, a goal and no trailing data.");
        // Bound the worst possible simultaneous run count across all actors.
        // No sprite is dropped or silently degraded at render time.
        std::map<std::string,std::size_t> maximumRuns;
        for(const auto& [name,sprite]:w.Sprites)
            for(const auto& frame:sprite.Frames)
            {
                std::size_t runs=0;
                for(int y=0;y<sprite.Height;++y)
                    for(int x=0;x<sprite.Width;++x)
                    {
                        const int c=Hex(frame[y*sprite.Width+x]);
                        if(c&&(x==0||Hex(frame[y*sprite.Width+x-1])!=c))++runs;
                    }
                maximumRuns[name]=std::max(maximumRuns[name],runs);
            }
        std::size_t spriteRuns=0;
        for(const auto& actor:w.Actors)spriteRuns+=maximumRuns[actor.SpriteName];
        Require(spriteRuns<=3072,"World sprite draw budget exceeds 3072 color runs.");
        for(const auto& a:w.Actors)if(a.Type==Kind::Player)
            for(const auto& b:w.Actors)if(b.Type==Kind::Solid)
                Require(!Overlap(a.Bounds,b.Bounds),"Player overlaps solid actor.");
        return w;
    }
    inline World Load(const std::filesystem::path& path)
    {
        std::ifstream in(path,std::ios::binary);
        if(!in)throw std::runtime_error("Cannot open 2D world.");
        std::string bytes(MaxBytes+1,'\0');
        in.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
        if(in.bad())throw std::runtime_error("Cannot read 2D world.");
        bytes.resize(static_cast<std::size_t>(in.gcount()));
        return Parse(bytes);
    }
    struct Input {int X=0,Y=0;bool Jump=false;};
    struct LiveActor {Box Bounds;bool Active=true;int Direction=1;};
    class Game
    {
        World m_World;
        std::vector<LiveActor> m_Actors;
        std::size_t m_Player=0;
        int m_VY=0;
        bool m_PreviousJump=false,m_Won=false;
        unsigned m_Collected=0,m_Total=0,m_Deaths=0;
        std::uint64_t m_Tick=0;
        bool Blocked(Box box) const
        {
            if(MapBlocked(m_World,box))return true;
            for(std::size_t i=0;i<m_Actors.size();++i)
                if(m_World.Actors[i].Type==Kind::Solid&&Overlap(box,m_Actors[i].Bounds))return true;
            return false;
        }
        // Check contacts at every unit sweep, including fast motion through
        // narrow triggers. Hazards take precedence over collection and goals.
        bool Contacts()
        {
            const auto player=m_Actors[m_Player].Bounds;
            for(std::size_t i=0;i<m_Actors.size();++i)
                if(m_Actors[i].Active&&Overlap(player,m_Actors[i].Bounds)&&
                    (m_World.Actors[i].Type==Kind::Hazard||m_World.Actors[i].Type==Kind::Patrol))
                {++m_Deaths;Reset();return true;}
            bool goal=false;
            for(std::size_t i=0;i<m_Actors.size();++i)
            {
                auto& actor=m_Actors[i];
                if(!actor.Active||!Overlap(player,actor.Bounds))continue;
                if(m_World.Actors[i].Type==Kind::Collect){actor.Active=false;++m_Collected;}
                if(m_World.Actors[i].Type==Kind::Goal)goal=true;
            }
            m_Won=goal&&m_Collected==m_Total;
            return m_Won;
        }
        bool Sweep(int dx,int dy)
        {
            auto& b=m_Actors[m_Player].Bounds;
            const int steps=std::max(dx<0?-dx:dx,dy<0?-dy:dy);
            for(int i=0;i<steps;++i)
            {
                Box next=b;next.X+=(dx>0)-(dx<0);next.Y+=(dy>0)-(dy<0);
                if(Blocked(next))return false;
                b=next;
                if(Contacts())return false;
            }
            return true;
        }
    public:
        explicit Game(World world):m_World(std::move(world))
        {
            for(std::size_t i=0;i<m_World.Actors.size();++i)
            {
                if(m_World.Actors[i].Type==Kind::Player)m_Player=i;
                if(m_World.Actors[i].Type==Kind::Collect)++m_Total;
            }
            Reset();
        }
        void Reset()
        {
            m_Actors.clear();
            for(const auto& a:m_World.Actors)m_Actors.push_back({a.Bounds,true,1});
            m_VY=0;m_PreviousJump=false;m_Won=false;m_Collected=0;m_Tick=0;
        }
        void Step(Input input)
        {
            Require(input.X>=-1&&input.X<=1&&input.Y>=-1&&input.Y<=1,"Invalid 2D input.");
            if(m_Won)return;
            ++m_Tick;
            for(std::size_t i=0;i<m_Actors.size();++i)
            {
                const auto& a=m_World.Actors[i];auto& live=m_Actors[i];
                if(a.Type!=Kind::Patrol||a.Range==0)continue;
                Box next=live.Bounds;next.X+=live.Direction;
                if(next.X<a.Bounds.X-a.Range||next.X>a.Bounds.X+a.Range||Blocked(next))
                    live.Direction=-live.Direction;
                else live.Bounds=next;
            }
            const auto deaths=m_Deaths;
            if(Contacts()){m_PreviousJump=input.Jump;return;}
            Sweep(input.X*m_World.Speed,0);
            if(m_Deaths!=deaths||m_Won){m_PreviousJump=input.Jump;return;}
            if(m_World.Movement==Mode::TopDown)Sweep(0,input.Y*m_World.Speed);
            else
            {
                Box below=m_Actors[m_Player].Bounds;++below.Y;
                if(input.Jump&&!m_PreviousJump&&Blocked(below))m_VY=-m_World.Jump;
                m_VY=std::min(32,m_VY+m_World.Gravity);
                if(!Sweep(0,m_VY))m_VY=0;
            }
            m_PreviousJump=input.Jump;
        }
        [[nodiscard]] const World& Definition() const noexcept{return m_World;}
        [[nodiscard]] const std::vector<LiveActor>& Actors() const noexcept{return m_Actors;}
        [[nodiscard]] Box Player() const noexcept{return m_Actors[m_Player].Bounds;}
        [[nodiscard]] unsigned Collected() const noexcept{return m_Collected;}
        [[nodiscard]] unsigned Total() const noexcept{return m_Total;}
        [[nodiscard]] unsigned Deaths() const noexcept{return m_Deaths;}
        [[nodiscard]] bool Won() const noexcept{return m_Won;}
        [[nodiscard]] std::uint64_t Tick() const noexcept{return m_Tick;}
        [[nodiscard]] Box Camera() const noexcept
        {
            const auto p=Player();const int w=m_World.ViewW*Tile,h=m_World.ViewH*Tile;
            return {std::clamp(p.X+p.W/2-w/2,0,m_World.Width*Tile-w),
                std::clamp(p.Y+p.H/2-h/2,0,m_World.Height*Tile-h),w,h};
        }
    };
}
