#include "CraftWorld.hpp"
#include "RuntimeWaterGrid.hpp"
#include "RuntimeTetraSurface.hpp"
#include "VoxelMeshPublication.hpp"
#include "RuntimeBreakableCube.hpp"
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

import Kairo.Renderer;
import Kairo.Foundation.Math;

namespace
{
    using namespace kairo::renderer;
    using namespace kairo::foundation::math;
    using namespace kairo::player::detail;

    // Reuse Craft's tested unit-cube topology. One renderer-owned mesh
    // represents all static terrain cells and water columns.
    Mesh UnitCube()
    {
        auto cube=BuildCraftMesh(BuildVoxelChunkSurface({0,0,0},1,[](VoxelCoordinate c){
            return c==VoxelCoordinate{}?CraftStone:VoxelAir;}));
        std::vector<MeshVertex> vertices;
        vertices.reserve(cube.Vertices.size());
        for(const auto& v:cube.Vertices)
            vertices.push_back({{v.Position[0],v.Position[1],v.Position[2]},
                {1,1,1},{v.Normal[0],v.Normal[1],v.Normal[2]},{v.UV[0],v.UV[1]}});
        return Mesh(std::move(vertices),std::move(cube.Indices));
    }

    // Per-instance model matrix maps [0,1]^3 cube to world-space box.
    // Do not submit singular transforms: the renderer computes inverse
    // transpose normals and rejects zero-size geometry.
    void Box(RenderScene& scene,MeshHandle cube,Vec3f origin,Vec3f size,Vec3f color)
    {
        MeshDraw draw;draw.Mesh=cube;draw.Model=Mat4f::Identity();
        draw.Model(0,0)=size.x;draw.Model(1,1)=size.y;draw.Model(2,2)=size.z;
        draw.Model(0,3)=origin.x;draw.Model(1,3)=origin.y;draw.Model(2,3)=origin.z;
        draw.Material.BaseColor=color;draw.CastShadows=false;draw.ReceiveShadows=false;
        scene.Add(draw);
    }

    // A visual adapter, not a fluid shader: each positive depth is drawn
    // as an opaque blue column above its bed. The grid itself owns volume.
    RenderScene WaterScene(const RuntimeWaterGrid& water,MeshHandle cube)
    {
        RenderScene scene;
        for(std::size_t z=0;z<water.Height();++z)for(std::size_t x=0;x<water.Width();++x)
        {
            const float depth=static_cast<float>(water.Depth(x,z));
            const float bed=static_cast<float>(water.Surface(x,z)-water.Depth(x,z));
            Box(scene,cube,{float(x),-.25f,float(z)},{1,bed+.25f,1},{.26f,.24f,.19f});
            // Hide tiny subpixel columns instead of submitting near-singular matrices.
            if(depth>=.02f)Box(scene,cube,{float(x),bed,float(z)},{.96f,depth,.96f},{.04f,.35f,.8f});
        }
        return scene;
    }

    // Adapt the tested CPU boundary geometry to the renderer vertex ABI.
    // Each face owns three vertices with one outward normal.
    Mesh TetraMesh(const RuntimeSoftBody& body)
    {
        const auto surface=BuildTetraSurface(body);
        std::vector<MeshVertex> vertices;std::vector<std::uint32_t> indices;
        vertices.reserve(surface.size());indices.reserve(surface.size());
        for(const auto& v:surface)
        {
            const auto& p=v.Position;const auto& n=v.Normal;
            indices.push_back(static_cast<std::uint32_t>(vertices.size()));
            vertices.push_back({{float(p[0]),float(p[1]),float(p[2])},{1,1,1},{float(n[0]),float(n[1]),float(n[2])},{0,0}});
        }
        return Mesh(std::move(vertices),std::move(indices));
    }

    // Static scene state is separated from simulation. All modes use the
    // same native renderer facade and capture path as the normal Player.
    void Light(RenderScene& scene)
    {
        RenderLight sun;sun.Direction={.6f,1.f,.4f};sun.Intensity=3;sun.CastShadows=false;
        scene.AddLight(sun);
        RenderEnvironment env;env.AmbientIntensity=.25f;scene.SetEnvironment(env);
    }
    ViewportCapture Capture(RendererRuntime& renderer)
    {
        renderer.RequestViewportCapture();
        for(unsigned i=0;i<12;++i)
        {
            renderer.DrawFrame();
            if(auto capture=renderer.TakeViewportCapture())
            {
                if(!capture->IsVisuallyNonUniform())throw std::runtime_error("Simulation lab capture blank.");
                return *capture;
            }
        }
        throw std::runtime_error("Simulation lab capture unavailable.");
    }
}

int main(int argc,char** argv)
{
    try
    {
        bool soft=false,smoke=false,destruction=false;
        GraphicsBackend backend=GraphicsBackend::Automatic;
        for(int i=1;i<argc;++i)
        {
            const std::string_view arg=argv[i];
            if(arg=="--soft")soft=true;
            else if(arg=="--destruction")destruction=true;
            else if(arg=="--smoke")smoke=true;
            else if(arg=="--renderer"&&i+1<argc)backend=ParseGraphicsBackend(argv[++i]);
            else throw std::invalid_argument("Usage: KairoSimulationLab [--soft | --destruction] [--renderer backend] [--smoke]");
        }
        if(soft&&destruction)throw std::invalid_argument("Select one simulation mode.");
        RendererRuntime renderer({"Kairo simulation lab",960u,540u,!smoke,backend});
        const auto [initialWidth,initialHeight]=renderer.NativeWindow().FramebufferExtent();
        if(!initialWidth||!initialHeight)throw std::runtime_error("Simulation lab framebuffer unavailable.");
        renderer.ResizeViewport(initialWidth,initialHeight);renderer.SetViewportShadingMode(ViewportShadingMode::Lit);
        CameraPose camera;
        if(soft||destruction){camera.Position={5,5,9};camera.Target={0,1.8f,0};}
        else{camera.Position={12,18,20};camera.Target={8,0,4.5f};}
        renderer.SetCameraPose(camera);

        // Same fixtures as the portable tests/labs; independent CPU solver state.
        RuntimeWaterGrid water(16,9);
        for(std::size_t z=0;z<water.Height();++z)if(z!=4)water.SetBed(8,z,2);
        water.AddWater(2,4,8);
        RuntimeSoftBody body;
        body.AddVertex({0,3,0});body.AddVertex({1,3,0});
        body.AddVertex({0,4,0});body.AddVertex({0,3,1});
        for(unsigned a=0;a<4;++a)for(unsigned b=a+1;b<4;++b)body.AddEdge(a,b);
        body.AddTetra({0,1,2,3});
        const double volume=body.TetraVolume(0);
        RuntimeBreakableCube breakable({0,2.5,0},.8,8,2);
        unsigned destructionFrame=0;

        const auto cube=renderer.CreateMesh(UnitCube());
        // Reuse Craft's bounded publication contract for one deformable mesh.
        // A failed submit may have partially replaced the scene; keep both
        // generations until a successful restore or empty-scene submission.
        using TetraPublication=VoxelMeshPublication<MeshHandle,1>;
        TetraPublication tetraPublication;
        const auto submitTetra=[&](const TetraPublication::Slots& slots)
        {
            RenderScene scene;
            if(slots[0])
            {
                Light(scene);Box(scene,cube,{-3,-.2f,-3},{6,.2f,6},{.22f,.28f,.24f});
                MeshDraw draw;draw.Mesh=*slots[0];draw.Material.BaseColor={.88f,.25f,.07f};
                draw.CastShadows=false;draw.ReceiveShadows=false;scene.Add(draw);
            }
            renderer.SubmitRenderScene(scene);
        };
        const auto destroy=[&](MeshHandle handle){renderer.DestroyMesh(handle);};
        const auto cleanup=[&]()
        {
            // Empty submission also unbinds the shared cube in every mode.
            if(tetraPublication.Release(submitTetra,destroy)!=0)
                throw std::runtime_error("Simulation lab retained dynamic mesh handles.");
            // The dynamic journal may be empty in water/destruction mode.
            // Explicitly unbind the cube even if an empty journal reported zero.
            renderer.SubmitRenderScene(RenderScene{});
            renderer.DestroyMesh(cube);
        };
        const auto publish=[&]()
        {
            if(destruction)
            {
                RenderScene scene;Light(scene);
                Box(scene,cube,{-4,-.2f,-4},{8,.2f,8},{.22f,.28f,.24f});
                // One mesh handle is shared by the intact block and all eight
                // fragments. Fracture changes instances, not mesh topology.
                const auto draw=[&](FragmentPoint p,double half)
                {
                    const float h=static_cast<float>(half);
                    Box(scene,cube,{float(p[0])-h,float(p[1])-h,float(p[2])-h},{2*h,2*h,2*h},{.85f,.38f,.08f});
                };
                if(!breakable.Fractured())draw(breakable.Center(),breakable.HalfExtent());
                else for(const auto& fragment:breakable.Fragments())draw(fragment.Center,fragment.HalfExtent);
                renderer.SubmitRenderScene(scene);return;
            }
            if(!soft)
            {
                auto scene=WaterScene(water,cube);Light(scene);renderer.SubmitRenderScene(scene);return;
            }
            if(!tetraPublication.Apply({true},[&](std::size_t){return renderer.CreateMesh(TetraMesh(body));},submitTetra,destroy))
                throw std::runtime_error("Simulation lab mesh retirement retry failed.");
        };
        const auto advance=[&]()
        {
            // Fixed simulation steps independent of presentation frequency.
            if(destruction)
            {
                // Authored fixture impact after 15 presentation updates.
                // It is not yet routed from a PhysicsEngine contact event.
                if(++destructionFrame==15)(void)breakable.Strike({5,3,0});
                for(unsigned i=0;i<4;++i)breakable.Step();
            }
            else if(soft)for(unsigned i=0;i<4;++i)body.Step();
            else water.Step(.2);
        };
        try
        {
        publish();
        if(smoke)
        {
            const auto before=Capture(renderer);
            for(unsigned i=0;i<30;++i)advance();
            publish();const auto after=Capture(renderer);
            if(before.RGBA==after.RGBA)throw std::runtime_error("Simulation did not change native image.");
            if(soft&&std::abs(body.TetraVolume(0)-volume)>.05*std::abs(volume))
                throw std::runtime_error("Soft body volume drift.");
            if(!soft&&!destruction&&std::abs(water.TotalVolume()-8)>1e-8)
                throw std::runtime_error("Water volume drift.");
            if(destruction)
            {
                if(!breakable.Fractured())throw std::runtime_error("Fracture did not trigger.");
                for(const auto& fragment:breakable.Fragments())if(fragment.Center[1]<fragment.HalfExtent)
                    throw std::runtime_error("Fragment penetrated floor.");
            }
            std::cout<<(destruction?"Destruction":soft?"Soft body":"Water")<<" native capture change and solver bounds passed\n";
        }
        else
        {
            // Throttle dynamic mesh uploads to 30 Hz. Poll events even when
            // minimized; never submit a zero-size viewport to the backend.
            auto previous=std::chrono::steady_clock::now();
            std::uint32_t width=initialWidth,height=initialHeight;
            while(!renderer.NativeWindow().ShouldClose())
            {
                renderer.NativeWindow().PollEvents();
                if(renderer.NativeWindow().ShouldClose())break;
                const auto [w,h]=renderer.NativeWindow().FramebufferExtent();
                if(!w||!h){previous=std::chrono::steady_clock::now();std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;}
                if(w!=width||h!=height){renderer.ResizeViewport(w,h);width=w;height=h;}
                const auto now=std::chrono::steady_clock::now();
                if(now-previous>=std::chrono::milliseconds(33)){advance();publish();previous=now;}
                renderer.DrawFrame();
            }
        }
        }
        catch(...)
        {
            // Never destroy a potentially submitted handle to hide a failure.
            // Best effort cleanup preserves the original exception; the
            // renderer remains the final owner if its device is unusable.
            try{cleanup();}catch(...){}
            throw;
        }
        cleanup();
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
