#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "CraftCaveWorld.hpp"
#include "CraftMeshBatch.hpp"
#include "VoxelMeshPublication.hpp"
#include "VoxelPlayer.hpp"
#include "TerrainRenderOrigin.hpp"
#include <chrono>
#include <iostream>
#include <memory>
#include <string_view>
#include <charconv>

import Kairo.Renderer;
import Kairo.Foundation.Math;

int main(int argc,char** argv)
{
    try
    {
        using namespace kairo::renderer;
        using namespace kairo::foundation::math;
        using namespace kairo::player::detail;
        bool smoke=false,survival=false;
        std::uint64_t seed=42;
        auto backend=GraphicsBackend::Automatic;
        for(int i=1;i<argc;++i)
        {
            const std::string_view arg=argv[i];
            if(arg=="--smoke")smoke=true;
            else if(arg=="--survival")survival=true;
            else if(arg=="--renderer"&&i+1<argc)backend=ParseGraphicsBackend(argv[++i]);
            else if(arg=="--seed"&&i+1<argc)
            {
                const std::string_view s=argv[++i];
                const auto result=std::from_chars(s.data(),s.data()+s.size(),seed);
                if(result.ec!=std::errc{}||result.ptr!=s.data()+s.size())throw std::invalid_argument("Invalid seed.");
            }
            else throw std::invalid_argument("KairoCraftCaves [--seed N] [--renderer backend] [--survival] [--smoke]");
        }
        RendererRuntime renderer({"Kairo Craft Caves",960u,540u,!smoke,backend});
        renderer.ResizeViewport(960,540);
        renderer.SetViewportShadingMode(ViewportShadingMode::Lit);
        using P=CaveWindowPlan<1>;
        using W=VoxelEdits<CaveCraftBaseline>;
        using Pub=VoxelMeshPublication<MeshHandle,P::Capacity>;
        auto world=std::make_shared<W>(CaveCraftBaseline{seed});
        CraftMeshBatch<1,P,CaveCraftBaseline,CaveChunk> workers(4);
        Pub pub;
        P::Slots coords{};
        std::optional<CaveChunk> committed,pending;
        std::optional<P> plan;
        VoxelCoordinate origin{};
        std::size_t loads=0,evictions=0,retained=0,peakUploads=0;
        RenderScene authored;
        RenderLight light;light.Type=RenderLightType::Directional;
        light.Direction={.6f,1.f,.4f};light.Intensity=3;light.CastShadows=false;
        authored.AddLight(light);
        const auto submit=[&](const Pub::Slots& handles,const P::Slots& cells,VoxelCoordinate base)
        {
            auto scene=authored;
            for(std::size_t i=0;i<handles.size();++i)if(handles[i])
            {
                const auto o=StreamingCraftOrigin(*cells[i]);
                MeshDraw draw;draw.Mesh=*handles[i];
                draw.Model=MakeTranslation(Vec3f{TerrainRelativeFloat(o.X,base.X),
                    TerrainRelativeFloat(o.Y,base.Y),TerrainRelativeFloat(o.Z,base.Z)});
                draw.CastShadows=false;scene.Add(draw);
            }
            renderer.SubmitRenderScene(scene);
        };
        const auto oldSubmit=[&](const Pub::Slots& handles){submit(handles,coords,origin);};
        const auto destroy=[&](MeshHandle h){renderer.DestroyMesh(h);};
        const auto upload=[&](CraftMeshData data)->std::optional<MeshHandle>
        {
            if(data.Indices.empty())return {};
            std::vector<MeshVertex> vertices;vertices.reserve(data.Vertices.size());
            for(const auto& v:data.Vertices)vertices.push_back({{v.Position[0],v.Position[1],v.Position[2]},
                {v.Color[0],v.Color[1],v.Color[2]},{v.Normal[0],v.Normal[1],v.Normal[2]},{v.UV[0],v.UV[1]}});
            return renderer.CreateMesh(Mesh(std::move(vertices),std::move(data.Indices)));
        };
        const auto cancel=[&](){pub.AbortStaging(destroy);workers.Cancel();pending.reset();plan.reset();};
        const auto update=[&](CaveChunk target)
        {
            if(!pub.Recover(oldSubmit,destroy))throw std::runtime_error("Cave retirement failed.");
            if(committed==target){if(pending)cancel();(void)workers.Poll();return true;}
            if(pending!=target)
            {
                cancel();plan=PlanCaveWindow<1>(coords,target);
                workers.Begin(*plan,world);pending=target;
            }
            if(!pub.Staging())
            {
                if(!workers.Poll())return false;
                if(!pub.BeginStaging(plan->Dirty,oldSubmit,destroy))return false;
            }
            std::size_t uploads=0;
            const bool ready=pub.Stage(4,[&](std::size_t i){++uploads;return upload(workers.Take(i));});
            peakUploads=std::max(peakUploads,uploads);
            if(!ready)return false;
            const auto nextOrigin=StreamingCraftOrigin(target);
            pub.CommitStaging([&](const Pub::Slots& h){submit(h,plan->Next,nextOrigin);},destroy);
            coords=plan->Next;origin=nextOrigin;committed=target;
            loads+=plan->Loaded;evictions+=plan->Evicted;retained+=plan->Retained;
            cancel();return true;
        };
        const auto settle=[&](CaveChunk target)
        {
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
            while(!update(target))
            {
                if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Cave streaming timeout.");
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        };
        const auto query=[&](VoxelCoordinate c)->std::optional<VoxelMaterial>
        {
            if(c.Y<CaveBottom||c.Y>=CaveTop)return VoxelAir;
            if(std::find(coords.begin(),coords.end(),std::optional<CaveChunk>{CaveOwner(c)})==coords.end())return {};
            return world->Get(c);
        };
        const auto edit=[&](VoxelCoordinate c,VoxelMaterial m)
        {
            if(!InCaveWorld(c)||!query(c)||world->Get(c)==m)return false;
            auto next=std::make_shared<W>(*world);
            if(!next->Set(c,m))return false;
            cancel();
            if(!pub.Apply(CaveDirty(coords,c),[&](std::size_t i){
                return upload(BuildCraftMesh(BuildVoxelChunkSurface(StreamingCraftOrigin(*coords[i]),16,
                    [&](VoxelCoordinate v){return next->Get(v);})));
            },oldSubmit,destroy))return false;
            world.swap(next);return true;
        };
        const auto camera=[&](double x,double y,double z,const std::array<double,3>& d)
        {
            CameraPose pose;
            pose.Position={TerrainRelativeFloat(x,origin.X),TerrainRelativeFloat(y,origin.Y),TerrainRelativeFloat(z,origin.Z)};
            pose.Target={TerrainRelativeFloat(x+d[0],origin.X),TerrainRelativeFloat(y+d[1],origin.Y),TerrainRelativeFloat(z+d[2],origin.Z)};
            renderer.SetCameraPose(pose);
        };
        try
        {
            settle({0,0,0});
            if(smoke)
            {
                const auto capture=[&](double x,double y,double z,const std::array<double,3>& d)
                {
                    camera(x,y,z,d);renderer.RequestViewportCapture();
                    for(unsigned i=0;i<12;++i)
                    {
                        renderer.DrawFrame();
                        if(auto image=renderer.TakeViewportCapture())
                        {
                            if(!image->IsVisuallyNonUniform())throw std::runtime_error("Blank cave capture.");
                            return;
                        }
                    }
                    throw std::runtime_error("Missing cave capture.");
                };
                capture(.5,12,12,{0,-.6,-1});
                settle({0,-1,0});
                if(loads!=36||evictions!=9||retained!=18)throw std::runtime_error("Vertical residency failed.");
                capture(8.5,-7.5,8.5,{0,0,-1});
                const VoxelCoordinate block{0,-16,0};
                const auto material=world->Get(block);
                if(material==VoxelAir||!edit(block,VoxelAir))throw std::runtime_error("Cave edit failed.");
                settle(CaveObserver(1e9,-24,-1e9));
                capture(1000000008.5,-23.5,-999999991.5,{0,0,-1});
                settle({0,-1,0});
                if(query(block)!=VoxelAir||!edit(block,material)||!world->Overrides().empty())
                    throw std::runtime_error("Cave revisit failed.");
                capture(8.5,-7.5,8.5,{0,0,-1});
                if(loads!=90||evictions!=63||peakUploads>4)throw std::runtime_error("Cave streaming accounting.");
                std::cout<<"Cave native: 90 loads, 63 evictions, 18 vertical retained, 4 captures, peak uploads "<<peakUploads<<"; edit revisit passed\n";
            }
            else
            {
                double x=8.5,y=12,z=8.5,yaw=0,pitch=-.5;
                VoxelPlayer body{{8.5,12,8.5}};
                VoxelPlayerConfig config;
                const auto bodyQuery=[&](VoxelCoordinate c)->std::optional<VoxelMaterial>{
                    if(c.Y<CaveBottom||c.Y>=CaveTop||std::abs(double(c.X))>=999999950||std::abs(double(c.Z))>=999999950)return CraftStone;
                    return query(c);
                };
                bool leftHeld=false,rightHeld=false,jumpHeld=false;
                auto previous=std::chrono::steady_clock::now();
                auto* window=renderer.NativeWindow().NativeHandle();
                const auto key=[&](int k){return glfwGetKey(window,k)==GLFW_PRESS?1.:0.;};
                while(!renderer.NativeWindow().ShouldClose())
                {
                    renderer.NativeWindow().PollEvents();if(key(GLFW_KEY_ESCAPE))break;
                    const auto now=std::chrono::steady_clock::now();
                    const double dt=std::min(.1,std::chrono::duration<double>(now-previous).count());previous=now;
                    yaw=std::remainder(yaw+(key(GLFW_KEY_RIGHT)-key(GLFW_KEY_LEFT))*1.8*dt,6.283185307179586);
                    pitch=std::clamp(pitch+(key(GLFW_KEY_UP)-key(GLFW_KEY_DOWN))*1.4*dt,-1.45,1.45);
                    const double f=key(GLFW_KEY_W)-key(GLFW_KEY_S),r=key(GLFW_KEY_D)-key(GLFW_KEY_A),n=std::max(1.,std::hypot(f,r));
                    const double dx=(std::sin(yaw)*f+std::cos(yaw)*r)/n,dz=(-std::cos(yaw)*f+std::sin(yaw)*r)/n;
                    const bool jump=key(GLFW_KEY_SPACE)>0;
                    if(survival)
                    {
                        AdvanceVoxelPlayer(body,config,{dx,dz,jump&&!jumpHeld,key(GLFW_KEY_LEFT_SHIFT)>0},dt,bodyQuery);
                        x=body.Feet[0];y=body.Feet[1]+config.EyeHeight;z=body.Feet[2];
                    }
                    else
                    {
                        x=std::clamp(x+12*dt*dx,double(origin.X)-15.,double(origin.X)+30.);
                        z=std::clamp(z+12*dt*dz,double(origin.Z)-15.,double(origin.Z)+30.);
                        y=std::clamp(y+12*dt*(key(GLFW_KEY_SPACE)-key(GLFW_KEY_LEFT_CONTROL)),
                            std::max(-63.,double(origin.Y)-15.),std::min(31.,double(origin.Y)+30.));
                    }
                    jumpHeld=jump;
                    (void)update(CaveObserver(x,survival?body.Feet[1]:y,z));
                    const std::array d{std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
                    camera(x,y,z,d);
                    const bool left=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
                    const bool right=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS;
                    if((left&&!leftHeld)||(right&&!rightHeld))
                        if(auto hit=TraceVoxels({x,y,z},d,8,64,query))
                        {
                            if(left&&!leftHeld)(void)edit(hit->Cell,VoxelAir);
                            else if(hit->Placement&&(!survival||!PlayerIntersectsBlock(body,config,*hit->Placement)))
                                (void)edit(*hit->Placement,CraftPlaced);
                        }
                    leftHeld=left;rightHeld=right;renderer.DrawFrame();
                }
            }
        }
        catch(...){(void)pub.Release(oldSubmit,destroy);throw;}
        if(pub.Release(oldSubmit,destroy)!=0)throw std::runtime_error("Cave cleanup failed.");
        return 0;
    }
    catch(const std::exception& e){std::cerr<<"Craft Caves failed: "<<e.what()<<'\n';return 1;}
}
