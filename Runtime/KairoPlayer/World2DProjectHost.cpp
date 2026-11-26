// Native host for authored 2D worlds; the portable fixed-step core and view
// remain independent of GLFW and graphics backend modules.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "RuntimeWorld2DView.hpp"
#include "RuntimeFrameClock.hpp"
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

import Kairo.Renderer;

namespace kairo::player::world2d
{
    int Run(Game game,const std::string& title,kairo::renderer::GraphicsBackend backend,bool smoke)
    {
        using namespace kairo::renderer;
        RendererRuntime renderer({title+" - Kairo 2D",1280u,720u,!smoke,backend});
        if(!renderer.SupportsViewportOverlay())
            throw std::runtime_error("World2D currently requires a Metal or Vulkan viewport overlay.");
        const auto [width,height]=renderer.NativeWindow().FramebufferExtent();
        if(!width||!height)throw std::runtime_error("World2D framebuffer unavailable.");
        renderer.ResizeViewport(width,height);
        renderer.SubmitRenderScene(RenderScene{});
        const auto publish=[&](unsigned w,unsigned h)
        {
            const auto rects=View(game,w,h);
            std::vector<ViewportOverlayRect> native;
            native.reserve(rects.size());
            for(const auto& rect:rects)
            {
                const auto b=rect.Bounds;const auto rgb=rect.RGB;
                native.push_back({b.X,b.Y,static_cast<unsigned>(b.W),static_cast<unsigned>(b.H),
                    {float((rgb>>16)&255u)/255.f,float((rgb>>8)&255u)/255.f,float(rgb&255u)/255.f}});
            }
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
                    if(!capture->IsVisuallyNonUniform())throw std::runtime_error("World2D capture is uniform.");
                    std::cout<<"Native World2D viewport smoke passed\n";
                    return 0;
                }
            }
            throw std::runtime_error("World2D capture unavailable.");
        }
        auto* window=renderer.NativeWindow().NativeHandle();
        kairo::player::detail::RuntimeFrameClock clock;
        const auto start=std::chrono::steady_clock::now();
        unsigned lastW=width,lastH=height;
        double accumulator=0;
        bool previousJump=false,jumpQueued=false,previousReset=false;
        while(!renderer.NativeWindow().ShouldClose())
        {
            renderer.NativeWindow().PollEvents();
            const auto [w,h]=renderer.NativeWindow().FramebufferExtent();
            const bool active=w&&h&&glfwGetWindowAttrib(window,GLFW_FOCUSED)==GLFW_TRUE;
            const auto key=[&](int k){return glfwGetKey(window,k)==GLFW_PRESS;};
            const bool jump=key(GLFW_KEY_SPACE),reset=key(GLFW_KEY_R);
            const auto dt=clock.Sample(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(),active);
            if(!active)
            {
                accumulator=0;jumpQueued=false;previousJump=jump;previousReset=reset;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;
            }
            if(key(GLFW_KEY_ESCAPE)){renderer.NativeWindow().RequestClose();break;}
            if(reset&&!previousReset){game.Reset();accumulator=0;jumpQueued=false;}
            previousReset=reset;
            if(w!=lastW||h!=lastH){renderer.ResizeViewport(w,h);lastW=w;lastH=h;}
            if(jump&&!previousJump)jumpQueued=true;
            previousJump=jump;
            accumulator+=dt;
            while(accumulator>=1./60.)
            {
                game.Step({int(key(GLFW_KEY_D)||key(GLFW_KEY_RIGHT))-int(key(GLFW_KEY_A)||key(GLFW_KEY_LEFT)),
                    int(key(GLFW_KEY_S)||key(GLFW_KEY_DOWN))-int(key(GLFW_KEY_W)||key(GLFW_KEY_UP)),jump||jumpQueued});
                jumpQueued=false;accumulator-=1./60.;
            }
            publish(w,h);renderer.DrawFrame();
        }
        renderer.SetViewportOverlay({});
        return 0;
    }
}
