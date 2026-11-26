#include "../RuntimeWorld2DView.hpp"
#include <iostream>
#include <random>
#include <sstream>
using namespace kairo::player::world2d;
void Check(bool ok){if(!ok)throw std::runtime_error("World2D regression");}
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Check(rejected);}
std::string Fixture(std::string mode="topdown",std::string actors="1 player 16 16 4 4 hero 0\n2 collect 25 16 4 4 hero 0\n3 goal 60 16 4 4 hero 0\n",int count=3)
{
    std::ostringstream out;
    out<<"kairo-world2d 1\nsize 8 6\nmode "<<mode<<"\ncamera 4 4 50\nmotion 16 2 19\ncolors 1\n1 123456\ntiles 2\n. 0 0\n# 1 1\nsprites 1\nhero 2 2\n1111111111111111\n2222222222222222\nbackground\n";
    for(int y=0;y<6;++y)out<<"........\n";
    out<<"map\n########\n#......#\n#......#\n#......#\n#......#\n########\nactors "<<count<<"\n"<<actors;
    return out.str();
}
int main()
{
    auto w=Parse(Fixture());
    Check(w.Colors[1]==0x123456&&w.Sprites.at("hero").Frames.size()==2);
    Game g(w);
    g.Step({1,0,false});
    Check(g.Collected()==1&&g.Player().X==32&&!g.Won());
    for(int i=0;i<5&&!g.Won();++i)g.Step({1,0,false});
    Check(g.Won()&&g.Collected()==g.Total());
    g.Reset();Check(!g.Won()&&g.Collected()==0&&g.Player().X==16);
    Game hazard(Parse(Fixture("topdown","1 player 16 16 4 4 hero 0\n2 hazard 25 16 4 4 hero 0\n3 goal 60 16 4 4 hero 0\n")));
    hazard.Step({1,0,false});
    Check(hazard.Deaths()==1&&hazard.Player().X==16);
    Game solid(Parse(Fixture("topdown","1 player 16 16 4 4 hero 0\n2 solid 25 16 4 4 hero 0\n3 goal 60 32 4 4 hero 0\n")));
    solid.Step({1,0,false});Check(solid.Player().X==21);
    solid.Step({0,1,false});solid.Step({1,0,false});Check(solid.Player().X==37);
    Game platform(Parse(Fixture("platform","1 player 16 76 4 4 hero 0\n2 goal 80 76 4 4 hero 0\n",2)));
    platform.Step({0,0,true});Check(platform.Player().Y<76);
    for(int i=0;i<100;++i)platform.Step({0,0,true});
    Check(platform.Player().Y==76);
    auto malformed=Fixture();malformed.replace(malformed.find("1 player"),8,"-1 player");
    Reject([&]{(void)Parse(malformed);});
    Reject([&]{(void)Parse(Fixture()+"extra");});
    Reject([&]{(void)Parse(Fixture("wrong"));});
    Reject([&]{(void)Parse(Fixture("topdown","1 player 0 0 4 4 hero 0\n2 goal 60 16 4 4 hero 0\n",2));});
    Reject([&]{(void)Parse(Fixture("topdown","1 player 16 16 4 4 hero 0\n1 goal 60 16 4 4 hero 0\n",2));});
    std::mt19937 rng(2026);
    Game a(w),b(w);
    for(int i=0;i<10000;++i)
    {
        const Input input{int(rng()%3)-1,int(rng()%3)-1,bool(rng()%2)};
        a.Step(input);b.Step(input);
        Check(a.Player()==b.Player()&&a.Collected()==b.Collected()&&a.Won()==b.Won());
        Check(!MapBlocked(w,a.Player()));
        const auto camera=a.Camera();
        Check(camera.X>=0&&camera.Y>=0&&camera.X+camera.W<=w.Width*Tile&&camera.Y+camera.H<=w.Height*Tile);
        const unsigned width=1+rng()%1920,height=1+rng()%1080;
        const auto rects=View(a,width,height);
        Check(rects.size()<=MaxDrawRects);
        for(const auto& rect:rects)
        {
            const auto r=rect.Bounds;
            Check(r.X>=0&&r.Y>=0&&r.W>0&&r.H>0&&r.X+r.W<=int(width)&&r.Y+r.H<=int(height));
        }
        if(a.Won()||i%500==499){a.Reset();b.Reset();}
    }
    Check(View(a,0,100).empty());
    std::cout<<"10000 deterministic world ticks and clipped viewport checks passed\n";
}
