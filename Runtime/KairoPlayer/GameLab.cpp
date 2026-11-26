// Native presentation for the existing portable game rules. This lab does
// not duplicate physics, quest state or pathfinding inside the renderer.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "RuntimeGameLabView.hpp"
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
    enum class Mode{Platformer,Strategy,RPG,Animation};
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
            else if(arg=="--animation")mode=Mode::Animation;
            else if(arg=="--smoke")smoke=true;
            else if(arg=="--renderer"&&i+1<argc)backend=ParseGraphicsBackend(argv[++i]);
            else if(arg=="--save"&&i+1<argc)save=argv[++i];
            else throw std::invalid_argument("Usage: KairoGameLab [--strategy|--rpg|--animation] [--renderer vulkan|metal] [--smoke] [--save path]");
        }
        RendererRuntime renderer({"Kairo Game Lab",960,540,!smoke,backend});
        if(!renderer.SupportsViewportOverlay())throw std::runtime_error("Game lab requires Vulkan or Metal overlay support.");
        // Logical window size and physical target may differ on high-DPI hosts.
        const auto [initialWidth,initialHeight]=renderer.NativeWindow().FramebufferExtent();
        if(!initialWidth||!initialHeight)throw std::runtime_error("Game lab framebuffer unavailable.");
        renderer.ResizeViewport(initialWidth,initialHeight);renderer.SubmitRenderScene(RenderScene{});
        RuntimePlatformer platformer(PlatformerDemoLevel());
        RuntimeStrategy strategy(StrategyDemoLevel());QuestLab quest;
        std::size_t selected=0;StrategyCell cursor{2,1};double phase=0.;
        const auto publish=[&](unsigned w,unsigned h)
        {
            const auto rects=mode==Mode::Platformer?PlatformerView(platformer,w,h):
                mode==Mode::Strategy?StrategyView(strategy,selected,cursor,w,h):
                mode==Mode::Animation?AnimationView(phase,w,h):QuestView(quest,w,h);
            // The CPU regression exercises these exact rectangles. The only
            // backend-specific step here is translation to the native API.
            std::vector<ViewportOverlayRect> native;native.reserve(rects.size());
            for(const auto& r:rects)native.push_back({r.Box.X,r.Box.Y,
                static_cast<std::uint32_t>(r.Box.W),static_cast<std::uint32_t>(r.Box.H),r.Tint});
            renderer.SetViewportOverlay(native);
        };
        const auto won=[&](){return mode==Mode::Platformer?platformer.Current().Won:mode==Mode::Strategy?strategy.Won():mode==Mode::Animation?SamplePoseLab(phase).Reached:quest.Book().Won();};
        publish(initialWidth,initialHeight);
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
            else if(mode==Mode::Animation)
            {
                // Phase zero is outside reach; this phase is reachable.
                if(SamplePoseLab(phase).Reached)throw std::runtime_error("Animation fixture must start clamped.");
                phase=1.6;
            }
            else
            {
                if(!quest.Choose(0)||!quest.Choose(0)||!quest.Choose(1))
                    throw std::runtime_error("Quest route rejected.");
                if(quest.Bag().Count("crystal")!=0||quest.Bag().Count("gold")!=10)
                    throw std::runtime_error("Quest inventory reward mismatch.");
            }
            publish(initialWidth,initialHeight);const auto after=Capture(renderer);
            if(!won()||before.RGBA==after.RGBA)throw std::runtime_error("Game lab did not reach visible winning state.");
            std::cout<<"Native game lab objective and image-change gate passed\n";return 0;
        }
        auto* window=renderer.NativeWindow().NativeHandle();
        glfwSetWindowTitle(window,mode==Mode::Platformer?"Platformer: A/D Space, R reset, Esc":
            mode==Mode::Strategy?"Strategy: 1/2 select, arrows cursor, M move, G gather, Esc":
            mode==Mode::Animation?"Animation: source, retarget, IK, Space pause, R reset, Esc":"RPG: 1/2 choose, S save, L load, Esc");
        RuntimeFrameClock clock;const auto start=std::chrono::steady_clock::now();double accumulator=0;
        std::array<MenuEdge,11> edges;bool paused=false;
        std::uint32_t lastW=initialWidth,lastH=initialHeight;
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
            else if(mode==Mode::Animation)
            {
                if(press(0,GLFW_KEY_SPACE))paused=!paused;
                if(press(1,GLFW_KEY_R))phase=0.;
                if(!paused)phase=std::fmod(phase+dt,6.283185307179586);
            }
            else
            {
                if(press(0,GLFW_KEY_1))(void)quest.Choose(0);
                if(press(1,GLFW_KEY_2))(void)quest.Choose(1);
                try
                {
                    if(press(2,GLFW_KEY_S))quest.Save(save);
                    if(press(3,GLFW_KEY_L))quest.Load(save);
                }
                catch(const std::exception& e){std::cerr<<e.what()<<'\n';}
            }
            publish(w,h);renderer.DrawFrame();
        }
        renderer.SetViewportOverlay({});return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
