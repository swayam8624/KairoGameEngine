#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "TerrainWalker.hpp"
#include "TerrainRenderOrigin.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <charconv>
#include <iostream>
#include <limits>
#include <string_view>
#include <optional>
#include <stdexcept>

import Kairo.Player.RuntimeProceduralTerrainBridge;
import Kairo.Renderer;
import Kairo.Foundation.Math;

int main(int argc,char** argv)
{
    try
    {
        using namespace kairo::renderer;
        using namespace kairo::engine;
        using namespace kairo::player::detail;
        bool smoke=false;
        std::uint64_t seed=42;
        GraphicsBackend backend=GraphicsBackend::Automatic;
        for(int i=1;i<argc;++i)
        {
            const std::string_view arg=argv[i];
            if(arg=="--smoke")smoke=true;
            else if(arg=="--renderer"&&i+1<argc)backend=ParseGraphicsBackend(argv[++i]);
            else if(arg=="--seed"&&i+1<argc)
            {
                const std::string_view value=argv[++i];
                const auto result=std::from_chars(value.data(),value.data()+value.size(),seed);
                if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size())
                    throw std::invalid_argument("Invalid seed.");
            }
            else throw std::invalid_argument("Usage: KairoTerrainExplorer [--seed N] [--renderer backend] [--smoke]");
        }
        RendererRuntime renderer({"Kairo Explorer: WASD / arrows / Shift / Space / Esc",960u,540u,!smoke,backend});
        renderer.ResizeViewport(960,540);
        renderer.SetViewportShadingMode(ViewportShadingMode::Lit);
        ProceduralWorldConfig world;
        world.Seed=seed;
        world.Terrain.MinimumMillimeters=-4000;
        world.Terrain.MaximumMillimeters=12000;
        world.Streaming.LoadRadius=64;
        world.Streaming.KeepRadius=96;
        world.Streaming.MaximumCommittedCells=128;
        world.Streaming.MaximumLoadsPerUpdate=8;
        world.Streaming.MaximumUnloadsPerUpdate=16;
        kairo::player::TerrainBackgroundConfig background;
        background.MaximumPendingMeshes=8;
        background.MaximumUploadsPerUpdate=4;
        background.WorkerCount=4;
        // Keep nearby traversal exactly on the canonical surface.
        background.Lod.SwitchDistances={32.0,48.0,64.0};
        kairo::player::RuntimeProceduralTerrainBridge terrain(renderer,world,background);
        const double extent=terrain.Streaming().Config().Streaming.CellSize;
        RenderScene authored;
        RenderLight sun;
        sun.Type=RenderLightType::Directional;
        sun.Direction={.6f,1.f,.4f};sun.Intensity=3.f;sun.CastShadows=false;
        authored.AddLight(sun);
        TerrainWalker walker;
        TerrainWalkConfig movement;
        if(smoke){walker.Yaw=1.5707963267948966;movement.Speed=8;movement.MaximumGrade=3;}
        const auto ground=[&](double x,double z)->std::optional<double>
        {
            const double cx=std::floor(x/extent),cz=std::floor(z/extent);
            if(cx<std::numeric_limits<std::int32_t>::min()||cx>std::numeric_limits<std::int32_t>::max()||
                cz<std::numeric_limits<std::int32_t>::min()||cz>std::numeric_limits<std::int32_t>::max())return {};
            const WorldCellCoordinate cell{static_cast<std::int32_t>(cx),static_cast<std::int32_t>(cz)};
            if(!terrain.IsFullDetailResident(cell))return {};
            const auto sample=SampleSeededTerrainSurface(seed,cell,x-cx*extent,z-cz*extent,
                world.Terrain,world.SampleSpacingMillimeters);
            if(!sample)return {};
            return sample->HeightMeters;
        };
        const auto start=std::chrono::steady_clock::now();
        auto previous=start;
        unsigned frames=0,moves=0,captures=0;
        std::size_t loads=0,unloads=0;
        bool requested=false,jumpHeld=false,jumpStarted=false,jumpLanded=false;
        double peakJumpHeight=0;
        auto* window=renderer.NativeWindow().NativeHandle();
        const auto key=[&](int code){return glfwGetKey(window,code)==GLFW_PRESS?1.0:0.0;};
        while(!renderer.NativeWindow().ShouldClose())
        {
            renderer.NativeWindow().PollEvents();
            const auto now=std::chrono::steady_clock::now();
            const double dt=smoke?0.1:std::min(0.1,std::chrono::duration<double>(now-previous).count());
            previous=now;
            TerrainWalkInput input;
            if(smoke)
            {
                if(!jumpStarted&&walker.Initialized&&walker.Grounded)
                {input.JumpPressed=true;jumpStarted=true;}
                if(jumpStarted&&jumpLanded)input.Forward=moves<300?1.0:0.0;
            }
            else
            {
                if(key(GLFW_KEY_ESCAPE)){renderer.NativeWindow().RequestClose();break;}
                input.Forward=key(GLFW_KEY_W)-key(GLFW_KEY_S);
                input.Right=key(GLFW_KEY_D)-key(GLFW_KEY_A);
                input.YawDelta=(key(GLFW_KEY_RIGHT)-key(GLFW_KEY_LEFT))*1.8*dt;
                input.PitchDelta=(key(GLFW_KEY_UP)-key(GLFW_KEY_DOWN))*1.4*dt;
                input.Sprint=key(GLFW_KEY_LEFT_SHIFT)>0;
                const bool jump=key(GLFW_KEY_SPACE)>0;
                input.JumpPressed=jump&&!jumpHeld;jumpHeld=jump;
            }
            if(AdvanceTerrainWalker(walker,input,dt,movement,ground))++moves;
            if(jumpStarted)
            {
                peakJumpHeight=std::max(peakJumpHeight,walker.FootY-walker.GroundY);
                if(walker.Grounded&&peakJumpHeight>0.5)jumpLanded=true;
            }
            const auto o=ChooseTerrainRenderOrigin(walker.X,walker.Z,extent);
            const kairo::foundation::math::Vec3d origin{o[0],o[1],o[2]};
            const std::array observers{WorldStreamingObserver{{walker.X,walker.GroundY,walker.Z},1.0}};
            const auto update=terrain.UpdateAndSubmit(observers,authored,origin);
            if(!update.Succeeded())throw std::runtime_error("Explorer terrain update failed.");
            loads+=update.Loaded;unloads+=update.Unloaded;
            if(!walker.Initialized)(void)AdvanceTerrainWalker(walker,{},0,movement,ground);
            CameraPose camera;
            const double y=walker.FootY+movement.EyeHeight;
            const double cp=std::cos(walker.Pitch);
            camera.Position={TerrainRelativeFloat(walker.X,origin.x),TerrainRelativeFloat(y,origin.y),
                TerrainRelativeFloat(walker.Z,origin.z)};
            camera.Target={TerrainRelativeFloat(walker.X+std::sin(walker.Yaw)*cp,origin.x),
                TerrainRelativeFloat(y+std::sin(walker.Pitch),origin.y),
                TerrainRelativeFloat(walker.Z-std::cos(walker.Yaw)*cp,origin.z)};
            renderer.SetCameraPose(camera);
            if(smoke&&!requested&&moves>=(captures+1u)*100u&&ground(walker.X,walker.Z))
            {renderer.RequestViewportCapture();requested=true;}
            renderer.DrawFrame();
            if(smoke)
            {
                if(auto image=renderer.TakeViewportCapture())
                {
                    if(!image->IsVisuallyNonUniform())throw std::runtime_error("Explorer capture blank.");
                    ++captures;requested=false;
                }
                if(captures==3)break;
                if(++frames>2000||std::chrono::duration<double>(now-start).count()>90)
                    throw std::runtime_error("Explorer smoke timed out.");
            }
        }
        if(smoke&&(!jumpLanded||peakJumpHeight<1.0||peakJumpHeight>1.4||captures!=3||walker.X<200||loads==0||unloads==0))
            throw std::runtime_error("Explorer did not cross streamed terrain.");
        if(terrain.Shutdown(authored)!=0)throw std::runtime_error("Explorer shutdown retained meshes.");
        if(smoke)std::cout<<"Explorer jump and landing passed; peak sampled height "<<peakJumpHeight<<" m\n";
        std::cout<<"Terrain explorer: "<<moves<<" moving updates, x "<<walker.X
            <<", "<<loads<<" loads, "<<unloads<<" unloads, "<<captures<<" captures; shutdown passed\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<"Explorer failed: "<<e.what()<<'\n';return 1;}
}
