#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace kairo::player::vfx2d
{
    inline constexpr std::size_t MaxBytes=8192,MaxMaterials=16,MaxEmitters=32,MaxParticles=512;
    inline void Check(bool ok,const char* why){if(!ok)throw std::invalid_argument(why);}
    inline bool Name(std::string_view s)
    {
        return !s.empty()&&s.size()<=32&&std::all_of(s.begin(),s.end(),[](char c)
        {return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_';});
    }
    inline unsigned Color(std::string_view s)
    {
        Check(s.size()==6,"Invalid VFX RGB color.");unsigned rgb=0;
        for(char c:s){int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;Check(n>=0,"Invalid VFX RGB color.");rgb=16*rgb+unsigned(n);}
        return rgb;
    }
    enum class Event {Ambient,Collect,Death,Complete};
    struct Material {unsigned Start=0,End=0,StartSize=1,EndSize=1;bool Cross=false;friend bool operator==(Material,Material)=default;};
    struct Emitter
    {
        std::string Level,MaterialID;unsigned Actor=0;
        Event Trigger=Event::Ambient;
        unsigned Count=1,Interval=1,Life=60,Speed=0;
        int Gravity=0;
    };
    struct Config {std::map<std::string,Material> Materials;std::vector<Emitter> Emitters;};
    inline Config Parse(std::string_view bytes)
    {
        Check(!bytes.empty()&&bytes.size()<=MaxBytes,"Invalid VFX document size.");
        std::istringstream in{std::string(bytes)};in.imbue(std::locale::classic());
        Config c;std::string tag;int version=0,count=0;
        Check(bool(in>>tag>>version)&&tag=="kairo-vfx2d"&&version==1,"Unsupported VFX version.");
        Check(bool(in>>tag>>count)&&tag=="materials"&&count>=1&&count<=int(MaxMaterials),"Invalid VFX material count.");
        for(int i=0;i<count;++i)
        {
            std::string id,start,end,shape;Material m;
            Check(bool(in>>id>>start>>end>>m.StartSize>>m.EndSize>>shape)&&Name(id)&&
                m.StartSize>=1&&m.StartSize<=24&&m.EndSize>=1&&m.EndSize<=24&&
                (shape=="square"||shape=="cross"),"Invalid VFX material.");
            m.Start=Color(start);m.End=Color(end);m.Cross=shape=="cross";
            Check(c.Materials.emplace(id,m).second,"Duplicate VFX material.");
        }
        Check(bool(in>>tag>>count)&&tag=="emitters"&&count>=1&&count<=int(MaxEmitters),"Invalid VFX emitter count.");
        for(int i=0;i<count;++i)
        {
            Emitter e;std::string event;long long actor=0;
            Check(bool(in>>e.Level>>actor>>event>>e.MaterialID>>e.Count>>e.Interval>>e.Life>>e.Speed>>e.Gravity)&&
                Name(e.Level)&&actor>0&&actor<=0xffffffffll&&c.Materials.contains(e.MaterialID)&&
                e.Count>=1&&e.Count<=64&&e.Interval>=1&&e.Interval<=3600&&e.Life>=1&&e.Life<=600&&
                e.Speed<=240&&e.Gravity>=-600&&e.Gravity<=600,"Invalid VFX emitter.");
            e.Actor=static_cast<unsigned>(actor);
            Check(event=="ambient"||event=="collect"||event=="death"||event=="complete","Unknown VFX event.");
            e.Trigger=event=="ambient"?Event::Ambient:event=="collect"?Event::Collect:event=="death"?Event::Death:Event::Complete;
            Check(e.Trigger==Event::Ambient||e.Interval==1,"Burst emitter interval must be 1.");
            c.Emitters.push_back(std::move(e));
        }
        Check(!(in>>tag),"Trailing VFX data.");return c;
    }
    // Fixed-point positions and velocities (1/256 pixel, per 60 Hz tick).
    // No floating RNG, wall-clock dependence or platform trig functions.
    struct Particle {int X=0,Y=0,VX=0,VY=0,G=0;unsigned Age=0,Life=1;Material Style;friend bool operator==(const Particle&,const Particle&)=default;};
    struct Rect {int X=0,Y=0,W=0,H=0;unsigned RGB=0;friend bool operator==(Rect,Rect)=default;};
    inline unsigned Lerp(unsigned a,unsigned b,unsigned age,unsigned life){return (a*(life-age)+b*age+life/2)/life;}
    inline unsigned Tint(const Particle& p)
    {
        unsigned rgb=0;for(unsigned shift:{16u,8u,0u})rgb|=Lerp((p.Style.Start>>shift)&255,(p.Style.End>>shift)&255,p.Age,p.Life)<<shift;
        return rgb;
    }
    class System
    {
        Config m_Config;
        std::vector<Particle> m_Live;
        std::uint32_t m_Random=1;
        std::uint64_t m_Tick=0,m_Dropped=0;
        std::string m_Level;
        unsigned Random(){m_Random^=m_Random<<13;m_Random^=m_Random>>17;m_Random^=m_Random<<5;return m_Random;}
        void Spawn(const Emitter& e,int x,int y)
        {
            Check(x>=-100000&&x<=100000&&y>=-100000&&y<=100000,"VFX origin out of bounds.");
            constexpr std::array<std::array<int,2>,8> dirs{{{256,0},{181,181},{0,256},{-181,181},{-256,0},{-181,-181},{0,-256},{181,-181}}};
            for(unsigned i=0;i<e.Count;++i)
            {
                const auto& d=dirs[Random()%dirs.size()];const int speed=int(e.Speed)*(128+int(Random()%129))/256;
                if(m_Live.size()==MaxParticles){++m_Dropped;continue;}
                m_Live.push_back({x*256,y*256,d[0]*speed/60,d[1]*speed/60,e.Gravity*256/3600,0,e.Life,m_Config.Materials.at(e.MaterialID)});
            }
        }
    public:
        explicit System(Config config):m_Config(std::move(config)){m_Live.reserve(MaxParticles);}
        void Begin(std::string_view level){m_Level=level;m_Tick=0;m_Dropped=0;m_Random=1;m_Live.clear();}
        const auto& Particles()const noexcept{return m_Live;}
        std::uint64_t Dropped()const noexcept{return m_Dropped;}
        template<class Origin> void Burst(Event event,Origin origin)
        {
            for(const auto& e:m_Config.Emitters)if(e.Level==m_Level&&e.Trigger==event)
                if(auto p=origin(e.Actor))Spawn(e,p->first,p->second);
        }
        template<class Origin> void Step(Origin origin,bool ambient=true)
        {
            ++m_Tick;
            for(auto& p:m_Live){++p.Age;p.VY+=p.G;p.X+=p.VX;p.Y+=p.VY;}
            std::erase_if(m_Live,[](const Particle& p){return p.Age>=p.Life;});
            if(ambient)for(const auto& e:m_Config.Emitters)if(e.Level==m_Level&&e.Trigger==Event::Ambient&&m_Tick%e.Interval==0)
                if(auto p=origin(e.Actor))Spawn(e,p->first,p->second);
        }
        // Project to the same letterboxed physical-pixel canvas as the world.
        // Caller supplies a bounded overlay allowance; old particles win ties.
        void Paint(int cx,int cy,int cw,int ch,unsigned w,unsigned h,std::vector<Rect>& out,std::size_t allowance=1024)const
        {
            out.clear();if(cw<=0||ch<=0||!w||!h||w>32768||h>32768)return;
            const double scale=std::min(double(w)/cw,double(h)/ch);
            const int sw=int(cw*scale),sh=int(ch*scale),ox=(int(w)-sw)/2,oy=(int(h)-sh)/2;
            const auto draw=[&](double x,double y,double width,double height,unsigned rgb)
            {
                const int l=ox+int(std::clamp(x-cx,0.,double(cw))*sw/cw),r=ox+int(std::clamp(x+width-cx,0.,double(cw))*sw/cw);
                const int t=oy+int(std::clamp(y-cy,0.,double(ch))*sh/ch),b=oy+int(std::clamp(y+height-cy,0.,double(ch))*sh/ch);
                if(r>l&&b>t&&out.size()<allowance)out.push_back({l,t,r-l,b-t,rgb});
            };
            for(const auto& p:m_Live)
            {
                const double size=Lerp(p.Style.StartSize,p.Style.EndSize,p.Age,p.Life),x=p.X/256.-size/2,y=p.Y/256.-size/2;
                const auto rgb=Tint(p);
                if(p.Style.Cross){const double arm=std::max(1.,size/3);draw(x+(size-arm)/2,y,arm,size,rgb);draw(x,y+(size-arm)/2,size,arm,rgb);}
                else draw(x,y,size,size,rgb);
            }
        }
    };
}
