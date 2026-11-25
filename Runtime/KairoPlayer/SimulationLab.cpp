#include "CraftWorld.hpp"
#include "RuntimeWaterGrid.hpp"
#include "RuntimeSoftBody.hpp"
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

    // Construct outward-facing triangles from the current four vertices.
    // Recompute face normals after every deformation, not from rest pose.
    // Opposite vertex chooses orientation even if the authored order changes.
    Mesh TetraMesh(const RuntimeSoftBody& body)
    {
        using namespace soft_detail;
        const std::array<std::array<unsigned,4>,4> faces{{{{0,1,2,3}},{{0,3,1,2}},{{0,2,3,1}},{{1,3,2,0}}}};
        std::vector<MeshVertex> vertices;std::vector<std::uint32_t> indices;
        for(const auto& face:faces)
        {
            auto a=body.Vertices()[face[0]].Position,b=body.Vertices()[face[1]].Position,c=body.Vertices()[face[2]].Position;
            const auto opposite=body.Vertices()[face[3]].Position;
            auto normal=Cross(Sub(b,a),Sub(c,a));
            if(Dot(normal,Sub(opposite,a))>0){std::swap(b,c);normal=Scale(normal,-1.);}
            const auto length=Length(normal);
            if(length<1e-10)throw std::runtime_error("Degenerate visual tetra face.");
            normal=Scale(normal,1./length);
            for(const auto& p:{a,b,c})
            {
                indices.push_back(static_cast<std::uint32_t>(vertices.size()));
                vertices.push_back({{float(p[0]),float(p[1]),float(p[2])},{1,1,1},
                    {float(normal[0]),float(normal[1]),float(normal[2])},{0,0}});
            }
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
        bool soft=false,smoke=false;
        GraphicsBackend backend=GraphicsBackend::Automatic;
        for(int i=1;i<argc;++i)
        {
            const std::string_view arg=argv[i];
            if(arg=="--soft")soft=true;
            else if(arg=="--smoke")smoke=true;
            else if(arg=="--renderer"&&i+1<argc)backend=ParseGraphicsBackend(argv[++i]);
            else throw std::invalid_argument("Usage: KairoSimulationLab [--soft] [--renderer backend] [--smoke]");
        }
        RendererRuntime renderer({"Kairo simulation lab",960u,540u,!smoke,backend});
        renderer.ResizeViewport(960,540);renderer.SetViewportShadingMode(ViewportShadingMode::Lit);
        CameraPose camera;
        if(soft){camera.Position={4,4,7};camera.Target={0,1.8f,0};}
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

        const auto cube=renderer.CreateMesh(UnitCube());
        MeshHandle tetra=InvalidMeshHandle;
        const auto publish=[&]()
        {
            if(!soft)
            {
                auto scene=WaterScene(water,cube);Light(scene);renderer.SubmitRenderScene(scene);return;
            }
            // Upload the newly deformed tetrahedron before changing the
            // scene. Once submission succeeds, the previous mesh is unbound
            // and can be destroyed. Never delete a still-submitted handle.
            const auto next=renderer.CreateMesh(TetraMesh(body));
            try
            {
                RenderScene scene;Light(scene);
                Box(scene,cube,{-3,-.2f,-3},{6,.2f,6},{.22f,.28f,.24f});
                MeshDraw draw;draw.Mesh=next;draw.Material.BaseColor={.88f,.25f,.07f};
                draw.CastShadows=false;draw.ReceiveShadows=false;scene.Add(draw);
                renderer.SubmitRenderScene(scene);
            }
            catch(...){renderer.DestroyMesh(next);throw;}
            const auto previous=tetra;tetra=next;
            if(previous!=InvalidMeshHandle)renderer.DestroyMesh(previous);
        };
        const auto advance=[&]()
        {
            // Fixed simulation steps independent of presentation frequency.
            if(soft)for(unsigned i=0;i<4;++i)body.Step();
            else water.Step(.2);
        };
        publish();
        if(smoke)
        {
            const auto before=Capture(renderer);
            for(unsigned i=0;i<30;++i)advance();
            publish();const auto after=Capture(renderer);
            if(before.RGBA==after.RGBA)throw std::runtime_error("Simulation did not change native image.");
            if(soft&&std::abs(body.TetraVolume(0)-volume)>.05*std::abs(volume))
                throw std::runtime_error("Soft body volume drift.");
            if(!soft&&std::abs(water.TotalVolume()-8)>1e-8)
                throw std::runtime_error("Water volume drift.");
            std::cout<<(soft?"Soft body":"Water")<<" native capture change and solver bounds passed\n";
        }
        else
        {
            // Throttle dynamic mesh uploads to 30 Hz. Poll events even when
            // minimized; never submit a zero-size viewport to the backend.
            auto previous=std::chrono::steady_clock::now();
            std::uint32_t width=960,height=540;
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
        // Unbind scene references first, then release renderer-owned meshes.
        renderer.SubmitRenderScene(RenderScene{});
        if(tetra!=InvalidMeshHandle)renderer.DestroyMesh(tetra);
        renderer.DestroyMesh(cube);
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
