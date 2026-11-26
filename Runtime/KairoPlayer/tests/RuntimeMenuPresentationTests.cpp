#include "RuntimeMenuPresentation.hpp"
#include <iostream>
#include <limits>
#include <random>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()
{
    std::mt19937 rng(828);
    for(unsigned i=0;i<20000;++i)
    {
        const unsigned w=1+rng()%4000,h=1+rng()%3000;
        const double split=double(rng()%1001)*w/1000.;
        const auto a=QuantizeMenuBox(0,0,split,h,w,h),b=QuantizeMenuBox(split,0,w-split,h,w,h);
        Check(a.X==0&&a.X+a.W==b.X&&b.X+b.W==int(w),"Quantized edge mismatch");
        const std::array<MenuVisualButton,2> buttons{{{a,"LEFT"},{b,"RIGHT"}}};
        const auto x=int(rng()%w),y=int(rng()%h);
        Check(HitMenu(buttons,x,y)==(x<b.X?0u:1u),"Pixel hit mismatch");
        Check(!HitMenu(buttons,w,h),"Outside hit");
        for(const auto& r:BuildMenuOverlay(buttons,0,i%2))
            Check(r.Box.X>=0&&r.Box.Y>=0&&r.Box.W>0&&r.Box.H>0&&r.Box.X+r.Box.W<=int(w)&&r.Box.Y+r.Box.H<=int(h),"Overlay escaped viewport");
    }
    std::vector<MenuVisualButton> crowded;
    for(int i=0;i<64;++i)crowded.push_back({{(i%8)*512,(i/8)*512,512,512},std::string(64,'M')});
    Check(BuildMenuOverlay(crowded,63,true).size()<=MenuOverlayLimit,"Overlay exceeds renderer budget");
    const std::array<MenuVisualButton,1> longLabel{{{{0,0,48,30},std::string(64,'A')}}};
    Check(BuildMenuOverlay(longLabel,0,false).size()>2,"Long label disappeared instead of fitting prefix");
    bool rejected=false;try{(void)QuantizeMenuBox(std::numeric_limits<double>::infinity(),0,1,1,100,100);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Nonfinite layout accepted");
    std::cout<<"20000 adjacent layout/hit/overlay cases and bounded glyph fixtures passed\n";
}
