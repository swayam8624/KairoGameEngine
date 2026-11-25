#include "TerrainRenderOrigin.hpp"
#include <array>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <vector>
#include <stdexcept>
#include <string_view>
#include <system_error>

import Kairo.Player.RuntimeProceduralTerrainBridge;
import Kairo.Renderer;
import Kairo.Foundation.Math;

int main(int argc,char** argv)
{
    try
    {
        using namespace kairo::renderer;
        using namespace kairo::engine;
        bool smoke=false,asynchronous=false,farOrigin=false;
        std::uint64_t seed=42;
        unsigned workerCount=1;
        GraphicsBackend backend=GraphicsBackend::Automatic;
        for(int i=1;i<argc;++i)
        {
            const std::string_view arg=argv[i];
            if(arg=="--smoke")smoke=true;
            else if(arg=="--async")asynchronous=true;
            else if(arg=="--far-origin")farOrigin=true;
            else if(arg=="--workers" && i+1<argc)
            {
                const std::string_view value=argv[++i];
                const auto result=std::from_chars(value.data(),value.data()+value.size(),workerCount);
                if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size()||
                    workerCount==0||workerCount>8)
                    throw std::invalid_argument("Terrain demo workers must be 1..8.");
                asynchronous=true;
            }
            else if(arg=="--seed" && i+1<argc)
            {
                const std::string_view value=argv[++i];
                const auto result=std::from_chars(value.data(),value.data()+value.size(),seed);
                if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size())
                    throw std::invalid_argument("Invalid unsigned world seed.");
            }
            else if(arg=="--renderer" && i+1<argc)backend=ParseGraphicsBackend(argv[++i]);
            else throw std::invalid_argument("Usage: KairoTerrainDemo [--seed N] [--renderer auto|vulkan|metal|d3d12|opengl] [--smoke] [--async] [--workers N] [--far-origin]");
        }
        RendererRuntime renderer({"Kairo procedural terrain",960u,540u,!smoke,backend});
        renderer.ResizeViewport(960u,540u);
        renderer.SetViewportShadingMode(ViewportShadingMode::Lit);
        ProceduralWorldConfig config;
        config.Seed=seed;
        config.Terrain.Intervals=32;
        config.Terrain.MinimumMillimeters=-4000;
        config.Terrain.MaximumMillimeters=12000;
        config.Streaming.LoadRadius=64;
        config.Streaming.KeepRadius=96;
        config.Streaming.MaximumCommittedCells=128;
        config.Streaming.MaximumLoadsPerUpdate=8;
        config.Streaming.MaximumUnloadsPerUpdate=16;
        kairo::player::TerrainBackgroundConfig background;
        if(asynchronous){background.MaximumPendingMeshes=8; background.MaximumUploadsPerUpdate=4; background.WorkerCount=workerCount;}
        kairo::player::RuntimeProceduralTerrainBridge terrain(renderer,config,background);
        RenderScene authored;
        RenderLight sun;
        sun.Type=RenderLightType::Directional;
        sun.Direction={.6f,1.f,.4f};
        sun.Intensity=3.f;
        // This demonstration isolates terrain streaming from the shadow pass.
        sun.CastShadows=false;
        authored.AddLight(sun);
        const auto start=std::chrono::steady_clock::now();
        unsigned frame=0,captures=0;
        bool captureRequested=false;
        std::optional<ViewportCapture> lastTerrainCapture;
        std::size_t loads=0,unloads=0,deferred=0,stale=0,peakPending=0,rebases=0;
        double lastX=0.0,lastZ=0.0;
        kairo::foundation::math::Vec3d origin{};
        const double cellExtent=terrain.Streaming().Config().Streaming.CellSize;
        const auto setCamera=[&](double x,double z,const kairo::foundation::math::Vec3d& base)
        {
            using kairo::player::detail::TerrainRelativeFloat;
            CameraPose camera;
            camera.Position={TerrainRelativeFloat(x,base.x),38.f,TerrainRelativeFloat(z+40.0,base.z)};
            camera.Target={TerrainRelativeFloat(x,base.x),0.f,TerrainRelativeFloat(z,base.z)};
            renderer.SetCameraPose(camera);
        };
        while(!renderer.NativeWindow().ShouldClose())
        {
            renderer.NativeWindow().PollEvents();
            const double seconds=std::chrono::duration<double>(
                std::chrono::steady_clock::now()-start).count();
            // Three distant regions in the bounded smoke; normal mode
            // continuously flies around a large deterministic circuit.
            const double x=(farOrigin?1000000000.0:0.0)+(smoke?double(asynchronous?captures:frame/12u)*256.0:180.0*std::sin(seconds*.08));
            const double z=(farOrigin?-1000000000.0:0.0)+(smoke?16.0:180.0*std::cos(seconds*.08));
            lastX=x;lastZ=z;
            const auto chosen=kairo::player::detail::ChooseTerrainRenderOrigin(x,z,cellExtent);
            if(origin.x!=chosen[0]||origin.z!=chosen[2])++rebases;
            origin={chosen[0],chosen[1],chosen[2]};
            const std::array observers{WorldStreamingObserver{{x,0,z},1.0}};
            const auto update=terrain.UpdateAndSubmit(observers,authored,origin);
            if(!update.Succeeded())
            {
                if(update.FirstFailure)std::rethrow_exception(update.FirstFailure);
                throw std::runtime_error("Terrain streaming failed.");
            }
            loads+=update.Loaded;unloads+=update.Unloaded;
            deferred+=update.DeferredLoads;stale+=update.StaleLoads;
            peakPending=std::max(peakPending,terrain.PendingCount());
            if(terrain.PendingCount()>background.MaximumPendingMeshes)
                throw std::runtime_error("Terrain pending mesh budget exceeded.");
            if(terrain.ResidentCount()+terrain.PendingCount()!=terrain.Streaming().Runtime().CommittedCellCount() ||
                terrain.ResidentCount()>config.Streaming.MaximumCommittedCells)
                throw std::runtime_error("Terrain mesh ownership differs from residency.");
            setCamera(x,z,origin);
            if(smoke && ((!asynchronous && frame%12u==9u) ||
                (asynchronous && !captureRequested && update.Plan.Empty() &&
                    terrain.PendingCount()==0 && terrain.ResidentCount()!=0)))
            {
                renderer.RequestViewportCapture();
                captureRequested=true;
            }
            renderer.DrawFrame();
            if(smoke)
                if(const auto image=renderer.TakeViewportCapture();image.has_value())
                {
                    if(!image->IsVisuallyNonUniform())throw std::runtime_error("Terrain capture is blank.");
                    lastTerrainCapture=*image;
                    ++captures;
                    captureRequested=false;
                }
            ++frame;
            if(smoke&&asynchronous&&(seconds>60.0||frame>3000u))
                throw std::runtime_error("Background terrain smoke did not converge.");
            if(smoke&&((!asynchronous&&frame==36u)||(asynchronous&&captures==3u)))
            {
                if(captures<3u||loads==0u||unloads==0u)
                    throw std::runtime_error("Terrain smoke did not execute every travel phase.");
                break;
            }
        }
        if(smoke&&((!asynchronous&&frame!=36u)||(asynchronous&&captures!=3u)))throw std::runtime_error("Terrain smoke terminated prematurely.");
        // Explicitly change only the rendering origin at the same absolute
        // observer location. Terrain residency must not depend on rebasing.
        if(smoke&&farOrigin)
        {
            if(rebases<3u)throw std::runtime_error("Far-origin test did not cross cell origins.");
            const auto before=terrain.ResidentCount();
            const std::array observers{WorldStreamingObserver{{lastX,0,lastZ},1.0}};
            const auto baselineUpdate=terrain.UpdateAndSubmit(observers,authored,origin);
            if(!baselineUpdate.Succeeded())throw std::runtime_error("Baseline rebase update failed.");
            setCamera(lastX,lastZ,origin);
            const auto capture=[&]()
            {
                renderer.RequestViewportCapture();
                for(unsigned attempt=0;attempt<8u;++attempt)
                {
                    renderer.DrawFrame();
                    if(auto image=renderer.TakeViewportCapture())return *image;
                }
                throw std::runtime_error("Rebase capture unavailable.");
            };
            const auto baseline=capture();
            const auto shifted=kairo::foundation::math::Vec3d{origin.x+cellExtent,0.0,origin.z-cellExtent};
            const auto update=terrain.UpdateAndSubmit(observers,authored,shifted);
            if(!update.Succeeded()||update.Loaded||update.Unloaded||
                terrain.ResidentCount()!=before)
                throw std::runtime_error("Render origin changed terrain residency.");
            setCamera(lastX,lastZ,shifted);
            const auto rebased=capture();
            if(!baseline.IsVisuallyNonUniform()||baseline.Width!=rebased.Width||
                baseline.Height!=rebased.Height||baseline.RGBA.size()!=rebased.RGBA.size())
                throw std::runtime_error("Rebased image unavailable.");
            // Rasterization may differ by a few edge pixels after a different
            // floating-point operation sequence. Compare mean absolute byte error.
            double absoluteError=0.0;
            for(std::size_t i=0;i<baseline.RGBA.size();++i)
                absoluteError+=std::abs(int(baseline.RGBA[i])-int(rebased.RGBA[i]));
            if(absoluteError>0.1*static_cast<double>(baseline.RGBA.size()))
                throw std::runtime_error("Render origin materially changed captured image.");
            lastTerrainCapture=rebased;
            std::cout<<"Far-origin image comparison mean byte error "<<
                absoluteError/static_cast<double>(baseline.RGBA.size())<<"; rebases "<<rebases<<'\n';
        }
        if(terrain.Shutdown(authored)!=0u)throw std::runtime_error("Terrain shutdown leaked handles.");
        if(smoke)
        {
            renderer.RequestViewportCapture();
            std::optional<ViewportCapture> cleared;
            for(unsigned attempt=0;attempt<8u && !cleared;++attempt)
            {
                renderer.DrawFrame();
                cleared=renderer.TakeViewportCapture();
            }
            if(!cleared || !lastTerrainCapture ||
                cleared->Width!=lastTerrainCapture->Width ||
                cleared->Height!=lastTerrainCapture->Height ||
                cleared->RGBA==lastTerrainCapture->RGBA)
                throw std::runtime_error("Terrain did not change the native image relative to the cleared scene.");
        }
        std::cout<<"Terrain seed "<<seed<<": "<<loads<<" uploads, "<<unloads
            <<" unloads, "<<captures<<" captures, "<<deferred<<" deferred, "<<stale
            <<" stale, peak pending "<<peakPending<<"; shutdown passed\n";
        return 0;
    }
    catch(const std::exception& e)
    {
        std::cerr<<"Terrain demo failed: "<<e.what()<<'\n';
        return 1;
    }
}
