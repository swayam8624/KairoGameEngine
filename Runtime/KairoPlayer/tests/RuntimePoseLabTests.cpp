#include "RuntimePoseLab.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
static void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static double Distance(RetargetVector a,RetargetVector b){return ik_detail::Length(ik_detail::Sub(a,b));}
int main()
{
    const auto first=SamplePoseLab(0.),second=SamplePoseLab(1.6);
    Check(!first.Reached&&second.Reached&&Distance(second.Chains[2].back().Translation,second.Target)<1e-9,"visible reach states");
    const auto a=PoseLabView(0,864,360),b=PoseLabView(1.6,864,360);
    bool changed=a.size()!=b.size();
    for(std::size_t i=0;i<std::min(a.size(),b.size());++i)changed|=a[i].Box!=b[i].Box||a[i].Color!=b[i].Color;
    Check(changed,"presentation unchanged");
    std::mt19937 rng(91);std::uniform_real_distribution<double> phase(-100.,100.);
    for(int i=0;i<5000;++i)
    {
        const double t=phase(rng);const auto f=SamplePoseLab(t);
        const auto& source=f.Chains[0];const auto& target=f.Chains[1];const auto& solved=f.Chains[2];
        Check(std::abs(Distance(source[0].Translation,source[1].Translation)-1)<1e-9,"source length");
        for(const auto* chain:{&target,&solved})Check(std::abs(Distance((*chain)[0].Translation,(*chain)[1].Translation)-1.5)<1e-9&&
            std::abs(Distance((*chain)[1].Translation,(*chain)[2].Translation)-1.1)<1e-9,"target length");
        if(f.Reached)Check(Distance(solved.back().Translation,f.Target)<1e-9,"IK misses target");
        const unsigned w=1+rng()%4000,h=1+rng()%3000;
        const auto rects=PoseLabView(t,w,h);Check(rects.size()<4096,"overlay budget");
        for(const auto& r:rects)Check(r.Box.X>=0&&r.Box.Y>=0&&r.Box.W>0&&r.Box.H>0&&r.Box.X+r.Box.W<=int(w)&&r.Box.Y+r.Box.H<=int(h),"viewport bounds");
    }
    std::cout<<"5000 animated source/retarget/IK poses and viewport bounds passed\n";
}
