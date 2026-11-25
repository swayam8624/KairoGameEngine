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
#include <span>
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
        bool smoke=false,asynchronous=false,farOrigin=false,lod=false;
        std::uint64_t seed=42;
        std::optional<std::uint64_t> errorBudget;
        std::optional<double> pixelBudget;
        unsigned workerCount=1;
        GraphicsBackend backend=GraphicsBackend::Automatic;
        for(int i=1;i<argc;++i)
        {
            const std::string_view arg=argv[i];
            if(arg=="--smoke")smoke=true;
            else if(arg=="--async")asynchronous=true;
            else if(arg=="--far-origin")farOrigin=true;
            else if(arg=="--lod"){lod=true;asynchronous=true;}
            else if(arg=="--workers" && i+1<argc)
            {
                const std::string_view value=argv[++i];
                const auto result=std::from_chars(value.data(),value.data()+value.size(),workerCount);
                if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size()||
                    workerCount==0||workerCount>8)
                    throw std::invalid_argument("Terrain demo workers must be 1..8.");
                asynchronous=true;
            }
            else if(arg=="--screen-px" && i+1<argc)
            {
                std::uint32_t parsed=0;
                const std::string_view value=argv[++i];
                const auto result=std::from_chars(value.data(),value.data()+value.size(),parsed);
                if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size()||
                    parsed==0)
                    throw std::invalid_argument("Invalid terrain pixel error budget.");
                pixelBudget=static_cast<double>(parsed);lod=true;asynchronous=true;
            }
            else if(arg=="--error-mm" && i+1<argc)
            {
                std::uint64_t parsed=0;
                const std::string_view value=argv[++i];
                const auto result=std::from_chars(value.data(),value.data()+value.size(),parsed);
                if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size())
                    throw std::invalid_argument("Invalid unsigned terrain error budget.");
                errorBudget=parsed;lod=true;asynchronous=true;
            }
            else if(arg=="--seed" && i+1<argc)
            {
                const std::string_view value=argv[++i];
                const auto result=std::from_chars(value.data(),value.data()+value.size(),seed);
                if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size())
                    throw std::invalid_argument("Invalid unsigned world seed.");
            }
            else if(arg=="--renderer" && i+1<argc)backend=ParseGraphicsBackend(argv[++i]);
            else throw std::invalid_argument("Usage: KairoTerrainDemo [--seed N] [--renderer auto|vulkan|metal|d3d12|opengl] [--smoke] [--async] [--workers N] [--far-origin] [--lod] [--error-mm N] [--screen-px N]");
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
        if(lod)background.Lod.SwitchDistances={16.0,40.0,56.0};
        background.MaximumLodErrorMillimeters=errorBudget;
        background.MaximumLodErrorPixels=pixelBudget;
        kairo::player::RuntimeProceduralTerrainBridge terrain(renderer,config,background);
        RenderScene authored;
        RenderLight sun;
        sun.Type=RenderLightType::Directional;
        sun.Direction={.6f,1.f,.4f};
        sun.Intensity=3.f;
        // This demonstration isolates terrain streaming from the shadow pass.
        sun.CastShadows=false;
        authored.AddLight(sun);
        std::uint64_t peakError=0;
        const auto validateError=[&]()
        {
            if(!errorBudget)return;
            if(terrain.ResidentErrorProfileCount()!=terrain.ResidentCount())
                throw std::runtime_error("Resident terrain missing error profile.");
            const auto error=terrain.MaximumResidentLodErrorMillimeters();
            if(error>*errorBudget)throw std::runtime_error("Resident terrain exceeds geometric error budget.");
            peakError=std::max(peakError,error);
        };
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
        const auto viewFor=[&](double x,double z)
        {
            kairo::player::TerrainLodView view;
            view.Position={x,38.0,z+40.0};
            view.Forward={0.0,-38.0,-40.0};
            view.VerticalFovRadians=CameraPose{}.VerticalFovRadians;
            view.NearPlane=CameraPose{}.NearPlane;
            return view;
        };
        const auto updateTerrain=[&](std::span<const WorldStreamingObserver> observers,
            const kairo::foundation::math::Vec3d& base)
        {
            const auto view=viewFor(lastX,lastZ);
            return terrain.UpdateAndSubmit(observers,authored,base,pixelBudget?&view:nullptr);
        };
        double maximumSettledPixels=0;
        const auto validateSettledProjection=[&]()
        {
            if(!pixelBudget)return;
            if(terrain.ResidentErrorProfileCount()!=terrain.ResidentCount())
                throw std::runtime_error("Missing camera LOD profile.");
            const double error=terrain.MaximumResidentProjectedErrorPixels(viewFor(lastX,lastZ));
            if(error>*pixelBudget)throw std::runtime_error("Settled terrain exceeds projected error estimate.");
            maximumSettledPixels=std::max(maximumSettledPixels,error);
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
            const auto update=updateTerrain(observers,origin);
            if(!update.Succeeded())
            {
                if(update.FirstFailure)std::rethrow_exception(update.FirstFailure);
                throw std::runtime_error("Terrain streaming failed.");
            }
            validateError();
            loads+=update.Loaded;unloads+=update.Unloaded;
            deferred+=update.DeferredLoads;stale+=update.StaleLoads;
            peakPending=std::max(peakPending,terrain.PendingCount());
            if(terrain.PendingCount()>background.MaximumPendingMeshes)
                throw std::runtime_error("Terrain pending mesh budget exceeded.");
            if(terrain.ResidentCount()+terrain.PendingLoadCount()!=terrain.Streaming().Runtime().CommittedCellCount() ||
                terrain.ResidentCount()>config.Streaming.MaximumCommittedCells)
                throw std::runtime_error("Terrain mesh ownership differs from residency.");
            setCamera(x,z,origin);
            if(smoke && ((!asynchronous && frame%12u==9u) ||
                (asynchronous && !captureRequested && update.Plan.Empty() &&
                    terrain.PendingCount()==0 && terrain.ResidentCount()!=0)))
            {
                validateSettledProjection();
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
        if(smoke&&lod)
        {
            if(!errorBudget && !pixelBudget && (terrain.ResidentLodCount(1)==0 || terrain.ResidentLodCount(1)==terrain.ResidentCount()))
                throw std::runtime_error("Expected mixed terrain LODs.");
            // Hold the world position fixed, but move the detail demand
            // inward and outward via observer scale. The closest loaded
            // cells must survive while their meshes are replaced.
            std::size_t replaced=0;
            const auto settle=[&](double scale)
            {
                const std::array observers{WorldStreamingObserver{{lastX,0,lastZ},scale}};
                const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
                for(unsigned step=0;step<2000;++step)
                {
                    const auto update=updateTerrain(observers,origin);
                    if(!update.Succeeded())throw std::runtime_error("LOD transition failed.");
                    validateError();
                    replaced+=update.Replaced;
                    if(terrain.ResidentCount()==0||
                        terrain.ResidentCount()+terrain.PendingLoadCount()!=terrain.Streaming().Runtime().CommittedCellCount())
                        throw std::runtime_error("LOD transition lost residency accounting.");
                    renderer.DrawFrame();
                    if(update.Plan.Empty()&&terrain.PendingCount()==0&&update.RetainedRetirements==0)
                    {validateSettledProjection();return;}
                    if(std::chrono::steady_clock::now()>deadline)break;
                }
                throw std::runtime_error("LOD transition did not settle.");
            };
            settle(0.6);
            settle(1.0);
            if(!errorBudget && !pixelBudget && replaced==0)throw std::runtime_error("No resident LOD replacement executed.");
            renderer.RequestViewportCapture();
            bool captured=false;
            for(unsigned attempt=0;attempt<8&&!captured;++attempt)
            {
                renderer.DrawFrame();
                if(auto image=renderer.TakeViewportCapture())
                {
                    if(!image->IsVisuallyNonUniform())throw std::runtime_error("LOD terrain image blank.");
                    lastTerrainCapture=*image;captured=true;
                }
            }
            if(!captured)throw std::runtime_error("LOD transition capture unavailable.");
            std::cout<<"Mixed terrain LOD transitions: "<<replaced<<" resident replacements passed\n";
        }
        // Explicitly change only the rendering origin at the same absolute
        // observer location. Terrain residency must not depend on rebasing.
        if(smoke&&farOrigin)
        {
            if(rebases<3u)throw std::runtime_error("Far-origin test did not cross cell origins.");
            const auto before=terrain.ResidentCount();
            const std::array observers{WorldStreamingObserver{{lastX,0,lastZ},1.0}};
            const auto baselineUpdate=updateTerrain(observers,origin);
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
            const auto update=updateTerrain(observers,shifted);
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
        if(pixelBudget)
        {
            if(smoke&&maximumSettledPixels==0)throw std::runtime_error("No nonzero projected LOD error exercised.");
            std::cout<<"Terrain projected error target "<<*pixelBudget<<" px, maximum settled estimate "<<maximumSettledPixels<<" px\n";
        }
        if(errorBudget)
            std::cout<<"Terrain error ceiling "<<*errorBudget<<" mm, maximum resident error "
                <<peakError<<" mm; profiles verified\n";
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
