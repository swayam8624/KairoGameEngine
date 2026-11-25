#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "CraftStreamingWindow.hpp"
#include "CraftStreamSave.hpp"
#include "CraftInventory.hpp"
#include "CraftRecipesHud.hpp"
#include "CraftHudProjection.hpp"
#include "CraftHudPublication.hpp"
#include "CraftVitals.hpp"
#include "CraftMining.hpp"
#include "CraftEquipment.hpp"
#include "CraftEquipmentSave.hpp"
#include "TerrainScreenError.hpp"
#include <string>
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
        unsigned workers=0,uploadBudget=4;
        std::optional<std::filesystem::path> savePath;
        std::uint64_t seed=42;
        GraphicsBackend backend=GraphicsBackend::Automatic;
        for(int i=1;i<argc;++i)
        {
            const std::string_view arg=argv[i];
            if(arg=="--smoke")smoke=true;
            else if(arg=="--survival")survival=true;
            else if(arg=="--uploads"&&i+1<argc)
            {
                const std::string_view value=argv[++i];
                const auto result=std::from_chars(value.data(),value.data()+value.size(),uploadBudget);
                if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size()||uploadBudget<1||uploadBudget>25)
                    throw std::invalid_argument("Craft uploads must be 1..25.");
            }
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
            else throw std::invalid_argument("Usage: KairoCraftStream [--seed N] [--renderer backend] [--survival] [--workers N] [--uploads N] [--save path] [--smoke]");
        }
        RendererRuntime renderer({"Kairo Craft Stream: WASD / arrows / Space / Ctrl / mouse / Esc",960u,540u,!smoke,backend});
        CraftHudViewport hudViewport;
        const bool nativeHud=renderer.SupportsViewportOverlay();
        renderer.ResizeViewport(hudViewport.Width,hudViewport.Height);
        renderer.SetViewportShadingMode(ViewportShadingMode::Lit);
        using World=VoxelEdits<StreamingCraftBaseline>;
        using Plan=CraftWindowPlan<2>;
        using Publication=VoxelMeshPublication<MeshHandle,Plan::Capacity>;
        auto world=std::make_unique<World>(StreamingCraftBaseline{seed});
        CraftInventory inventory;
        CraftEquipment equipment;
        if(smoke&&savePath)throw std::invalid_argument("Smoke mode does not touch user saves.");
        if(savePath&&std::filesystem::exists(*savePath))
        {
            const auto size=std::filesystem::file_size(*savePath);
            if(size>CraftSessionMaximumBytes+CraftEquipmentOverhead)throw std::length_error("Stream save size cap.");
            std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
            std::ifstream file(*savePath,std::ios::binary);
            if(!file||!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))||
                file.peek()!=std::char_traits<char>::eof())
                throw std::runtime_error("Stream save read failed.");
            std::span<const std::uint8_t> payload=bytes;
            if(HasCraftEquipmentEnvelope(payload))
            {
                const auto decoded=DecodeCraftEquipmentEnvelope(payload,CraftSessionMaximumBytes);
                equipment=decoded.Equipment;payload=decoded.Payload;
            }
            if(payload.size()>=CraftSessionMagic.size()&&
                std::equal(CraftSessionMagic.begin(),CraftSessionMagic.end(),payload.begin()))
            {
                auto decoded=DecodeCraftSession(payload);
                seed=decoded.Seed;world=std::make_unique<World>(std::move(decoded.World));
                inventory=decoded.Inventory;
            }
            else
            {
                auto decoded=DecodeStreamCraftSave(payload);
                seed=decoded.Seed;world=std::make_unique<World>(std::move(decoded.World));
            }
        }
        const auto save=[&]()
        {
            if(!savePath)return;
            const auto payload=EncodeCraftSession(seed,*world,inventory);
            const auto bytes=EncodeCraftEquipmentEnvelope(payload,equipment,CraftSessionMaximumBytes);
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
        std::size_t loads=0,evictions=0,retained=0,edits=0,peakUploads=0;
        RenderScene authored;
        RenderLight sun;
        sun.Type=RenderLightType::Directional;
        sun.Direction={.6f,1.f,.4f};sun.Intensity=3.f;sun.CastShadows=false;
        authored.AddLight(sun);
        std::optional<MeshHandle> hudMesh;
        std::optional<CameraPose> hudPose;
        bool hudEnabled=false;
        CraftHudPublication hudPublication;
        CraftVitals vitals;
        CraftMining mining;
        const auto refreshNativeHud=[&]()
        {
            if(!nativeHud)return;
            const CraftHudKey key{hudEnabled,inventory.Counts,inventory.Selected,hudViewport.Width,hudViewport.Height,vitals.Health(),mining.Progress(),equipment.Durability,equipment.Equipped};
            (void)hudPublication.Refresh(key,[&]()
            {
                std::vector<ViewportOverlayRect> rects;
                if(hudEnabled)
                {
                    const auto layout=BuildCraftHud(inventory,vitals.Health(),mining.Progress(),equipment.Durability,equipment.Equipped);
                    rects.reserve(layout.size());
                    // Paint opaque backing first, then icons and numerals.
                    // No camera, world origin or depth state is involved.
                    for(bool backing:{true,false})for(const auto& rect:layout)
                    {
                        if(rect.Background!=backing)continue;
                        const auto p=QuantizeCraftHudRect(hudViewport,rect.X,rect.Y,rect.W,rect.H,rect.Y>=430);
                        if(p.Width&&p.Height)rects.push_back({p.X,p.Y,p.Width,p.Height,rect.Color});
                    }
                }
                renderer.SetViewportOverlay(rects);
            });
        };
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
            if(!nativeHud&&hudEnabled&&hudMesh&&hudPose)
            {
                const auto& pose=*hudPose;
                const ScreenVec eye{pose.Position.x,pose.Position.y,pose.Position.z};
                const auto forward=ScreenUnit(ScreenVec{double(pose.Target.x)-eye[0],
                    double(pose.Target.y)-eye[1],double(pose.Target.z)-eye[2]});
                const auto right=ScreenUnit(ScreenCross(forward,{pose.Up.x,pose.Up.y,pose.Up.z}));
                const auto up=ScreenUnit(ScreenCross(right,forward));
                for(const auto& rect:BuildCraftHud(inventory,vitals.Health(),mining.Progress(),equipment.Durability,equipment.Equipped))
                {
                    const double depth=rect.Background?.16:.14;
                    const auto projected=ProjectCraftHudRect(hudViewport,rect.X,rect.Y,rect.W,rect.H,
                        pose.VerticalFovRadians,depth,rect.Y>=430);
                    const double dx=projected.X,dy=projected.Y;
                    const double width=projected.Width,height=projected.Height;
                    MeshDraw draw;
                    draw.Mesh=*hudMesh;draw.Model=Mat4f::Identity();
                    for(unsigned axis=0;axis<3;++axis)
                    {
                        draw.Model(axis,0)=static_cast<float>(right[axis]*width);
                        draw.Model(axis,1)=static_cast<float>(up[axis]*height);
                        draw.Model(axis,2)=static_cast<float>(-forward[axis]*std::max(width,height));
                        draw.Model(axis,3)=static_cast<float>(eye[axis]+forward[axis]*depth+right[axis]*dx+up[axis]*dy);
                    }
                    draw.Material.BaseColor={0,0,0};
                    draw.Material.Emissive={rect.Color[0],rect.Color[1],rect.Color[2]};
                    draw.Material.DoubleSided=true;draw.CastShadows=false;draw.ReceiveShadows=false;
                    scene.Add(draw);
                }
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
            publication.AbortStaging(destroy);
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
                    cancelPending();
                    pendingPlan=PlanCraftWindow<2>(coordinates,nextCenter);
                    batch->Begin(*pendingPlan,std::make_shared<const World>(*world));
                    pendingCenter=nextCenter;
                }
                if(!publication.Staging())
                {
                    if(!batch->Poll())return false;
                    if(!publication.BeginStaging(pendingPlan->Dirty,committedSubmit,destroy))return false;
                }
            }
            const auto plan=batch?*pendingPlan:PlanCraftWindow<2>(coordinates,nextCenter);
            const auto nextOrigin=StreamingCraftOrigin(nextCenter);
            const auto nextSubmit=[&](const Publication::Slots& slots){submit(slots,plan.Next,nextOrigin);};
            if(batch)
            {
                std::size_t uploads=0;
                const bool complete=publication.Stage(uploadBudget,[&](std::size_t i){
                    ++uploads;return uploadMesh(batch->Take(i));
                });
                peakUploads=std::max(peakUploads,uploads);
                if(!complete)return false;
                publication.CommitStaging(nextSubmit,destroy);
            }
            else if(!publication.Apply(plan.Dirty,[&](std::size_t i){return upload(*world,*plan.Next[i]);},
                nextSubmit,destroy))throw std::runtime_error("Craft window publication deferred.");
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
            if(batch)cancelPending(); // abort unsubmitted uploads before editing the committed scene
            if(!publication.Apply(StreamingCraftDirty(coordinates,cell),
                [&](std::size_t i){return upload(*next,*coordinates[i]);},committedSubmit,destroy))return false;
            world.swap(next);++edits;

            return true;
        };
        const auto createHud=[&]()
        {
            if(nativeHud||hudMesh)return;
            std::vector<MeshVertex> vertices{
                {{-.5f,-.5f,0},{1,1,1},{0,0,1},{0,0}},
                {{ .5f,-.5f,0},{1,1,1},{0,0,1},{1,0}},
                {{ .5f, .5f,0},{1,1,1},{0,0,1},{1,1}},
                {{-.5f, .5f,0},{1,1,1},{0,0,1},{0,1}}};
            hudMesh=renderer.CreateMesh(Mesh(std::move(vertices),{0,1,2,0,2,3}));
        };
        const auto camera=[&](double x,double y,double z,const std::array<double,3>& direction,bool overhead=false)
        {
            CameraPose view;
            view.Position={TerrainRelativeFloat(x,origin.X),TerrainRelativeFloat(y,origin.Y),TerrainRelativeFloat(z,origin.Z)};
            view.Target={TerrainRelativeFloat(x+direction[0],origin.X),
                TerrainRelativeFloat(y+direction[1],origin.Y),TerrainRelativeFloat(z+direction[2],origin.Z)};
            if(overhead)view.Up={0.f,0.f,-1.f};
            renderer.SetCameraPose(view);
            hudPose=view;
            if(nativeHud)refreshNativeHud();
            else if(hudEnabled)committedSubmit(publication.Active());
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
                    const auto selected=TraceVoxels({.5,12,.5},{0,-1,0},12,32,query);
                    if(!selected)throw std::runtime_error("Missing survival mining target.");
                    const auto cell=selected->Cell;
                    const CraftMiningTarget target{{cell.X,cell.Y,cell.Z},selected->Material};
                    const auto holds=static_cast<unsigned>(std::ceil(CraftMiningSeconds(target.Material)/.1-1e-9));
                    for(unsigned step=0;step<holds;++step)
                    {
                        const bool mined=mining.Advance(true,target,.1,[&](const auto& completed){
                            return completed==target&&MineCraftBlock(inventory,cell,query,edit);
                        });
                        const bool last=step+1==holds;
                        if(mined!=last||inventory.Count(target.Material)!=(last?1u:0u)||
                            query(cell)!=(last?VoxelAir:target.Material))
                            throw std::runtime_error("Native timed mining transaction differed.");
                    }
                    inventory.Selected=selected->Material;
                    const auto session=DecodeCraftSession(EncodeCraftSession(seed,*world,inventory));
                    if(session.Inventory.Count(inventory.Selected)!=1||
                        session.World.Overrides()!=world->Overrides())
                        throw std::runtime_error("Survival inventory persistence failed.");
                    const auto target=TraceVoxels({.5,12,.5},{0,-1,0},12,32,query);
                    if(!target||target->Placement!=selected->Cell||
                        !PlaceCraftBlock(inventory,*target->Placement,query,edit,[](auto){return true;})||
                        inventory.Count(selected->Material)!=0||!world->Overrides().empty())
                        throw std::runtime_error("Survival placement did not consume inventory.");
                    if(capture(.5,.5).RGBA!=baseline.RGBA)
                        throw std::runtime_error("Survival inventory edit did not restore image.");
                    std::cout<<"Survival mining, inventory save roundtrip and material-consuming placement passed\n";
                    createHud();hudEnabled=true;
                    const auto withHud=capture(.5,.5);
                    if(withHud.RGBA==baseline.RGBA)throw std::runtime_error("HUD did not appear in native capture.");
                    hudEnabled=false;committedSubmit(publication.Active());
                    if(capture(.5,.5).RGBA!=baseline.RGBA)throw std::runtime_error("HUD removal changed world.");
                    // Exercise actual native target resize in both aspect directions,
                    // independently of interactive window manager behavior.
                    for(const CraftHudViewport extent: {CraftHudViewport{480,720},CraftHudViewport{1280,480}})
                    {
                        renderer.ResizeViewport(extent.Width,extent.Height);hudViewport=extent;
                        const auto without=capture(.5,.5);
                        if(without.Width!=extent.Width||without.Height!=extent.Height)
                            throw std::runtime_error("HUD resize capture extent differs.");
                        hudEnabled=true;
                        const auto with=capture(.5,.5);
                        if(with.Width!=extent.Width||with.Height!=extent.Height||with.RGBA==without.RGBA)
                            throw std::runtime_error("HUD did not render after viewport resize.");
                        hudEnabled=false;committedSubmit(publication.Active());
                        if(capture(.5,.5).RGBA!=without.RGBA)
                            throw std::runtime_error("Resized HUD removal changed world.");
                    }
                    renderer.ResizeViewport(960,540);hudViewport={};
                    if(capture(.5,.5).RGBA!=baseline.RGBA)
                        throw std::runtime_error("Restored viewport changed baseline.");
                    std::cout<<"Native portrait and wide HUD resize captures passed\n";
                    CraftInventory recipeFixture;recipeFixture.Counts={0,1,2,0};
                    if(!CraftApplyRecipe(recipeFixture,CraftRecipes[0])||recipeFixture.Counts!=std::array<std::uint32_t,4>{0,0,0,2})
                        throw std::runtime_error("Recipe transaction failed.");
                    std::cout<<"Native camera-aligned HUD capture and recipe transaction passed\n";
                }
                if(batch&&(peakUploads==0||peakUploads>uploadBudget))throw std::runtime_error("Upload budget exceeded.");
                if(batch)std::cout<<"Craft peak streaming upload calls per update "<<peakUploads<<"; budget "<<uploadBudget<<"\n";
                if(batch)std::cout<<"Craft worker batch mode: "<<workers<<" workers, "<<batch->StaleCount()<<" discarded jobs\n";
                std::cout<<"Streamed Craft: 80 loads, 55 evictions, 20 retained, 6 captures, billion-meter revisit and edit restore passed\n";
            }
            else
            {
                double x=.5,y=12,z=.5,yaw=0,pitch=-.5;
                auto player=spawn();
                vitals.Respawn(player.Feet[1]);
                if(survival){createHud();hudEnabled=true;}
                bool recipeOneHeld=false,recipeTwoHeld=false;
                bool toolCraftHeld=false,toolToggleHeld=false;
                bool jumpHeld=false,saveHeld=false;
                bool leftHeld=false,rightHeld=false;
                std::string previousTitle;
                auto previous=std::chrono::steady_clock::now();
                auto* window=renderer.NativeWindow().NativeHandle();
                const auto key=[&](int k){return glfwGetKey(window,k)==GLFW_PRESS?1.:0.;};
                while(!renderer.NativeWindow().ShouldClose())
                {
                    renderer.NativeWindow().PollEvents();
                    if(key(GLFW_KEY_ESCAPE))break;
                    const auto [frameWidth,frameHeight]=renderer.NativeWindow().FramebufferExtent();
                    // Minimized windows do not provide a drawable extent. Preserve
                    // the previous target and avoid a zero-sized GPU allocation.
                    if(frameWidth&&frameHeight&&(frameWidth!=hudViewport.Width||frameHeight!=hudViewport.Height))
                    {
                        renderer.ResizeViewport(frameWidth,frameHeight);
                        hudViewport={frameWidth,frameHeight};
                    }
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
                        auto probe=player;probe.Feet[1]-=.000001;
                        const bool support=player.Grounded&&VoxelPlayerBlocked(probe,bodyConfig,
                            [&](VoxelCoordinate c){return query(c).value_or(VoxelAir);});
                        (void)vitals.Observe(player.Feet[1],player.Grounded,support);
                        if(vitals.Dead())
                        {
                            settleWindow(.5,.5);
                            player=spawn();vitals.Respawn(player.Feet[1]);mining.Reset();
                        }
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
                    if(survival)
                    {
                        for(int slot=0;slot<4;++slot)
                            if(key(GLFW_KEY_1+slot))inventory.Selected=static_cast<VoxelMaterial>(slot+1);
                        const bool one=key(GLFW_KEY_R)>0,two=key(GLFW_KEY_T)>0;
                        if(one&&!recipeOneHeld)(void)CraftApplyRecipe(inventory,CraftRecipes[0]);
                        if(two&&!recipeTwoHeld)(void)CraftApplyRecipe(inventory,CraftRecipes[1]);
                        recipeOneHeld=one;recipeTwoHeld=two;
                        const bool craftTool=key(GLFW_KEY_F)>0,toggleTool=key(GLFW_KEY_G)>0;
                        if(craftTool&&!toolCraftHeld&&equipment.Craft(inventory.Counts))mining.Reset();
                        if(toggleTool&&!toolToggleHeld&&equipment.Toggle())mining.Reset();
                        toolCraftHeld=craftTool;toolToggleHeld=toggleTool;
                        constexpr std::array<const char*,4> labels{"Grass","Dirt","Stone","Building"};
                        std::string title="Kairo Craft | HP "+std::to_string(vitals.Health())+"/20 | ";
                        for(unsigned i=0;i<4;++i)
                        {
                            if(i+1u==inventory.Selected)title+="[";
                            title+=std::to_string(i+1u)+":"+labels[i]+" "+std::to_string(inventory.Counts[i]);
                            if(i+1u==inventory.Selected)title+="]";
                            title+="  ";
                        }
                        title+="| Pick "+std::to_string(equipment.Durability)+"/64"+(equipment.Equipped?" active":" stowed")+
                            " | F craft pick (2 dirt + 3 stone), G equip | R/T craft | F5 save";
                        if(title!=previousTitle){glfwSetWindowTitle(window,title.c_str());previousTitle=std::move(title);}
                    }
                    const bool left=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
                    const bool right=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS;
                    if(survival)
                    {
                        const auto hit=(left||(right&&!rightHeld))?TraceVoxels({x,y,z},direction,8,64,query):std::optional<VoxelHit>{};
                        std::optional<CraftMiningTarget> target;
                        if(hit&&hit->Material>=CraftGrass&&hit->Material<=CraftPlaced)
                            target=CraftMiningTarget{{hit->Cell.X,hit->Cell.Y,hit->Cell.Z},hit->Material};
                        (void)mining.Advance(left,target,dt,[&](const CraftMiningTarget& completed)
                        {
                            const VoxelCoordinate cell{completed.Cell[0],completed.Cell[1],completed.Cell[2]};
                            if(query(cell)!=completed.Material||!MineCraftBlock(inventory,cell,query,edit))return false;
                            equipment.SuccessfulMine(completed.Material);
                            return true;
                        },target?equipment.Speed(target->Material):1.);
                        if(!left&&right&&!rightHeld&&hit&&hit->Placement)
                            (void)PlaceCraftBlock(inventory,*hit->Placement,query,edit,
                                [&](auto cell){return !PlayerIntersectsBlock(player,bodyConfig,cell);});
                    }
                    else if((left&&!leftHeld)||(right&&!rightHeld))
                    {
                        if(const auto hit=TraceVoxels({x,y,z},direction,8,64,query))
                        {
                            if(left&&!leftHeld)(void)edit(hit->Cell,VoxelAir);
                            else if(hit->Placement)(void)edit(*hit->Placement,CraftPlaced);
                        }
                    }
                    leftHeld=left;rightHeld=right;
                    const bool saveDown=key(GLFW_KEY_F5)>0;
                    if(saveDown&&!saveHeld){try{save();}catch(const std::exception& ex){std::cerr<<"Save failed: "<<ex.what()<<'\n';}}
                    saveHeld=saveDown;
                    if(nativeHud)refreshNativeHud();
                    else if(hudEnabled)committedSubmit(publication.Active());
                    renderer.DrawFrame();
                }
            }
        }
        catch(...){hudEnabled=false;(void)publication.Release(committedSubmit,destroy);throw;}
        hudEnabled=false;
        refreshNativeHud();
        if(publication.Release(committedSubmit,destroy)!=0)throw std::runtime_error("Streamed Craft shutdown retained meshes.");
        if(hudMesh)renderer.DestroyMesh(*hudMesh);
        return 0;
    }
    catch(const std::exception& e){std::cerr<<"Streamed Craft failed: "<<e.what()<<'\n';return 1;}
}
