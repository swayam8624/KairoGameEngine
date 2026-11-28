// Native 2D project host. Simulation and rendering geometry are shared with
// the tested Platformer lab, but the level comes from authored project bytes.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "RuntimePlatformerProject.hpp"
#include "RuntimeGameLabView.hpp"
#include "RuntimeFrameClock.hpp"
#include <chrono>
#include <iostream>
#include <thread>

import Kairo.Renderer;

namespace kairo::player::detail
{
    int RunPlatformerProject(RuntimePlatformer game,const std::string& title,
        kairo::renderer::GraphicsBackend backend,bool smoke)
    {
        using namespace kairo::renderer;
        RendererRuntime renderer({title+" - Kairo 2D",1280u,720u,!smoke,backend});
        if(!renderer.SupportsViewportOverlay())
            throw std::runtime_error("2D platformer projects require Metal or Vulkan overlay support.");
        const auto [width,height]=renderer.NativeWindow().FramebufferExtent();
        if(!width||!height)throw std::runtime_error("2D project framebuffer unavailable.");
        renderer.ResizeViewport(width,height);
        renderer.SubmitRenderScene(RenderScene{});
        const auto publish=[&](unsigned w,unsigned h)
        {
            const auto rects=PlatformerView(game,w,h);
            std::vector<ViewportOverlayRect> native;
            native.reserve(rects.size());
            for(const auto& r:rects)native.push_back({r.Box.X,r.Box.Y,
                static_cast<std::uint32_t>(r.Box.W),static_cast<std::uint32_t>(r.Box.H),r.Tint});
            renderer.SetViewportOverlay(native);
        };
        publish(width,height);
        if(smoke)
        {
            renderer.RequestViewportCapture();
            for(unsigned i=0;i<16;++i)
            {
                renderer.NativeWindow().PollEvents();
                renderer.DrawFrame();
                if(auto capture=renderer.TakeViewportCapture())
                {
                    if(!capture->IsVisuallyNonUniform())
                        throw std::runtime_error("2D project capture is uniform.");
                    std::cout<<"Native 2D project viewport smoke passed\n";
                    return 0;
                }
            }
            throw std::runtime_error("2D project capture unavailable.");
        }
        auto* window=renderer.NativeWindow().NativeHandle();
        RuntimeFrameClock clock;
        const auto start=std::chrono::steady_clock::now();
        double accumulator=0;
        MenuEdge reset;
        unsigned lastW=width,lastH=height;
        bool jumpQueued=false,previousJump=false;
        while(!renderer.NativeWindow().ShouldClose())
        {
            renderer.NativeWindow().PollEvents();
            const auto [w,h]=renderer.NativeWindow().FramebufferExtent();
            const bool active=w&&h&&glfwGetWindowAttrib(window,GLFW_FOCUSED)==GLFW_TRUE;
            const double dt=clock.Sample(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(),active);
            const auto key=[&](int k){return glfwGetKey(window,k)==GLFW_PRESS;};
            if(!active)
            {
                accumulator=0;jumpQueued=false;previousJump=key(GLFW_KEY_SPACE);
                (void)reset.Press(key(GLFW_KEY_R));
                std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;
            }
            if(key(GLFW_KEY_ESCAPE)){renderer.NativeWindow().RequestClose();break;}
            if(reset.Press(key(GLFW_KEY_R))){game.Reset();accumulator=0;jumpQueued=false;}
            if(w!=lastW||h!=lastH){renderer.ResizeViewport(w,h);lastW=w;lastH=h;}
            const bool jump=key(GLFW_KEY_SPACE);
            if(jump&&!previousJump)jumpQueued=true;
            previousJump=jump;
            accumulator+=dt;
            while(accumulator>=1./60.)
            {
                const bool left=key(GLFW_KEY_A)||key(GLFW_KEY_LEFT);
                const bool right=key(GLFW_KEY_D)||key(GLFW_KEY_RIGHT);
                game.Step({int(right)-int(left),jump||jumpQueued});
                jumpQueued=false;accumulator-=1./60.;
            }
            publish(w,h);renderer.DrawFrame();
        }
        renderer.SetViewportOverlay({});
        return 0;
    }
}
