#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "CraftStreamingWindow.hpp"
#include "CraftStreamSave.hpp"
#include "CraftMeshBatch.hpp"
#include <thread>
#include "VoxelPlayer.hpp"
#include <filesystem>
#include <fstream>
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
        bool smoke=false,survival=false;
        unsigned workers=0;
        std::optional<std::filesystem::path> savePath;
        std::uint64_t seed=42;
        GraphicsBackend backend=GraphicsBackend::Automatic;
        for(int i=1;i<argc;++i)
        {
            const std::string_view arg=argv[i];
            if(arg=="--smoke")smoke=true;
            else if(arg=="--survival")survival=true;
            else if(arg=="--workers"&&i+1<argc)
            {
                const std::string_view value=argv[++i];
                const auto result=std::from_chars(value.data(),value.data()+value.size(),workers);
                if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size()||workers<1||workers>8)
                    throw std::invalid_argument("Craft workers must be 1..8.");
            }
            else if(arg=="--save"&&i+1<argc)savePath=std::filesystem::path(argv[++i]);
            else if(arg=="--renderer"&&i+1<argc)backend=ParseGraphicsBackend(argv[++i]);
            else if(arg=="--seed"&&i+1<argc)
            {
                const std::string_view value=argv[++i];
                const auto result=std::from_chars(value.data(),value.data()+value.size(),seed);
                if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size())
                    throw std::invalid_argument("Invalid seed.");
            }
            else throw std::invalid_argument("Usage: KairoCraftStream [--seed N] [--renderer backend] [--survival] [--workers N] [--save path] [--smoke]");
        }
        RendererRuntime renderer({"Kairo Craft Stream: WASD / arrows / Space / Ctrl / mouse / Esc",960u,540u,!smoke,backend});
        renderer.ResizeViewport(960,540);
        renderer.SetViewportShadingMode(ViewportShadingMode::Lit);
        using World=VoxelEdits<StreamingCraftBaseline>;
        using Plan=CraftWindowPlan<2>;
        using Publication=VoxelMeshPublication<MeshHandle,Plan::Capacity>;
        auto world=std::make_unique<World>(StreamingCraftBaseline{seed});
        if(smoke&&savePath)throw std::invalid_argument("Smoke mode does not touch user saves.");
        if(savePath&&std::filesystem::exists(*savePath))
        {
            const auto size=std::filesystem::file_size(*savePath);
            if(size>CraftSaveMaximumBytes)throw std::length_error("Stream save size cap.");
            std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
            std::ifstream file(*savePath,std::ios::binary);
            if(!file||!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))||
                file.peek()!=std::char_traits<char>::eof())
                throw std::runtime_error("Stream save read failed.");
            auto decoded=DecodeStreamCraftSave(bytes);
            seed=decoded.Seed;world=std::make_unique<World>(std::move(decoded.World));
        }
        const auto save=[&]()
        {
            if(!savePath)return;
            const auto bytes=EncodeStreamCraftSave(seed,*world);
            auto temporary=*savePath;temporary+=".tmp";
            if(std::filesystem::exists(temporary))throw std::runtime_error("Temporary save already exists.");
            try
            {
                std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
                if(!file||!file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
                    throw std::runtime_error("Stream save write failed.");
                file.close();
                if(!file)throw std::runtime_error("Stream save close failed.");
                std::filesystem::rename(temporary,*savePath);
            }
            catch(...)
            {
                std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;
            }
            std::cout<<"Streamed Craft saved "<<world->Overrides().size()<<" edits\n";
        };
        Plan::Slots coordinates{};
        Publication publication;
        VoxelCoordinate origin{};
        std::optional<CraftColumn> center,pendingCenter;
        std::optional<Plan> pendingPlan;
        std::unique_ptr<CraftMeshBatch<2>> batch;
        if(workers)batch=std::make_unique<CraftMeshBatch<2>>(workers);
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
        const auto uploadMesh=[&](CraftMeshData mesh)->std::optional<MeshHandle>
        {
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
        const auto upload=[&](const World& source,CraftColumn cell)
        {
            return uploadMesh(BuildCraftMesh(BuildVoxelChunkSurface(StreamingCraftOrigin(cell),CraftExtent,
                [&](VoxelCoordinate c){return source.Get(c);})));
        };
        const auto cancelPending=[&]()
        {
            if(batch)batch->Cancel();
            pendingCenter.reset();pendingPlan.reset();
        };
        const auto updateWindow=[&](double x,double z)->bool
        {
            // Restore old mapping before staging a new mapping; a partially
            // failed scene must never recover using the new slot coordinates.
            if(!publication.Recover(committedSubmit,destroy))
                throw std::runtime_error("Craft retirement retry failed.");
            const auto nextCenter=CraftObserverColumn(x,z);
            if(center&&*center==nextCenter)
            {
                if(pendingCenter)cancelPending();
                if(batch)(void)batch->Poll(); // drain obsolete completions
                return true;
            }
            if(batch)
            {
                if(!pendingCenter||*pendingCenter!=nextCenter)
                {
                    pendingPlan=PlanCraftWindow<2>(coordinates,nextCenter);
                    batch->Begin(*pendingPlan,std::make_shared<const World>(*world));
                    pendingCenter=nextCenter;
                }
                if(!batch->Poll())return false;
            }
            const auto plan=batch?*pendingPlan:PlanCraftWindow<2>(coordinates,nextCenter);
            const auto nextOrigin=StreamingCraftOrigin(nextCenter);
            if(!publication.Apply(plan.Dirty,[&](std::size_t i){
                    return batch?uploadMesh(batch->Take(i)):upload(*world,*plan.Next[i]);
                },[&](const Publication::Slots& slots){submit(slots,plan.Next,nextOrigin);},destroy))
                throw std::runtime_error("Craft window publication deferred.");
            coordinates=plan.Next;origin=nextOrigin;center=nextCenter;
            loads+=plan.Loaded;evictions+=plan.Evicted;retained+=plan.Retained;
            if(batch)cancelPending();
            return true;
        };
        const auto settleWindow=[&](double x,double z)
        {
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
            while(!updateWindow(x,z))
            {
                if(std::chrono::steady_clock::now()>deadline)
                    throw std::runtime_error("Craft meshing batch timed out.");
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        };
        const auto query=[&](VoxelCoordinate cell)->std::optional<VoxelMaterial>
        {
            const auto owner=CraftOwner(cell);
            if(std::find(coordinates.begin(),coordinates.end(),std::optional<CraftColumn>{owner})==coordinates.end())
                return {};
            return world->Get(cell);
        };
        const VoxelPlayerConfig bodyConfig;
        const auto bodyQuery=[&](VoxelCoordinate c)->std::optional<VoxelMaterial>
        {
            if(!query(c))return {};
            if(c.Y<0||std::abs(static_cast<double>(c.X))>=999999999.0||
                std::abs(static_cast<double>(c.Z))>=999999999.0)return CraftStone;
            return query(c);
        };
        const auto spawn=[&]()
        {
            VoxelPlayer p{{.5,0,.5}};
            for(int y=CraftExtent-1;y>=0;--y)
                if(world->Get({0,y,0})!=VoxelAir){p.Feet[1]=y+1.;break;}
            AdvanceVoxelPlayer(p,bodyConfig,{},0,bodyQuery);
            return p;
        };
        const auto edit=[&](VoxelCoordinate cell,VoxelMaterial material)
        {
            if(!InStreamCraftWorld(cell)||!query(cell)||world->Get(cell)==material)return false;
            auto next=std::make_unique<World>(*world);
            if(!next->Set(cell,material))return false;
            if(!publication.Apply(StreamingCraftDirty(coordinates,cell),
                [&](std::size_t i){return upload(*next,*coordinates[i]);},committedSubmit,destroy))return false;
            world.swap(next);++edits;
            if(batch)cancelPending(); // old snapshots no longer describe the committed world
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
            settleWindow(.5,.5);
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
                settleWindow(16.5,.5);
                if(loads!=30||evictions!=5||retained!=20)
                    throw std::runtime_error("One-cell stream did not retain overlap.");
                (void)capture(16.5,.5);
                settleWindow(1000000000.5,-999999999.5);
                if(query(hit->Cell)||world->Get(hit->Cell)!=CraftPlaced)
                    throw std::runtime_error("Evicted edit lost or query escaped residency.");
                (void)capture(1000000000.5,-999999999.5);
                settleWindow(.5,.5);
                const auto decoded=DecodeStreamCraftSave(EncodeStreamCraftSave(seed,*world));
                if(decoded.Seed!=seed||decoded.World.Overrides()!=world->Overrides())
                    throw std::runtime_error("Stream save roundtrip failed.");
                const auto revisited=capture(.5,.5);
                if(revisited.RGBA!=changed.RGBA||query(hit->Cell)!=CraftPlaced)
                    throw std::runtime_error("Revisit failed to restore edit.");
                if(!edit(hit->Cell,hit->Material)||!world->Overrides().empty())
                    throw std::runtime_error("Revisit edit restore failed.");
                if(capture(.5,.5).RGBA!=baseline.RGBA)
                    throw std::runtime_error("Restored streamed image differs.");
                if(loads!=80||evictions!=55||edits!=2)throw std::runtime_error("Stream accounting mismatch.");
                if(survival)
                {
                    auto p=spawn();
                    const auto start=p.Feet[1];
                    double peak=0;
                    for(unsigned i=0;i<240;++i)
                    {
                        AdvanceVoxelPlayer(p,bodyConfig,{0,0,i==0,false},1./120,bodyQuery);
                        peak=std::max(peak,p.Feet[1]-start);
                        if(VoxelPlayerBlocked(p,bodyConfig,bodyQuery))throw std::runtime_error("Streamed body penetration.");
                    }
                    if(!p.Grounded||peak<1.2||peak>1.3)throw std::runtime_error("Streamed survival jump failed.");
                    std::cout<<"Streamed survival apex "<<peak<<" m; landing passed\n";
                }
                if(batch)std::cout<<"Craft worker batch mode: "<<workers<<" workers, "<<batch->StaleCount()<<" discarded jobs\n";
                std::cout<<"Streamed Craft: 80 loads, 55 evictions, 20 retained, 6 captures, billion-meter revisit and edit restore passed\n";
            }
            else
            {
                double x=.5,y=12,z=.5,yaw=0,pitch=-.5;
                auto player=spawn();
                bool jumpHeld=false,saveHeld=false;
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
                    const bool jump=key(GLFW_KEY_SPACE)>0;
                    if(survival)
                    {
                        const double norm=std::max(1.,std::hypot(f,r));
                        AdvanceVoxelPlayer(player,bodyConfig,
                            {(std::sin(yaw)*f+std::cos(yaw)*r)/norm,
                            (-std::cos(yaw)*f+std::sin(yaw)*r)/norm,
                            jump&&!jumpHeld,key(GLFW_KEY_LEFT_SHIFT)>0},dt,bodyQuery);
                        x=player.Feet[0];y=player.Feet[1]+bodyConfig.EyeHeight;z=player.Feet[2];
                    }
                    else
                    {
                        x=std::clamp(x+(std::sin(yaw)*f+std::cos(yaw)*r)*scale,-1e9,1e9);
                        z=std::clamp(z+(-std::cos(yaw)*f+std::sin(yaw)*r)*scale,-1e9,1e9);
                        y=std::clamp(y+(key(GLFW_KEY_SPACE)-key(GLFW_KEY_LEFT_CONTROL))*12*dt,1.,32.);
                    }
                    jumpHeld=jump;
                    // A slow worker must not allow creative flight to
                    // outrun the currently published window. The central
                    // 3x3 region remains reachable while jobs run.
                    if(batch&&center&&!survival)
                    {
                        const auto base=StreamingCraftOrigin(*center);
                        x=std::clamp(x,double(base.X)-16.,double(base.X)+31.999);
                        z=std::clamp(z,double(base.Z)-16.,double(base.Z)+31.999);
                    }
                    (void)updateWindow(x,z);
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
                            else if(hit->Placement&&(!survival||!PlayerIntersectsBlock(player,bodyConfig,*hit->Placement)))
                                (void)edit(*hit->Placement,CraftPlaced);
                        }
                    }
                    leftHeld=left;rightHeld=right;
                    const bool saveDown=key(GLFW_KEY_F5)>0;
                    if(saveDown&&!saveHeld){try{save();}catch(const std::exception& ex){std::cerr<<"Save failed: "<<ex.what()<<'\n';}}
                    saveHeld=saveDown;
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
