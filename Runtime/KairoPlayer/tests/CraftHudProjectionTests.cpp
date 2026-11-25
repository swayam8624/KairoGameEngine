#include "../CraftHudProjection.hpp"
#include <iostream>
#include <random>

namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
bool Near(double a,double b){return std::abs(a-b)<1e-8;}
}
int main()
{
    std::mt19937 rng(734);
    for(unsigned i=0;i<100000;++i)
    {
        const CraftHudViewport v{1u+static_cast<std::uint32_t>(rng()%8192),1u+static_cast<std::uint32_t>(rng()%8192)};
        const double fov=.2+(rng()%2500)/1000.,depth=.1+(rng()%1000)/1000.;
        const auto center=ProjectCraftHudRect(v,479,269,2,2,fov,depth,false);
        const auto hotbar=ProjectCraftHudRect(v,356,472,56,56,fov,depth,true);
        const double pixel=2*depth*std::tan(fov*.5)/v.Height;
        const double scale=std::min(v.Width/960.,v.Height/540.);
        Check(Near(center.X,0)&&Near(center.Y,0),"Crosshair moved off center");
        Check(Near(center.Width,center.Height),"Crosshair stretched");
        Check(Near(hotbar.Width,hotbar.Height),"Hotbar stretched");
        Check(Near((hotbar.Y-hotbar.Height*.5)/pixel+v.Height*.5,12*scale),"Bottom margin changed");
        Check(std::abs(hotbar.X)+hotbar.Width*.5<=v.Width*.5*pixel+1e-10,"Hotbar clipped horizontally");
        Check(std::abs(hotbar.Y)+hotbar.Height*.5<=v.Height*.5*pixel+1e-10,"Hotbar clipped vertically");
    }
    const double fov=1.,depth=.14,pixel=2*depth*std::tan(fov*.5)/540;
    const auto reference=ProjectCraftHudRect({},356,472,56,56,fov,depth,true);
    Check(Near(reference.X,(356+28-480)*pixel)&&Near(reference.Y,(270-472-28)*pixel),"Reference layout changed");
    bool rejected=false;
    try{(void)ProjectCraftHudRect({0,540},0,0,1,1,fov,depth,false);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Zero viewport accepted");
    std::cout<<"100000 HUD projection cases passed: square pixels, centered crosshair, bottom anchor and reference layout\n";
}
