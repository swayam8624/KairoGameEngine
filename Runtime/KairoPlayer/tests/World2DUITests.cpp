#include "../World2DUI.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
using namespace kairo::player::ui2d;
void Assert(bool ok){if(!ok)throw std::runtime_error("UI regression");}
int main(int argc,char** argv)
{
    Assert(argc==2);std::ifstream in(argv[1]);std::string source{std::istreambuf_iterator<char>(in),{}};
    const auto doc=Parse(source);Assert(doc.Widgets.size()==15);
    State state{3,5,2,1,2,"KAIRO ADVENTURE"};
    Assert(Text(doc.Widgets[1],state)=="COINS 3/5");
    Assert(Text(doc.Widgets[3],state)=="LEVEL 1/2");
    const auto pause=Layout(doc.Widgets[4],1280,720);
    const int px=pause.X+pause.W/2,py=pause.Y+pause.H/2;
    Pointer pointer;
    const auto update=[&](Screen page,int x,int y,bool down,bool active=true){return pointer.Update(doc,page,1280,720,x,y,down,active);};
    Assert(update(Screen::Play,px,py,true)==Action::None);
    Assert(update(Screen::Play,px,py,false)==Action::None);
    Assert(update(Screen::Play,px,py,true)==Action::None&&pointer.Pressed()==4);
    Assert(update(Screen::Play,px,py,false)==Action::Pause);
    const auto resume=Layout(doc.Widgets[8],1280,720);
    const int rx=resume.X+resume.W/2,ry=resume.Y+resume.H/2;
    Assert(update(Screen::Pause,rx,ry,true)==Action::None);
    Assert(update(Screen::Pause,rx,ry,false)==Action::None);
    update(Screen::Pause,rx,ry,true);
    Assert(update(Screen::Pause,0,0,false)==Action::None);
    update(Screen::Pause,rx,ry,true);update(Screen::Pause,rx,ry,true,false);
    Assert(update(Screen::Pause,rx,ry,false)==Action::None);
    update(Screen::Pause,rx,ry,true);Assert(update(Screen::Pause,rx,ry,false)==Action::Resume);
    auto overlapping=doc;auto duplicate=doc.Widgets[8];duplicate.Command=Action::Quit;overlapping.Widgets.push_back(duplicate);
    Assert(Hit(overlapping,Screen::Pause,1280,720,rx,ry)==overlapping.Widgets.size()-1);
    std::vector<Rect> rects;Paint(doc,Screen::Play,state,pointer,1280,720,rects);
    const auto bar=Layout(doc.Widgets[2],1280,720);
    Assert(std::any_of(rects.begin(),rects.end(),[&](const Rect& r){return r.RGB==doc.Widgets[2].Foreground&&r.Bounds==Box{bar.X,bar.Y,int(bar.W*.6),bar.H};}));
    const auto reject=[&](const std::string& bytes){bool failed=false;try{(void)Parse(bytes);}catch(const std::invalid_argument&){failed=true;}Assert(failed);};
    reject(source+"junk");reject("kairo-ui2d 1\nwidgets 0\n");
    auto bad=source;bad.replace(bad.find("resume pause button"),19,"resume play button ");reject(bad);
    bad=source;bad.replace(bad.find("pause play button"),17,"hud play button  ");reject(bad);
    std::mt19937 rng(2026);std::uint64_t records=0;
    for(unsigned i=0;i<10000;++i)
    {
        const unsigned w=1+rng()%3840,h=1+rng()%2160;
        const auto page=static_cast<Screen>(rng()%3);state.Coins=rng()%6;
        Paint(doc,page,state,Pointer{},w,h,rects);Assert(rects.size()<=MaxRects);
        for(const auto& r:rects)Assert(r.Bounds.X>=0&&r.Bounds.Y>=0&&r.Bounds.W>0&&r.Bounds.H>0&&r.Bounds.X+r.Bounds.W<=int(w)&&r.Bounds.Y+r.Bounds.H<=int(h));
        records+=rects.size();
        for(std::size_t j=0;j<doc.Widgets.size();++j)
        {
            const auto& widget=doc.Widgets[j];if(widget.Page!=page||widget.Type!=Kind::Button)continue;
            const auto b=Clip(Layout(widget,w,h),w,h);
            if(b.W&&b.H)Assert(Hit(doc,page,w,h,b.X+b.W/2,b.Y+b.H/2).has_value());
        }
    }
    std::cout<<"10000 responsive layouts and paint checks, "<<records<<" clipped rectangles; pointer capture, page transitions, counters, progress and malformed docs passed\n";
}
