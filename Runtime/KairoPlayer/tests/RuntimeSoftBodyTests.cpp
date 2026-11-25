#include "RuntimeSoftBody.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static RuntimeSoftBody Tetra(double height,bool pin,double scale)
{
    RuntimeSoftBody body;
    body.AddVertex({0,height,0},pin?0:1);body.AddVertex({scale,height,0});
    body.AddVertex({0,height+scale,0});body.AddVertex({0,height,scale});
    for(unsigned a=0;a<4;++a)for(unsigned b=a+1;b<4;++b)body.AddEdge(a,b);
    body.AddTetra({0,1,2,3});return body;
}
int main()
{
    auto rest=Tetra(2,false,1);const auto initial=rest.TetraVolume(0);
    rest.Step({0,0,0});Check(std::abs(rest.TetraVolume(0)-initial)<1e-12,"Rest changed");
    auto pin=Tetra(2,true,1);const auto fixed=pin.Vertices()[0].Position;
    for(unsigned i=0;i<240;++i)pin.Step();
    Check(pin.Vertices()[0].Position==fixed,"Pinned vertex moved");
    Check(std::abs(pin.TetraVolume(0)-initial)<.02,"Pinned tetra collapsed");
    std::mt19937 rng(931);
    for(unsigned trial=0;trial<100;++trial)
    {
        const double height=1.+double(rng()%100)/20.,scale=.5+double(rng()%100)/100.;
        auto a=Tetra(height,false,scale),b=a;const double volume=a.TetraVolume(0);
        for(unsigned tick=0;tick<240;++tick)
        {
            a.Step();b.Step();
            for(unsigned i=0;i<4;++i)
            {
                const auto& p=a.Vertices()[i].Position;
                Check(p[1]>=0&&p==b.Vertices()[i].Position,"Ground or replay violation");
            }
            Check(std::abs(a.TetraVolume(0)-volume)<.05*std::abs(volume),"Volume error exceeds 5 percent");
        }
    }
    bool rejected=false;try{rest.AddTetra({0,1,1,3});}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Degenerate tetra accepted");
    rejected=false;try{rest.Step({0,0,0},0,0);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Invalid iterations accepted");
    std::cout<<"24000 soft-body steps with deterministic replay, contacts and volume bounds passed\n";
}
