#include "../TerrainScreenError.hpp"
#include <iostream>
#include <random>

namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
}
int main()
{
    const auto simple=PrepareTerrainView({{0,0,0},{0,0,-1},{0,1,0},1.0,1000,0.1});
    const auto bound=TerrainProjectedErrorPixels(simple,{-2,-2,-20},{2,2,-10},1);
    Check(std::abs(bound-simple.FocalPixels/10)<1e-10,"Front-facing projected bound incorrect");
    Check(std::isinf(TerrainProjectedErrorPixels(simple,{-2,-2,-20},{2,2,0},1)),"Near-plane crossing did not fail closed");
    Check(TerrainProjectedErrorPixels(simple,{-2,-2,-20},{2,2,0},0)==0,"Exact terrain has nonzero error");
    std::mt19937_64 rng(87621);
    std::uniform_real_distribution<double> unit(0,1);
    std::uint64_t samples=0;
    for(unsigned scene=0;scene<1000;++scene)
    {
        TerrainPerspectiveView input;
        input.Position={unit(rng)*10,25+unit(rng)*20,40+unit(rng)*30};
        input.Forward={-input.Position[0],-input.Position[1],-input.Position[2]};
        input.VerticalFovRadians=.5+unit(rng)*1.5;
        input.ViewportHeight=360+unit(rng)*1800;
        const auto view=PrepareTerrainView(input);
        const ScreenVec lo{-8,-4,-8},hi{8,12,8};
        const double error=.01+unit(rng)*8;
        const double ceiling=TerrainProjectedErrorPixels(view,lo,hi,error);
        Check(std::isfinite(ceiling),"Test cell behind near plane");
        const auto project=[&](ScreenVec p){
            for(unsigned j=0;j<3;++j)p[j]-=view.Position[j];
            const double depth=ScreenDot(p,view.Forward);
            return std::array{view.FocalPixels*ScreenDot(p,view.Right)/depth,
                view.FocalPixels*ScreenDot(p,view.Up)/depth};
        };
        for(unsigned i=0;i<1000;++i)
        {
            ScreenVec a{lo[0]+16*unit(rng),lo[1]+16*unit(rng),lo[2]+16*unit(rng)};
            auto b=a;b[1]=std::clamp(a[1]+(2*unit(rng)-1)*error,lo[1],hi[1]);
            const auto p=project(a),q=project(b);
            Check(std::hypot(p[0]-q[0],p[1]-q[1])<=ceiling+1e-8,"Projected sample exceeds bound");
            ++samples;
        }
        auto translated=input;
        for(auto& v:translated.Position)v+=1e9;
        const auto large=TerrainProjectedErrorPixels(PrepareTerrainView(translated),
            {1e9-8,1e9-4,1e9-8},{1e9+8,1e9+12,1e9+8},error);
        Check(std::abs(large-ceiling)<=1e-5*std::max(1.0,ceiling),"World translation altered estimate");
    }
    std::cout<<samples<<" perspective samples, near-plane fallback and billion-meter translation passed\n";
}
