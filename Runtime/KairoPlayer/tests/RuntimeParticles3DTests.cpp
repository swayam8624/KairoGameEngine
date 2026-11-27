#include <algorithm>
#include "RuntimeParticles3D.hpp"
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
using namespace kairo::player::detail;
void Check(bool ok){if(!ok)throw std::runtime_error("Particle regression");}
int main()
{
    std::mt19937 rng(2026);
    for(unsigned i=0;i<1000;++i)
    {
        const auto capacity=1u+rng()%128u;
        ParticlePoint origin{float(int(rng()%200)-100),2.f,3.f};
        RuntimeParticles3D a(capacity),b(capacity);
        for(unsigned tick=0;tick<120;++tick)a.Advance(1./60.,origin);
        for(unsigned frame=0;frame<40;++frame)b.Advance(3./60.,origin);
        Check(a.Slots()==b.Slots());
        unsigned count=0;
        for(const auto& p:a.Slots())if(p.Active){++count;Check(p.Age<48&&p.Scale()>.25f&&p.Scale()<=1.f);}
        Check(count==std::min<unsigned>(capacity,16));
        const auto before=a.Slots();
        try{a.Advance(-1,origin);Check(false);}catch(const std::invalid_argument&){}
        Check(a.Slots()==before);
        try{a.Advance(0,ParticlePoint{std::numeric_limits<float>::infinity(),0,0});Check(false);}catch(const std::invalid_argument&){}
        Check(a.Slots()==before);
        for(unsigned tick=0;tick<48;++tick)a.Advance(1./60.,std::nullopt);
        for(const auto& p:a.Slots())Check(!p.Active);
        a.Advance(.05,origin);
        Check(a.Slots().front().Active&&a.Slots().front().Position==origin);
    }
    for(auto capacity:{0u,129u}){try{RuntimeParticles3D invalid(capacity);Check(false);}catch(const std::invalid_argument&){}}
    std::cout<<"1000 deterministic partition, saturation, drain, reuse and rejection fixtures passed\n";
}
