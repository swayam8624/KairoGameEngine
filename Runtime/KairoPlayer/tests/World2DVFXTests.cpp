#include "../World2DVFX.hpp"
#include <iostream>
#include <optional>
#include <random>
using namespace kairo::player::vfx2d;
void Assert(bool ok){if(!ok)throw std::runtime_error("VFX regression");}
int main()
{
    const std::string bytes="kairo-vfx2d 1\nmaterials 2\ngold ffffff 884400 6 1 cross\nsmoke 8899aa 222222 2 8 square\nemitters 3\nmain 1 collect gold 12 1 30 120 180\nmain 8 ambient smoke 2 3 45 40 -30\nmain 1 death gold 64 1 30 240 600\n";
    auto config=Parse(bytes);System a(config),b(config);a.Begin("main");b.Begin("main");
    auto origin=[](unsigned id)->std::optional<std::pair<int,int>>{if(id==1||id==8)return std::pair{100,80};return {};};
    a.Burst(Event::Collect,origin);b.Burst(Event::Collect,origin);Assert(a.Particles().size()==12);
    Assert(Tint(a.Particles()[0])==0xffffff);
    std::mt19937 rng(2026);std::vector<Rect> x,y;
    for(unsigned i=0;i<10000;++i)
    {
        if(i%17==0){a.Burst(Event::Death,origin);b.Burst(Event::Death,origin);}
        a.Step(origin);b.Step(origin);Assert(a.Particles()==b.Particles()&&a.Particles().size()<=MaxParticles);
        const unsigned w=1+rng()%3840,h=1+rng()%2160;const int cx=int(rng()%100),cy=int(rng()%100);
        a.Paint(cx,cy,256,192,w,h,x,128);b.Paint(cx,cy,256,192,w,h,y,128);
        Assert(x==y&&x.size()<=128);
        for(const auto& r:x)Assert(r.X>=0&&r.Y>=0&&r.W>0&&r.H>0&&r.X+r.W<=int(w)&&r.Y+r.H<=int(h));
    }
    a.Begin("other");a.Burst(Event::Collect,origin);a.Step(origin);Assert(a.Particles().empty());
    a.Begin("main");for(unsigned i=0;i<20;++i)a.Burst(Event::Death,origin);
    Assert(a.Particles().size()==512&&a.Dropped()==768);
    for(unsigned i=0;i<30;++i)a.Step(origin,false);
    Assert(a.Particles().empty());
    const auto reject=[](std::string s){try{(void)Parse(s);}catch(const std::invalid_argument&){return;}throw std::runtime_error("Invalid VFX accepted");};
    reject(bytes+"junk");auto bad=bytes;bad.replace(bad.find("ffffff"),6,"gggggg");reject(bad);
    bad=bytes;bad.replace(bad.find("12 1 30"),7,"65 1 30");reject(bad);
    bad=bytes;bad.replace(bad.find("collect gold"),12,"collect nope");reject(bad);
    std::cout<<"10000 deterministic particle simulation/projection comparisons, budget, lifetime, material and malformed source checks passed\n";
}
