#include "../CraftMouseLook.hpp"
#include <iostream>
#include <limits>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
int main()
{
    CraftMouseLook look;
    auto delta=look.Observe(true,100,200);Check(delta.Yaw==0&&delta.Pitch==0,"First sample moved camera");
    delta=look.Observe(true,120,180);Check(delta.Yaw==.05&&delta.Pitch==.05,"Mouse signs or sensitivity");
    (void)look.Observe(false,1000,1000);
    delta=look.Observe(true,-5000,5000);Check(delta.Yaw==0&&delta.Pitch==0,"Focus regain jumped");
    delta=look.Observe(true,1e200,-1e200);Check(delta.Yaw==1&&delta.Pitch==1,"Warp cap failed");
    (void)look.Observe(true,std::numeric_limits<double>::infinity(),0);
    delta=look.Observe(true,0,0);Check(delta.Yaw==0&&delta.Pitch==0,"Nonfinite sample poisoned baseline");
    double yaw=0,pitch=0;
    ApplyCraftLook(yaw,pitch,{.2,.1},0,0,0);
    Check(yaw==.2&&pitch==.1,"Mouse incorrectly scaled by dt");
    std::mt19937 rng(192);
    for(unsigned i=0;i<100000;++i)
    {
        const bool active=i%17!=0;
        const double x=double(int(rng()%20000)-10000),y=double(int(rng()%20000)-10000);
        delta=look.Observe(active,x,y);
        if(!active||i%17==1)Check(delta.Yaw==0&&delta.Pitch==0,"Inactive/reacquire motion");
        Check(std::abs(delta.Yaw)<=1&&std::abs(delta.Pitch)<=1,"Unbounded look delta");
        ApplyCraftLook(yaw,pitch,delta,double(int(rng()%3)-1),double(int(rng()%3)-1),double(rng()%100)/1000.);
        Check(std::abs(yaw)<=3.141592653589793&&std::abs(pitch)<=1.45,"Camera angles escaped limits");
    }
    std::cout<<"100000 mouse/focus transitions, warp cap, signs and camera limits passed\n";
}
