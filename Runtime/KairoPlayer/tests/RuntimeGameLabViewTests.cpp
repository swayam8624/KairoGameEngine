#include "RuntimeGameLabView.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void Bounds(const GameLabRects& rects,unsigned w,unsigned h)
{
    Check(rects.size()<=4096,"Fixture exceeds renderer overlay limit");
    for(const auto& r:rects)
    {
        const auto b=r.Box;
        Check(b.X>=0&&b.Y>=0&&b.W>0&&b.H>0&&b.X+b.W<=int(w)&&b.Y+b.H<=int(h),"Rectangle outside viewport");
        for(float c:r.Tint)Check(c>=0&&c<=1,"Color outside normalized range");
    }
}
// CPU painter oracle is deliberately independent of the GPU. It verifies
// expected geometry changes, not native readback, color conversion or timing.
static std::vector<GameLabColor> Paint(const GameLabRects& rects,unsigned w,unsigned h)
{
    Bounds(rects,w,h);std::vector<GameLabColor> pixels(w*h);
    for(const auto& r:rects)for(int y=r.Box.Y;y<r.Box.Y+r.Box.H;++y)
        for(int x=r.Box.X;x<r.Box.X+r.Box.W;++x)pixels[unsigned(y)*w+unsigned(x)]=r.Tint;
    return pixels;
}
int main()
{
    RuntimePlatformer platformer(PlatformerDemoLevel());RuntimeStrategy strategy(StrategyDemoLevel());QuestLab quest;
    const auto p0=Paint(PlatformerView(platformer,768,324),768,324);
    const auto s0=Paint(StrategyView(strategy,0,{2,1},768,324),768,324);
    const auto q0=Paint(QuestView(quest,768,324),768,324);
    for(int i=0;i<31;++i)platformer.Step({1,false});
    for(int i=0;i<17;++i)platformer.Step({1,true});
    for(int i=0;i<80;++i)platformer.Step({1,false});
    strategy.Move(0,{2,1});strategy.Gather(1,{8,1});for(int i=0;i<100;++i)strategy.Tick();
    Check(!quest.Dialogue.Choose(1,quest.Book),"Locked choice accepted");
    Check(quest.Dialogue.Choose(0,quest.Book)&&quest.Dialogue.Choose(0,quest.Book)&&quest.Dialogue.Choose(1,quest.Book),"Quest route rejected");
    Check(platformer.Current().Won&&strategy.Won()&&quest.Book.Won(),"Objective not completed");
    Check(p0!=Paint(PlatformerView(platformer,768,324),768,324),"Platformer winning image unchanged");
    Check(s0!=Paint(StrategyView(strategy,0,{2,1},768,324),768,324),"Strategy winning image unchanged");
    Check(q0!=Paint(QuestView(quest,768,324),768,324),"Quest winning image unchanged");
    RuntimePlatformer narrow({"####","#SG#","####"});Bounds(PlatformerView(narrow,320,240),320,240);
    std::mt19937 rng(191);
    for(unsigned i=0;i<5000;++i)
    {
        const unsigned w=1+rng()%4000,h=1+rng()%3000;
        Bounds(PlatformerView(platformer,w,h),w,h);
        Bounds(StrategyView(strategy,i%2,{int(i%10),int(i%6)},w,h),w,h);
        Bounds(QuestView(quest,w,h),w,h);
    }
    std::cout<<"Three complete CPU painter trajectories and 15000 viewport cases passed\n";
}
