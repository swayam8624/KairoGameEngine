#include "RuntimeTetraSurface.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
using namespace kairo::player::detail::soft_detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void CheckSurface(const std::array<SoftPoint,4>& p)
{
    const auto surface=BuildTetraSurface(p);
    const auto center=Scale(Add(Add(p[0],p[1]),Add(p[2],p[3])),.25);
    for(unsigned i=0;i<12;i+=3)
    {
        const auto& a=surface[i];const auto& b=surface[i+1];const auto& c=surface[i+2];
        const auto winding=Cross(Sub(b.Position,a.Position),Sub(c.Position,a.Position));
        Check(a.Normal==b.Normal&&b.Normal==c.Normal,"Inconsistent flat normal");
        Check(std::abs(Length(a.Normal)-1)<1e-12&&Dot(winding,a.Normal)>0,"Winding/normal mismatch");
        Check(Dot(a.Normal,Sub(center,a.Position))<0,"Inward face");
        for(const auto* v:{&a,&b,&c})Check(std::find(p.begin(),p.end(),v->Position)!=p.end(),"Invented vertex");
    }
}
int main()
{
    std::mt19937 rng(451);
    for(unsigned i=0;i<10000;++i)
    {
        const SoftPoint offset{double(int(rng()%200)-100),double(int(rng()%200)-100),double(int(rng()%200)-100)};
        const double s=.1+double(rng()%100)/10.;
        std::array<SoftPoint,4> p{offset,Add(offset,{s,.1,0}),Add(offset,{.2,s,.1}),Add(offset,{0,.1,s+1})};
        if(i%2)std::swap(p[1],p[2]);
        CheckSurface(p);
    }
    RuntimeSoftBody body;body.AddVertex({0,3,0});body.AddVertex({1,3,0});body.AddVertex({0,4,0});body.AddVertex({0,3,1});
    for(unsigned a=0;a<4;++a)for(unsigned b=a+1;b<4;++b)body.AddEdge(a,b);
    body.AddTetra({0,1,2,3});
    for(unsigned tick=0;tick<240;++tick)
    {
        body.Step();const auto surface=BuildTetraSurface(body);
        Check(surface.size()==12,"Wrong surface size");
        CheckSurface({body.Vertices()[0].Position,body.Vertices()[1].Position,body.Vertices()[2].Position,body.Vertices()[3].Position});
    }
    bool rejected=false;try{(void)BuildTetraSurface(std::array<SoftPoint,4>{{{0,0,0},{1,0,0},{0,1,0},{1,1,0}}});}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Coplanar tetra accepted");
    std::cout<<"10000 oriented tetrahedra and 240 deforming surface frames passed\n";
}
