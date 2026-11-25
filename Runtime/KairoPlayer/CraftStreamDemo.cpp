#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "CraftStreamingWindow.hpp"
#include "VoxelMeshPublication.hpp"
#include "TerrainRenderOrigin.hpp"
#include <chrono>
#include <charconv>
#include <iostream>
#include <memory>
#include <string_view>

import Kairo.Renderer;
import Kairo.Foundation.Math;

int main(int argc,char** argv)
{
    try
    {
        using namespace kairo::renderer;
        using namespace kairo::foundation::math;
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
            else throw std::invalid_argument("Usage: KairoCraftStream [--seed N] [--renderer backend] [--smoke]");
        }
        RendererRuntime renderer({"Kairo Craft Stream: WASD / arrows / Space / Ctrl / mouse / Esc",960u,540u,!smoke,backend});
        renderer.ResizeViewport(960,540);
        renderer.SetViewportShadingMode(ViewportShadingMode::Lit);
        using World=VoxelEdits<StreamingCraftBaseline>;
        using Plan=CraftWindowPlan<2>;
        using Publication=VoxelMeshPublication<MeshHandle,Plan::Capacity>;
        auto world=std::make_unique<World>(StreamingCraftBaseline{seed});
        Plan::Slots coordinates{};
        Publication publication;
        VoxelCoordinate origin{};
        std::optional<CraftColumn> center;
        std::size_t loads=0,evictions=0,retained=0,edits=0;
        RenderScene authored;
        RenderLight sun;
        sun.Type=RenderLightType::Directional;
        sun.Direction={.6f,1.f,.4f};sun.Intensity=3.f;sun.CastShadows=false;
        authored.AddLight(sun);
        const auto submit=[&](const Publication::Slots& slots,const Plan::Slots& coords,VoxelCoordinate base)
        {
            auto scene=authored;
            for(std::size_t i=0;i<slots.size();++i)if(slots[i])
            {
                if(!coords[i])throw std::logic_error("Craft native slot lacks coordinate.");
                const auto cell=StreamingCraftOrigin(*coords[i]);
                MeshDraw draw;
                draw.Mesh=*slots[i];
                draw.Model=MakeTranslation(Vec3f{TerrainRelativeFloat(cell.X,base.X),
                    TerrainRelativeFloat(cell.Y,base.Y),TerrainRelativeFloat(cell.Z,base.Z)});
                draw.CastShadows=false;
                scene.Add(draw);
            }
            renderer.SubmitRenderScene(scene);
        };
        const auto committedSubmit=[&](const Publication::Slots& slots){submit(slots,coordinates,origin);};
        const auto destroy=[&](MeshHandle h){renderer.DestroyMesh(h);};
        const auto upload=[&](const World& source,CraftColumn cell)->std::optional<MeshHandle>
        {
            // Full deterministic halo, including nonresident neighbors.
            // Streaming itself does not change any existing mesh's faces.
            const auto surface=BuildVoxelChunkSurface(StreamingCraftOrigin(cell),CraftExtent,
                [&](VoxelCoordinate c){return source.Get(c);});
            auto mesh=BuildCraftMesh(surface);
            if(mesh.Indices.empty())return {};
            std::vector<MeshVertex> vertices;
            vertices.reserve(mesh.Vertices.size());
            for(const auto& v:mesh.Vertices)
                vertices.push_back({{v.Position[0],v.Position[1],v.Position[2]},
                    {v.Color[0],v.Color[1],v.Color[2]},
                    {v.Normal[0],v.Normal[1],v.Normal[2]},
                    {v.UV[0],v.UV[1]}});
            return renderer.CreateMesh(Mesh(std::move(vertices),std::move(mesh.Indices)));
        };
        const auto updateWindow=[&](double x,double z)
        {
            // Restore old mapping before staging a new mapping; a partially
            // failed scene must never recover using the new slot coordinates.
            if(!publication.Recover(committedSubmit,destroy))
                throw std::runtime_error("Craft retirement retry failed.");
            const auto nextCenter=CraftObserverColumn(x,z);
            if(center&&*center==nextCenter)return;
            const auto plan=PlanCraftWindow<2>(coordinates,nextCenter);
            const auto nextOrigin=StreamingCraftOrigin(nextCenter);
            if(!publication.Apply(plan.Dirty,[&](std::size_t i){return upload(*world,*plan.Next[i]);},
                [&](const Publication::Slots& slots){submit(slots,plan.Next,nextOrigin);},destroy))
                throw std::runtime_error("Craft window publication deferred.");
            coordinates=plan.Next;origin=nextOrigin;center=nextCenter;
            loads+=plan.Loaded;evictions+=plan.Evicted;retained+=plan.Retained;
        };
        const auto query=[&](VoxelCoordinate cell)->std::optional<VoxelMaterial>
        {
            const auto owner=CraftOwner(cell);
            if(std::find(coordinates.begin(),coordinates.end(),std::optional<CraftColumn>{owner})==coordinates.end())
                return {};
            return world->Get(cell);
        };
        const auto edit=[&](VoxelCoordinate cell,VoxelMaterial material)
        {
            if(cell.Y<0||cell.Y>=CraftExtent||!query(cell)||world->Get(cell)==material)return false;
            auto next=std::make_unique<World>(*world);
            if(!next->Set(cell,material))return false;
            if(!publication.Apply(StreamingCraftDirty(coordinates,cell),
                [&](std::size_t i){return upload(*next,*coordinates[i]);},committedSubmit,destroy))return false;
            world.swap(next);++edits;
            return true;
        };
        const auto camera=[&](double x,double y,double z,const std::array<double,3>& direction,bool overhead=false)
        {
            CameraPose view;
            view.Position={TerrainRelativeFloat(x,origin.X),TerrainRelativeFloat(y,origin.Y),TerrainRelativeFloat(z,origin.Z)};
            view.Target={TerrainRelativeFloat(x+direction[0],origin.X),
                TerrainRelativeFloat(y+direction[1],origin.Y),TerrainRelativeFloat(z+direction[2],origin.Z)};
            if(overhead)view.Up={0.f,0.f,-1.f};
            renderer.SetCameraPose(view);
        };
        try
        {
            updateWindow(.5,.5);
            if(smoke)
            {
                const auto capture=[&](double x,double z)
                {
                    camera(x,12,z,{0,-1,0},true);
                    renderer.RequestViewportCapture();
                    for(unsigned i=0;i<12;++i)
                    {
                        renderer.DrawFrame();
                        if(auto image=renderer.TakeViewportCapture())
                        {
                            if(!image->IsVisuallyNonUniform())throw std::runtime_error("Streamed Craft capture blank.");
                            return *image;
                        }
                    }
                    throw std::runtime_error("Streamed Craft capture unavailable.");
                };
                const auto baseline=capture(.5,.5);
                const auto hit=TraceVoxels({.5,12,.5},{0,-1,0},12,32,query);
                if(!hit||!edit(hit->Cell,CraftPlaced))throw std::runtime_error("Stream edit failed.");
                const auto changed=capture(.5,.5);
                if(changed.RGBA==baseline.RGBA)throw std::runtime_error("Stream edit image unchanged.");
                updateWindow(16.5,.5);
                if(loads!=30||evictions!=5||retained!=20)
                    throw std::runtime_error("One-cell stream did not retain overlap.");
                (void)capture(16.5,.5);
                updateWindow(1000000000.5,-999999999.5);
                if(query(hit->Cell)||world->Get(hit->Cell)!=CraftPlaced)
                    throw std::runtime_error("Evicted edit lost or query escaped residency.");
                (void)capture(1000000000.5,-999999999.5);
                updateWindow(.5,.5);
                const auto revisited=capture(.5,.5);
                if(revisited.RGBA!=changed.RGBA||query(hit->Cell)!=CraftPlaced)
                    throw std::runtime_error("Revisit failed to restore edit.");
                if(!edit(hit->Cell,hit->Material)||!world->Overrides().empty())
                    throw std::runtime_error("Revisit edit restore failed.");
                if(capture(.5,.5).RGBA!=baseline.RGBA)
                    throw std::runtime_error("Restored streamed image differs.");
                if(loads!=80||evictions!=55||edits!=2)throw std::runtime_error("Stream accounting mismatch.");
                std::cout<<"Streamed Craft: 80 loads, 55 evictions, 20 retained, 6 captures, billion-meter revisit and edit restore passed\n";
            }
            else
            {
                double x=.5,y=12,z=.5,yaw=0,pitch=-.5;
                bool leftHeld=false,rightHeld=false;
                auto previous=std::chrono::steady_clock::now();
                auto* window=renderer.NativeWindow().NativeHandle();
                const auto key=[&](int k){return glfwGetKey(window,k)==GLFW_PRESS?1.:0.;};
                while(!renderer.NativeWindow().ShouldClose())
                {
                    renderer.NativeWindow().PollEvents();
                    if(key(GLFW_KEY_ESCAPE))break;
                    const auto now=std::chrono::steady_clock::now();
                    const double dt=std::min(.1,std::chrono::duration<double>(now-previous).count());
                    previous=now;
                    yaw=std::remainder(yaw+(key(GLFW_KEY_RIGHT)-key(GLFW_KEY_LEFT))*1.8*dt,6.283185307179586);
                    pitch=std::clamp(pitch+(key(GLFW_KEY_UP)-key(GLFW_KEY_DOWN))*1.4*dt,-1.45,1.45);
                    const double f=key(GLFW_KEY_W)-key(GLFW_KEY_S),r=key(GLFW_KEY_D)-key(GLFW_KEY_A);
                    const double scale=12*dt/std::max(1.,std::hypot(f,r));
                    x=std::clamp(x+(std::sin(yaw)*f+std::cos(yaw)*r)*scale,-1e9,1e9);
                    z=std::clamp(z+(-std::cos(yaw)*f+std::sin(yaw)*r)*scale,-1e9,1e9);
                    y=std::clamp(y+(key(GLFW_KEY_SPACE)-key(GLFW_KEY_LEFT_CONTROL))*12*dt,1.,32.);
                    updateWindow(x,z);
                    const std::array direction{std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
                    camera(x,y,z,direction);
                    const bool left=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
                    const bool right=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS;
                    if((left&&!leftHeld)||(right&&!rightHeld))
                    {
                        const auto hit=TraceVoxels({x,y,z},direction,8,64,query);
                        if(hit)
                        {
                            if(left&&!leftHeld)(void)edit(hit->Cell,VoxelAir);
                            else if(hit->Placement)(void)edit(*hit->Placement,CraftPlaced);
                        }
                    }
                    leftHeld=left;rightHeld=right;
                    renderer.DrawFrame();
                }
            }
        }
        catch(...){(void)publication.Release(committedSubmit,destroy);throw;}
        if(publication.Release(committedSubmit,destroy)!=0)throw std::runtime_error("Streamed Craft shutdown retained meshes.");
        return 0;
    }
    catch(const std::exception& e){std::cerr<<"Streamed Craft failed: "<<e.what()<<'\n';return 1;}
}
