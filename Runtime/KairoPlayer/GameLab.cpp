// Native presentation for the existing portable game rules. This lab does
// not duplicate physics, quest state or pathfinding inside the renderer.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "RuntimeGridCanvas.hpp"
#include "RuntimePlatformer.hpp"
#include "RuntimeStrategy.hpp"
#include "RuntimeQuestSave.hpp"
#include "RuntimeMenuFocus.hpp"
#include "RuntimeFrameClock.hpp"
#include <array>
#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

import Kairo.Renderer;

namespace
{
    using namespace kairo::player::detail;
    using namespace kairo::renderer;
    using Color=std::array<float,3>;
    enum class Mode{Platformer,Strategy,RPG};
    using Rects=std::vector<ViewportOverlayRect>;

    // Convert only at the presentation boundary. Painter order: background,
    // terrain, collectibles, players/cursor, then HUD. Skip empty rectangles.
    void Add(Rects& rects,GridPixelRect r,Color color)
    {
        if(r.W>0&&r.H>0)rects.push_back({r.X,r.Y,static_cast<std::uint32_t>(r.W),static_cast<std::uint32_t>(r.H),color});
    }
    void Text(Rects& rects,const RuntimeGridCanvas& canvas,int x,int y,std::string_view value,Color color)
    {
        for(char c:value)
        {
            const auto glyph=MenuGlyph(c);
            for(unsigned row=0;row<7;++row)for(unsigned col=0;col<5;++col)
                if(glyph[row]&(1u<<(4-col)))Add(rects,canvas.Rect(x+int(col),y+int(row)),color);
            x+=6;
        }
    }
    Rects PlatformerView(const RuntimePlatformer& game,unsigned w,unsigned h)
    {
        const auto& rows=game.Rows();
        RuntimeGridCanvas canvas(w,h,int(rows.front().size())*16,int(rows.size())*16+12);
        Rects rects;Add(rects,canvas.Rect(0,0,256,108),{.035f,.055f,.08f});
        for(int y=0;y<int(rows.size());++y)for(int x=0;x<int(rows[y].size());++x)
        {
            switch(rows[y][x])
            {
                case '#':Add(rects,canvas.Rect(x*16,y*16,16,16),{.23f,.31f,.42f});break;
                case 'H':Add(rects,canvas.Rect(x*16+1,y*16+3,14,13),{.95f,.15f,.12f});break;
                case 'C':Add(rects,canvas.Rect(x*16+5,y*16+5,6,6),{1,.75f,.15f});break;
                case 'G':Add(rects,canvas.Rect(x*16+2,y*16,12,16),{.12f,.65f,.35f});break;
                default:break;
            }
        }
        const auto& s=game.Current();
        Add(rects,canvas.Rect(s.X,s.Y,RuntimePlatformer::Width,RuntimePlatformer::Height),{.2f,.75f,1});
        const auto label=s.Won?std::string("LEVEL COMPLETE"):("COINS "+std::to_string(s.Coins)+"/"+std::to_string(game.CoinCount()));
        Text(rects,canvas,4,98,label,{1,1,1});return rects;
    }
    Rects StrategyView(const RuntimeStrategy& game,std::size_t selected,StrategyCell cursor,unsigned w,unsigned h)
    {
        const auto& rows=game.Rows();
        RuntimeGridCanvas canvas(w,h,160,108);Rects rects;
        Add(rects,canvas.Rect(0,0,160,108),{.035f,.055f,.08f});
        for(int y=0;y<int(rows.size());++y)for(int x=0;x<int(rows[y].size());++x)
        {
            const auto c=rows[y][x];
            const Color color=c=='#'?Color{.22f,.27f,.33f}:c=='B'?Color{.1f,.55f,.8f}:
                c=='R'&&game.Stock({x,y})?Color{.8f,.55f,.1f}:Color{.12f,.17f,.15f};
            Add(rects,canvas.Rect(x*16,y*16,15,15),color);
        }
        for(std::size_t i=0;i<game.Units().size();++i)
        {
            const auto& u=game.Units()[i];const auto p=u.Position;
            Add(rects,canvas.Rect(p.X*16+2,p.Y*16+2,12,12),i==selected?Color{.3f,1,.6f}:Color{.7f,.85f,.9f});
            if(u.Cargo)Add(rects,canvas.Rect(p.X*16+3,p.Y*16+3,int(u.Cargo)*3,3),{1,.7f,.1f});
        }
        const int x=cursor.X*16,y=cursor.Y*16;
        Add(rects,canvas.Rect(x,y,16,1),{1,1,1});Add(rects,canvas.Rect(x,y+15,16,1),{1,1,1});
        Add(rects,canvas.Rect(x,y,1,16),{1,1,1});Add(rects,canvas.Rect(x+15,y,1,16),{1,1,1});
        const auto label=game.Won()?std::string("OBJECTIVE COMPLETE"):("STORED "+std::to_string(game.Stored())+"/"+std::to_string(game.Goal()));
        Text(rects,canvas,3,98,label,{1,1,1});return rects;
    }
    // One authored RPG fixture using the same objective/dialogue contracts
    // as QuestDialogueDemo. Save/load uses the existing versioned envelope.
    struct QuestLab
    {
        RuntimeQuestBook Book;
        RuntimeDialogue Dialogue;
        static RuntimeQuestBook Definitions()
        {
            RuntimeQuestBook b;b.Add({"accept",{}});b.Add({"find",{"accept"}});b.Add({"return",{"find"}});return b;
        }
        QuestLab():Book(Definitions()),Dialogue({
            {"village","THE KEEPER NEEDS A CRYSTAL",{{"ACCEPT QUEST","cave",{},"accept"},{"RETURN CRYSTAL","","find","return"}}},
            {"cave","A CRYSTAL GLOWS",{{"PICK UP CRYSTAL","village","accept","find"},{"GO BACK","village",{},{}}}}
        },"village",Book){}
    };
    Rects QuestView(const QuestLab& q,unsigned w,unsigned h)
    {
        RuntimeGridCanvas canvas(w,h,180,100);Rects rects;
        Add(rects,canvas.Rect(0,0,180,100),{.055f,.07f,.13f});
        if(const auto* node=q.Dialogue.Current())
        {
            Text(rects,canvas,4,12,node->Text,{1,1,1});
            for(std::size_t i=0;i<node->Choices.size();++i)
            {
                const bool enabled=q.Dialogue.Available(i,q.Book);
                Add(rects,canvas.Rect(4,28+int(i)*20,172,16),enabled?Color{.12f,.35f,.3f}:Color{.24f,.24f,.24f});
                Text(rects,canvas,7,32+int(i)*20,std::to_string(i+1)+" "+node->Choices[i].Label,enabled?Color{1,1,1}:Color{.55f,.55f,.55f});
            }
            Text(rects,canvas,4,83,"1 2 CHOOSE S SAVE L LOAD",{.75f,.85f,1});
        }
        else Text(rects,canvas,4,40,q.Book.Won()?"QUEST COMPLETE":"CONVERSATION ENDED",{.3f,1,.6f});
        return rects;
    }
    // Capture only the actual renderer output, never a synthetic CPU image.
    ViewportCapture Capture(RendererRuntime& renderer)
    {
        renderer.RequestViewportCapture();
        for(unsigned i=0;i<16;++i)
        {
            renderer.DrawFrame();
            if(auto capture=renderer.TakeViewportCapture())
            {
                if(!capture->IsVisuallyNonUniform())throw std::runtime_error("Game lab capture uniform.");
                return *capture;
            }
        }
        throw std::runtime_error("Native capture unavailable.");
    }
}

int main(int argc,char** argv)
{
    try
    {
        Mode mode=Mode::Platformer;bool smoke=false;
        GraphicsBackend backend=GraphicsBackend::Automatic;
        std::filesystem::path save="quest-lab.ksave";
        for(int i=1;i<argc;++i)
        {
            const std::string_view arg=argv[i];
            if(arg=="--strategy")mode=Mode::Strategy;
            else if(arg=="--rpg")mode=Mode::RPG;
            else if(arg=="--smoke")smoke=true;
            else if(arg=="--renderer"&&i+1<argc)backend=ParseGraphicsBackend(argv[++i]);
            else if(arg=="--save"&&i+1<argc)save=argv[++i];
            else throw std::invalid_argument("Usage: KairoGameLab [--strategy|--rpg] [--renderer vulkan|metal] [--smoke] [--save path]");
        }
        RendererRuntime renderer({"Kairo Game Lab",960,540,!smoke,backend});
        if(!renderer.SupportsViewportOverlay())throw std::runtime_error("Game lab requires Vulkan or Metal overlay support.");
        renderer.ResizeViewport(960,540);renderer.SubmitRenderScene(RenderScene{});
        RuntimePlatformer platformer(PlatformerDemoLevel());
        RuntimeStrategy strategy(StrategyDemoLevel());QuestLab quest;
        std::size_t selected=0;StrategyCell cursor{2,1};
        const auto publish=[&](unsigned w,unsigned h)
        {
            const auto rects=mode==Mode::Platformer?PlatformerView(platformer,w,h):
                mode==Mode::Strategy?StrategyView(strategy,selected,cursor,w,h):QuestView(quest,w,h);
            renderer.SetViewportOverlay(rects);
        };
        const auto won=[&](){return mode==Mode::Platformer?platformer.Current().Won:mode==Mode::Strategy?strategy.Won():quest.Book.Won();};
        publish(960,540);
        if(smoke)
        {
            const auto before=Capture(renderer);
            if(mode==Mode::Platformer)
            {
                for(int i=0;i<31;++i)platformer.Step({1,false});
                for(int i=0;i<17;++i)platformer.Step({1,true});
                for(int i=0;i<80;++i)platformer.Step({1,false});
            }
            else if(mode==Mode::Strategy)
            {
                strategy.Move(0,{2,1});strategy.Gather(1,{8,1});
                for(int i=0;i<100;++i)strategy.Tick();
            }
            else
            {
                if(!quest.Dialogue.Choose(0,quest.Book)||!quest.Dialogue.Choose(0,quest.Book)||!quest.Dialogue.Choose(1,quest.Book))
                    throw std::runtime_error("Quest route rejected.");
            }
            publish(960,540);const auto after=Capture(renderer);
            if(!won()||before.RGBA==after.RGBA)throw std::runtime_error("Game lab did not reach visible winning state.");
            std::cout<<"Native game lab objective and image-change gate passed\n";return 0;
        }
        auto* window=renderer.NativeWindow().NativeHandle();
        glfwSetWindowTitle(window,mode==Mode::Platformer?"Platformer: A/D Space, R reset, Esc":
            mode==Mode::Strategy?"Strategy: 1/2 select, arrows cursor, M move, G gather, Esc":"RPG: 1/2 choose, S save, L load, Esc");
        RuntimeFrameClock clock;const auto start=std::chrono::steady_clock::now();double accumulator=0;
        std::array<MenuEdge,11> edges;
        std::uint32_t lastW=960,lastH=540;
        while(!renderer.NativeWindow().ShouldClose())
        {
            renderer.NativeWindow().PollEvents();
            const auto [w,h]=renderer.NativeWindow().FramebufferExtent();
            const bool active=w&&h&&glfwGetWindowAttrib(window,GLFW_FOCUSED)==GLFW_TRUE;
            const double dt=clock.Sample(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(),active);
            if(!active)
            {
                for(auto& edge:edges)edge.Reset();
                accumulator=0;std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;
            }
            if(w!=lastW||h!=lastH){renderer.ResizeViewport(w,h);lastW=w;lastH=h;}
            const auto key=[&](int k){return glfwGetKey(window,k)==GLFW_PRESS;};
            const auto press=[&](unsigned i,int k){return edges[i].Press(key(k));};
            if(key(GLFW_KEY_ESCAPE)){renderer.NativeWindow().RequestClose();break;}
            if(mode==Mode::Platformer)
            {
                if(press(0,GLFW_KEY_R))platformer.Reset();
                accumulator+=dt;
                while(accumulator>=1./60.)
                {
                    platformer.Step({int(key(GLFW_KEY_D))-int(key(GLFW_KEY_A)),key(GLFW_KEY_SPACE)});
                    accumulator-=1./60.;
                }
            }
            else if(mode==Mode::Strategy)
            {
                if(press(0,GLFW_KEY_1))selected=0;
                if(press(1,GLFW_KEY_2))selected=1;
                if(press(2,GLFW_KEY_LEFT))cursor.X=std::max(0,cursor.X-1);
                if(press(3,GLFW_KEY_RIGHT))cursor.X=std::min(9,cursor.X+1);
                if(press(4,GLFW_KEY_UP))cursor.Y=std::max(0,cursor.Y-1);
                if(press(5,GLFW_KEY_DOWN))cursor.Y=std::min(5,cursor.Y+1);
                if(press(6,GLFW_KEY_M)&&strategy.Rows()[cursor.Y][cursor.X]=='.')strategy.Move(selected,cursor);
                if(press(7,GLFW_KEY_G)&&strategy.Rows()[cursor.Y][cursor.X]=='R'&&strategy.Stock(cursor))strategy.Gather(selected,cursor);
                accumulator+=dt;while(accumulator>=.15){strategy.Tick();accumulator-=.15;}
            }
            else
            {
                if(press(0,GLFW_KEY_1))(void)quest.Dialogue.Choose(0,quest.Book);
                if(press(1,GLFW_KEY_2))(void)quest.Dialogue.Choose(1,quest.Book);
                try
                {
                    if(press(2,GLFW_KEY_S))SaveQuest(save,quest.Book,quest.Dialogue);
                    if(press(3,GLFW_KEY_L))LoadQuest(save,quest.Book,quest.Dialogue);
                }
                catch(const std::exception& e){std::cerr<<e.what()<<'\n';}
            }
            publish(w,h);renderer.DrawFrame();
        }
        renderer.SetViewportOverlay({});return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
