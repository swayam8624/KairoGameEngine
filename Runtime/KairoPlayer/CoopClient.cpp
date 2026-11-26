// Graphical client for the existing authoritative co-op service. Networking
// runs off the render thread, and the view consumes only accepted snapshots.
#include "RuntimeCoopWorker.hpp"
#include "RuntimeCoopView.hpp"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <charconv>
#include <iostream>
#include <thread>
#include <string_view>

import Kairo.Renderer;

namespace
{
    using namespace kairo::player::detail;
    using namespace kairo::renderer;
    std::uint16_t ParsePort(std::string_view text)
    {
        unsigned port=0;const auto r=std::from_chars(text.data(),text.data()+text.size(),port);
        if(r.ec!=std::errc{}||r.ptr!=text.data()+text.size()||port<1||port>65535)throw std::invalid_argument("Invalid port.");
        return static_cast<std::uint16_t>(port);
    }
    ViewportCapture Capture(RendererRuntime& renderer)
    {
        renderer.RequestViewportCapture();
        for(unsigned i=0;i<16;++i)
        {
            renderer.DrawFrame();
            if(auto image=renderer.TakeViewportCapture())
            {
                if(!image->IsVisuallyNonUniform())throw std::runtime_error("Uniform co-op capture.");
                return *image;
            }
        }
        throw std::runtime_error("Co-op capture unavailable.");
    }
    void Print(CoopState s)
    {std::cout<<"COOP TURN "<<s.Tick<<' '<<s.Actors[0].X<<' '<<s.Actors[0].Y<<' '<<s.Actors[1].X<<' '<<s.Actors[1].Y<<' '<<s.Won()<<std::endl;}
}
int main(int argc,char** argv)
{
    try
    {
        if(argc<2)throw std::invalid_argument("Use KairoCoopClient PORT [--renderer vulkan|metal] [--smoke].");
        const auto port=ParsePort(argv[1]);bool smoke=false;auto backend=GraphicsBackend::Automatic;
        for(int i=2;i<argc;++i)
        {
            const std::string_view arg=argv[i];
            if(arg=="--smoke")smoke=true;
            else if(arg=="--renderer"&&i+1<argc)backend=ParseGraphicsBackend(argv[++i]);
            else throw std::invalid_argument("Invalid co-op client option.");
        }
        // Join before initializing the GPU so both seats are accepted quickly.
        NetworkSocketSession sockets;CoopWorker worker(port);
        std::cout<<"COOP SLOT "<<worker.Slot()<<std::endl;
        RendererRuntime renderer({"Kairo Co-op",720,700,!smoke,backend});
        if(!renderer.SupportsViewportOverlay())throw std::runtime_error("Co-op requires native viewport overlay.");
        auto* window=renderer.NativeWindow().NativeHandle();
        const std::string title="Kairo Co-op P"+std::to_string(worker.Slot()+1)+" - arrows move, Space wait, Esc exit";
        glfwSetWindowTitle(window,title.c_str());
        auto [lastW,lastH]=renderer.NativeWindow().FramebufferExtent();
        if(!lastW||!lastH)throw std::runtime_error("Co-op framebuffer unavailable.");
        renderer.ResizeViewport(lastW,lastH);renderer.SubmitRenderScene(RenderScene{});
        const auto publish=[&](unsigned w,unsigned h)
        {
            const auto rects=CoopView(worker.Visible(),worker.Slot(),worker.Busy(),w,h);
            std::vector<ViewportOverlayRect> native;native.reserve(rects.size());
            for(const auto& r:rects)native.push_back({r.Box.X,r.Box.Y,static_cast<std::uint32_t>(r.Box.W),static_cast<std::uint32_t>(r.Box.H),r.Color});
            renderer.SetViewportOverlay(native);
        };
        publish(lastW,lastH);
        if(smoke)
        {
            const auto before=Capture(renderer);
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
            while(!worker.Visible().Won())
            {
                renderer.NativeWindow().PollEvents();
                if(worker.Poll())Print(worker.Visible());
                if(!worker.Visible().Won()&&!worker.Busy()&&!worker.Submit(1,0))
                    throw std::runtime_error("Co-op smoke turn rejected.");
                publish(lastW,lastH);renderer.DrawFrame();
                if(std::chrono::steady_clock::now()>=deadline||renderer.NativeWindow().ShouldClose())
                    throw std::runtime_error("Co-op smoke deadline or window close.");
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            const auto after=Capture(renderer);
            if(worker.Visible().Tick!=6||before.RGBA==after.RGBA)throw std::runtime_error("Co-op victory capture failed.");
            std::cout<<"COOP CAPTURE PASS"<<std::endl;renderer.SetViewportOverlay({});return 0;
        }
        constexpr std::array<int,5> keys{GLFW_KEY_LEFT,GLFW_KEY_RIGHT,GLFW_KEY_UP,GLFW_KEY_DOWN,GLFW_KEY_SPACE};
        constexpr std::array<CoopCell,5> delta{{{-1,0},{1,0},{0,-1},{0,1},{0,0}}};
        std::array<MenuEdge,5> edges;
        while(!renderer.NativeWindow().ShouldClose())
        {
            renderer.NativeWindow().PollEvents();
            // Always poll network completions even when the window is minimized.
            if(worker.Poll())Print(worker.Visible());
            const auto [w,h]=renderer.NativeWindow().FramebufferExtent();
            if(glfwGetKey(window,GLFW_KEY_ESCAPE)==GLFW_PRESS){renderer.NativeWindow().RequestClose();break;}
            if(glfwGetWindowAttrib(window,GLFW_FOCUSED)==GLFW_TRUE&&w&&h)
            {
                // Consume every edge, accepting at most one per frame. Held
                // or extra inputs during an outstanding turn are discarded.
                bool submitted=false;
                for(unsigned i=0;i<keys.size();++i)
                    if(edges[i].Press(glfwGetKey(window,keys[i])==GLFW_PRESS)&&!submitted)
                        submitted=worker.Submit(delta[i].X,delta[i].Y);
            }
            else for(auto& edge:edges)edge.Reset();
            if(!w||!h){std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;}
            if(w!=lastW||h!=lastH){renderer.ResizeViewport(w,h);lastW=w;lastH=h;}
            publish(w,h);renderer.DrawFrame();
        }
        renderer.SetViewportOverlay({});return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
