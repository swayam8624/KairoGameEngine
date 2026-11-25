#include "RuntimeBreakableCube.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()
{
    std::mt19937 rng(311);
    for(unsigned trial=0;trial<1000;++trial)
    {
        const double mass=1.+double(rng()%100)/10.,h=.5+double(rng()%100)/100.;
        RuntimeBreakableCube cube({0,3,0},h,mass,2);
        Check(!cube.Strike({1,0,0})&&!cube.Fractured(),"Weak strike fractured");
        const FragmentPoint impulse{2.+double(rng()%100)/10.,double(rng()%100)/20.,-3.};
        Check(cube.Strike(impulse)&&!cube.Strike(impulse),"One-shot fracture violated");
        double total=0,volume=0;FragmentPoint momentum{},center{};
        for(const auto& f:cube.Fragments())
        {
            total+=f.Mass;volume+=8*f.HalfExtent*f.HalfExtent*f.HalfExtent;
            for(unsigned axis=0;axis<3;++axis){momentum[axis]+=f.Mass*f.Velocity[axis];center[axis]+=f.Mass*f.Center[axis];}
        }
        Check(std::abs(total-mass)<1e-10&&std::abs(volume-8*h*h*h)<1e-10,"Mass/volume conservation");
        for(unsigned axis=0;axis<3;++axis)
            Check(std::abs(momentum[axis]-impulse[axis])<1e-10&&std::abs(center[axis]/mass-cube.Center()[axis])<1e-10,"Momentum/COM conservation");
        for(unsigned tick=0;tick<240;++tick)
        {
            cube.Step();for(const auto& f:cube.Fragments())Check(f.Center[1]>=f.HalfExtent,"Ground penetration");
        }
        cube.Reset();Check(!cube.Fractured()&&cube.Strike(impulse),"Reset failed");
    }
    bool rejected=false;try{RuntimeBreakableCube bad({0,0,0},1,0,1);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Zero mass accepted");
    std::cout<<"1000 fracture events and 240000 fragment simulation steps passed\n";
}
