#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "CraftStreamingWindow.hpp"
#include "CraftStreamSave.hpp"
#include "CraftInventory.hpp"
#include "CraftRecipesHud.hpp"
#include "CraftHudProjection.hpp"
#include "CraftHudPublication.hpp"
#include "CraftVitals.hpp"
#include "CraftNutrition.hpp"
#include "CraftDaylight.hpp"
#include "CraftSurfaceSpawn.hpp"
#include "CraftMining.hpp"
#include "CraftEquipment.hpp"
#include "CraftMouseLook.hpp"
#include "CraftActionLatch.hpp"
#include "CraftTargetFeedback.hpp"
#include "CraftEquipmentSave.hpp"
#include "CraftSaveFile.hpp"
#include "CraftAutosave.hpp"
#include "CraftCheckpoint.hpp"
#include "CraftCreatureGroup.hpp"
#include "CraftCombatReward.hpp"
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
        CraftAutosave autosave;
        std::optional<CraftCheckpoint> playerCheckpoint;
        std::uint64_t worldRevision=0;
        bool finalSaveFailed=false;
        if(smoke&&savePath)throw std::invalid_argument("Smoke mode does not touch user saves.");
        const bool existingSave=savePath&&std::filesystem::exists(*savePath);
        if(existingSave)
        {
            const auto bytes=ReadCraftSaveFile(*savePath,CraftSessionMaximumBytes+CraftEquipmentOverhead+CraftCheckpointOverhead);
            std::span<const std::uint8_t> payload=bytes;
            if(HasCraftCheckpoint(payload))
            {
                const auto decoded=DecodeCraftCheckpoint(payload,CraftSessionMaximumBytes+CraftEquipmentOverhead);
                playerCheckpoint=decoded.State;payload=decoded.Payload;
            }
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
            auto bytes=EncodeCraftEquipmentEnvelope(payload,equipment,CraftSessionMaximumBytes);
            if(playerCheckpoint)bytes=EncodeCraftCheckpoint(bytes,*playerCheckpoint,CraftSessionMaximumBytes+CraftEquipmentOverhead);
            WriteCraftSaveFile(*savePath,bytes,CraftSessionMaximumBytes+CraftEquipmentOverhead+CraftCheckpointOverhead);
            std::cout<<"Streamed Craft saved "<<world->Overrides().size()<<" edits\n";
        };
        const auto saveStamp=[&]()
        {
            return CraftSaveStamp{worldRevision,inventory.Counts,inventory.Selected,equipment.Durability,equipment.Equipped,playerCheckpoint};
        };
        autosave.Initialize(saveStamp(),existingSave||!savePath);
        const auto checkpoint=[&]()
        {
            if(!savePath)return true;
            try{save();autosave.Saved();return true;}
            catch(const std::exception& ex)
            {
                autosave.Failed();std::cerr<<"Craft save failed: "<<ex.what()<<'\n';
                return false;
            }
        };
        Plan::Slots coordinates{};
        Publication publication;
        VoxelCoordinate origin{};
        CraftTargetPublication targetPublication;
        const auto targetCell=[](VoxelCoordinate c){return CraftTargetCell{c.X,c.Y,c.Z};};
        const auto publishTarget=[&](std::optional<CraftTargetKey> key)
        {
            (void)targetPublication.Refresh(key,[&](const auto& selected)
            {
                DebugDrawList lines;
                if(selected)
                {
                    const auto boxes=CraftBuildTargetBoxes(*selected);
                    const auto add=[&](const CraftTargetBox& b)
                    {
                        lines.AddAABB({b.Min[0],b.Min[1],b.Min[2]},
                            {b.Max[0],b.Max[1],b.Max[2]},{b.Color[0],b.Color[1],b.Color[2],1.f});
                    };
                    add(boxes.Target);
                    if(boxes.Placement)add(*boxes.Placement);
                }
                renderer.SubmitDebugDraw(lines);
            });
        };
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
        // Time-of-day modifies only authored lighting; the current native
        // mesh handles and chunk generation remain untouched.
        const auto applyDaylight=[&](unsigned step)
        {
            const auto light=CraftDaylight::Sample(step);
            RenderScene next;
            auto directional=sun;
            directional.Direction={light.SunDirection[0],light.SunDirection[1],light.SunDirection[2]};
            directional.Color={light.SunColor[0],light.SunColor[1],light.SunColor[2]};
            directional.Intensity=light.SunIntensity;
            next.AddLight(directional);
            RenderEnvironment environment;
            environment.BackgroundColor={light.Sky[0],light.Sky[1],light.Sky[2]};
            environment.AmbientColor={light.Ambient[0],light.Ambient[1],light.Ambient[2]};
            environment.AmbientIntensity=light.AmbientIntensity;
            next.SetEnvironment(environment);
            authored=std::move(next);
        };
        std::optional<MeshHandle> hudMesh;
        std::optional<CameraPose> hudPose;
        bool hudEnabled=false;
        CraftHudPublication hudPublication;
        CraftVitals vitals;
        CraftNutrition nutrition;
        CraftMining mining;
        CraftCreatureGroup creatures;
        CraftPickups pickups;
        unsigned aimedHealth=0;
        std::optional<MeshHandle> creatureMesh;
        const auto refreshNativeHud=[&]()
        {
            if(!nativeHud)return;
            const CraftHudKey key{hudEnabled,inventory.Counts,inventory.Selected,hudViewport.Width,hudViewport.Height,vitals.Health(),mining.Progress(),equipment.Durability,equipment.Equipped,nutrition.Hunger(),aimedHealth};
            (void)hudPublication.Refresh(key,[&]()
            {
                std::vector<ViewportOverlayRect> rects;
                if(hudEnabled)
                {
                    const auto layout=BuildCraftHud(inventory,vitals.Health(),mining.Progress(),equipment.Durability,equipment.Equipped,nutrition.Hunger(),aimedHealth);
                    rects.reserve(layout.size());
                    // Paint opaque backing first, then icons and numerals.
                    // No camera, world origin or depth state is involved.
                    for(bool backing:{true,false})for(const auto& rect:layout)
                    {
                        if(rect.Background!=backing)continue;
                        const auto p=QuantizeCraftHudRect(hudViewport,rect.X,rect.Y,rect.W,rect.H,rect.Y>=410);
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
            if(creatureMesh)for(const auto& creature:creatures.Members())if(creature.Body())
            {
                const auto& feet=creature.Body()->Feet;
                const std::array<double,3> anchor{feet[0]-base.X,feet[1]-base.Y,feet[2]-base.Z};
                for(const auto& part:BuildCraftCreatureRig(creature.Visual()))
                {
                    MeshDraw draw;draw.Mesh=*creatureMesh;draw.Model=Mat4f::Identity();
                    for(unsigned row=0;row<3;++row)
                    {
                        for(unsigned col=0;col<3;++col)
                            draw.Model(row,col)=static_cast<float>(part.Axes[col][row]);
                        draw.Model(row,3)=TerrainRelativeFloat(anchor[row]+part.Origin[row],0);
                    }
                    draw.Material.BaseColor={part.Color[0],part.Color[1],part.Color[2]};
                    draw.Material.Emissive={.06f,.02f,.01f};
                    draw.CastShadows=false;scene.Add(draw);
                }
            }
            // Pickups reuse the unit creature cube; no mesh upload per item.
            if(creatureMesh)for(const auto& slot:pickups.Items())if(slot)
            {
                const auto& pos=slot->Center;
                if(std::abs(pos[0]-base.X)>64||std::abs(pos[1]-base.Y)>128||std::abs(pos[2]-base.Z)>64)continue;
                MeshDraw draw;draw.Mesh=*creatureMesh;draw.Model=Mat4f::Identity();
                for(unsigned axis=0;axis<3;++axis)draw.Model(axis,axis)=.32f;
                draw.Model(0,3)=TerrainRelativeFloat(pos[0]-.16,base.X);
                draw.Model(1,3)=TerrainRelativeFloat(pos[1]-.16,base.Y);
                draw.Model(2,3)=TerrainRelativeFloat(pos[2]-.16,base.Z);
                draw.Material.BaseColor={.64f,.82f,1.f};draw.Material.Emissive={.12f,.20f,.32f};
                draw.CastShadows=false;scene.Add(draw);
            }
            if(!nativeHud&&hudEnabled&&hudMesh&&hudPose)
            {
                const auto& pose=*hudPose;
                const ScreenVec eye{pose.Position.x,pose.Position.y,pose.Position.z};
                const auto forward=ScreenUnit(ScreenVec{double(pose.Target.x)-eye[0],
                    double(pose.Target.y)-eye[1],double(pose.Target.z)-eye[2]});
                const auto right=ScreenUnit(ScreenCross(forward,{pose.Up.x,pose.Up.y,pose.Up.z}));
                const auto up=ScreenUnit(ScreenCross(right,forward));
                for(const auto& rect:BuildCraftHud(inventory,vitals.Health(),mining.Progress(),equipment.Durability,equipment.Equipped,nutrition.Hunger(),aimedHealth))
                {
                    const double depth=rect.Background?.16:.14;
                    const auto projected=ProjectCraftHudRect(hudViewport,rect.X,rect.Y,rect.W,rect.H,
                        pose.VerticalFovRadians,depth,rect.Y>=410);
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
        const auto createCreature=[&]()
        {
            if(creatureMesh)return;
            auto cube=BuildCraftMesh(BuildVoxelChunkSurface({0,0,0},1,[](VoxelCoordinate cell){
                return cell==VoxelCoordinate{}?CraftStone:VoxelAir;}));
            for(auto& vertex:cube.Vertices)vertex.Color={1,1,1};
            creatureMesh=uploadMesh(std::move(cube));
            if(!creatureMesh)throw std::runtime_error("Creature mesh missing.");
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
            const auto candidate=FindCraftSurfaceSpawn(bodyConfig,CraftExtent,
                [&](VoxelCoordinate c)->std::optional<VoxelMaterial>{return world->Get(c);});
            if(!candidate)throw std::runtime_error("No supported clear surface spawn near origin.");
            settleWindow(candidate->Feet[0],candidate->Feet[2]);
            auto p=*candidate;
            AdvanceVoxelPlayer(p,bodyConfig,{},0,bodyQuery);
            if(!p.Grounded)throw std::runtime_error("Surface spawn lost support after publication.");
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
            world.swap(next);++edits;++worldRevision;

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
                    inventory.Counts={0,2,3,0};
                    if(!equipment.Craft(inventory.Counts))throw std::runtime_error("Native pick craft failed.");
                    const auto speed=equipment.Speed(target.Material);
                    const auto holds=static_cast<unsigned>(std::ceil(CraftMiningSeconds(target.Material)/speed/.1-1e-9));
                    for(unsigned step=0;step<holds;++step)
                    {
                        const bool mined=mining.Advance(true,target,.1,[&](const auto& completed){
                            if(completed!=target||!MineCraftBlock(inventory,cell,query,edit))return false;
                            equipment.SuccessfulMine(completed.Material);
                            return true;
                        },speed);
                        const bool last=step+1==holds;
                        if(mined!=last||inventory.Count(target.Material)!=(last?1u:0u)||
                            query(cell)!=(last?VoxelAir:target.Material))
                            throw std::runtime_error("Native timed mining transaction differed.");
                    }
                    inventory.Selected=selected->Material;
                    const auto inner=EncodeCraftSession(seed,*world,inventory);
                    const auto envelope=EncodeCraftEquipmentEnvelope(inner,equipment,CraftSessionMaximumBytes);
                    const auto equippedSave=DecodeCraftEquipmentEnvelope(envelope,CraftSessionMaximumBytes);
                    const auto session=DecodeCraftSession(equippedSave.Payload);
                    if(equippedSave.Equipment.Durability!=(speed>1?63u:64u)||!equippedSave.Equipment.Equipped)
                        throw std::runtime_error("Native pick durability/save differed.");
                    if(session.Inventory.Count(inventory.Selected)!=1||
                        session.World.Overrides()!=world->Overrides())
                        throw std::runtime_error("Survival inventory persistence failed.");
                    const auto placementHit=TraceVoxels({.5,12,.5},{0,-1,0},12,32,query);
                    if(!placementHit||placementHit->Placement!=selected->Cell||
                        !PlaceCraftBlock(inventory,*placementHit->Placement,query,edit,[](auto){return true;})||
                        inventory.Count(selected->Material)!=0||!world->Overrides().empty())
                        throw std::runtime_error("Survival placement did not consume inventory.");
                    if(capture(.5,.5).RGBA!=baseline.RGBA)
                        throw std::runtime_error("Survival inventory edit did not restore image.");
                    std::cout<<"Survival mining, inventory save roundtrip and material-consuming placement passed\n";
                    createHud();hudEnabled=true;
                    const auto withHud=capture(.5,.5);
                    if(withHud.RGBA==baseline.RGBA)throw std::runtime_error("HUD did not appear in native capture.");
                    aimedHealth=12;
                    if(capture(.5,.5).RGBA==withHud.RGBA)
                        throw std::runtime_error("Combat HUD capture unchanged.");
                    aimedHealth=0;
                    if(capture(.5,.5).RGBA!=withHud.RGBA)
                        throw std::runtime_error("Combat HUD clear did not restore normal HUD.");
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
                // Independent capture tests the retained renderer debug list.
                publishTarget(CraftTargetKey{targetCell(hit->Cell),targetCell(origin),true,false,std::nullopt});
                if(capture(.5,.5).RGBA==baseline.RGBA)
                    throw std::runtime_error("Target outline capture unchanged.");
                publishTarget(std::nullopt);
                if(capture(.5,.5).RGBA!=baseline.RGBA)
                    throw std::runtime_error("Target outline removal changed baseline.");
                std::cout<<"Native target outline and removal passed\n";
                // Prove native day/night changes without any chunk loads
                // or edits. Restore the exact prior authored scene.
                const auto beforeLighting=authored;
                const auto previousLoads=loads,previousEdits=edits;
                applyDaylight(60);committedSubmit(publication.Active());
                const auto day=capture(.5,.5);
                applyDaylight(180);committedSubmit(publication.Active());
                const auto night=capture(.5,.5);
                if(day.RGBA==night.RGBA||loads!=previousLoads||edits!=previousEdits)
                    throw std::runtime_error("Daylight capture or mesh residency differs.");
                authored=beforeLighting;committedSubmit(publication.Active());
                if(capture(.5,.5).RGBA!=baseline.RGBA)
                    throw std::runtime_error("Restored daylight capture differs.");
                std::cout<<"Native day/night lighting and unchanged mesh residency passed\n";
                createCreature();
                const auto survivor=spawn();
                for(unsigned tick=0;tick<60;++tick)(void)creatures.Advance(.05,survivor,query);
                if(creatures.Count()!=1)throw std::runtime_error("Native staggered creature spawn missing.");
                const auto mob=creatures.At(0).Body()->Feet;
                committedSubmit(publication.Active());
                const auto occupied=capture(mob[0],mob[2]);
                creatures.Reset();committedSubmit(publication.Active());
                if(capture(mob[0],mob[2]).RGBA==occupied.RGBA)
                    throw std::runtime_error("Creature mesh was not visible.");
                const auto emptyLoot=capture(mob[0],mob[2]);
                auto lootCenter=mob;lootCenter[1]+=.4;
                if(!pickups.TrySpawn(lootCenter,CraftStone,2))throw std::runtime_error("Native pickup spawn failed.");
                committedSubmit(publication.Active());
                if(capture(mob[0],mob[2]).RGBA==emptyLoot.RGBA)
                    throw std::runtime_error("Native pickup was not visible.");
                pickups.Clear();committedSubmit(publication.Active());
                if(capture(mob[0],mob[2]).RGBA!=emptyLoot.RGBA)
                    throw std::runtime_error("Native pickup removal changed baseline.");
                std::cout<<"Native creature and pickup mesh visibility and removal passed\n";
                if(batch&&(peakUploads==0||peakUploads>uploadBudget))throw std::runtime_error("Upload budget exceeded.");
                if(batch)std::cout<<"Craft peak streaming upload calls per update "<<peakUploads<<"; budget "<<uploadBudget<<"\n";
                if(batch)std::cout<<"Craft worker batch mode: "<<workers<<" workers, "<<batch->StaleCount()<<" discarded jobs\n";
                std::cout<<"Streamed Craft: 80 loads, 55 evictions, 20 retained, 6 captures, billion-meter revisit and edit restore passed\n";
            }
            else
            {
                double x=.5,y=12,z=.5,yaw=0,pitch=-.5;
                VoxelPlayer player;
                if(survival)
                {
                    std::optional<VoxelPlayer> restored;
                    if(playerCheckpoint&&std::abs(playerCheckpoint->Feet[0])<999999950&&
                        std::abs(playerCheckpoint->Feet[2])<999999950&&playerCheckpoint->Feet[1]>=1&&
                        playerCheckpoint->Feet[1]<=CraftExtent)
                    {
                        restored=RestoreCraftCheckpoint(*playerCheckpoint,bodyConfig,
                            [&](VoxelCoordinate c)->std::optional<VoxelMaterial>{return world->Get(c);});
                        if(restored)
                        {
                            settleWindow(restored->Feet[0],restored->Feet[2]);
                            if(VoxelPlayerBlocked(*restored,bodyConfig,bodyQuery))restored.reset();
                        }
                    }
                    player=restored?*restored:spawn();
                    vitals.Respawn(player.Feet[1]);
                    if(playerCheckpoint)
                    {
                        vitals.Restore(playerCheckpoint->Health,player.Feet[1]);nutrition.Restore(playerCheckpoint->Hunger);
                        yaw=playerCheckpoint->Yaw;pitch=playerCheckpoint->Pitch;
                    }
                    x=player.Feet[0];y=player.Feet[1]+bodyConfig.EyeHeight;z=player.Feet[2];
                    createHud();hudEnabled=true;createCreature();
                }
                CraftDaylight daylight(playerCheckpoint?playerCheckpoint->DayPhase:.25);
                unsigned publishedDaylight=daylight.Step();
                applyDaylight(publishedDaylight);committedSubmit(publication.Active());
                const auto recordPlayer=[&](){if(survival)playerCheckpoint=CraftCheckpoint{player.Feet,yaw,pitch,daylight.Phase(),vitals.Health(),nutrition.Hunger()};};
                recordPlayer();
                CraftActionInputs actions;
                bool leftHeld=false,rightHeld=false;
                std::string previousTitle;
                auto previous=std::chrono::steady_clock::now();
                auto* window=renderer.NativeWindow().NativeHandle();
                CraftMouseLook mouseLook;
                bool focused=glfwGetWindowAttrib(window,GLFW_FOCUSED)==GLFW_TRUE;
                bool mouseCaptured=true,tabHeld=glfwGetKey(window,GLFW_KEY_TAB)==GLFW_PRESS,mouseArmed=false;
                const auto captureMouse=[&]()
                {
                    if(glfwRawMouseMotionSupported())glfwSetInputMode(window,GLFW_RAW_MOUSE_MOTION,GLFW_FALSE);
                    glfwSetInputMode(window,GLFW_CURSOR,mouseCaptured?GLFW_CURSOR_DISABLED:GLFW_CURSOR_NORMAL);
                    if(mouseCaptured&&glfwRawMouseMotionSupported())
                        glfwSetInputMode(window,GLFW_RAW_MOUSE_MOTION,GLFW_TRUE);
                    mouseLook.Reset();mouseArmed=false;mining.Reset();
                };
                captureMouse();
                const auto key=[&](int k){return focused&&glfwGetKey(window,k)==GLFW_PRESS?1.:0.;};
                while(!renderer.NativeWindow().ShouldClose())
                {
                    renderer.NativeWindow().PollEvents();
                    if(renderer.NativeWindow().ShouldClose())break;
                    const auto [frameWidth,frameHeight]=renderer.NativeWindow().FramebufferExtent();
                    const bool nowFocused=CraftFrameActive(
                        glfwGetWindowAttrib(window,GLFW_FOCUSED)==GLFW_TRUE,
                        glfwGetWindowAttrib(window,GLFW_ICONIFIED)==GLFW_TRUE,frameWidth,frameHeight);
                    if(nowFocused!=focused)
                    {
                        focused=nowFocused;mouseLook.Reset();mouseArmed=false;mining.Reset();
                        actions.Reset();leftHeld=false;rightHeld=false;
                        tabHeld=glfwGetKey(window,GLFW_KEY_TAB)==GLFW_PRESS;
                    }
                    // No simulation or rendering while unfocused, iconified,
                    // or without a drawable framebuffer. Keep polling close events.
                    if(!focused)
                    {
                        previous=std::chrono::steady_clock::now();
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    }
                    if(key(GLFW_KEY_ESCAPE))break;
                    const bool tab=key(GLFW_KEY_TAB)>0;
                    if(tab&&!tabHeld){mouseCaptured=!mouseCaptured;captureMouse();}
                    tabHeld=tab;
                    // Only active, nonzero extents reach the resize and draw path.
                    if(frameWidth!=hudViewport.Width||frameHeight!=hudViewport.Height)
                    {
                        renderer.ResizeViewport(frameWidth,frameHeight);
                        hudViewport={frameWidth,frameHeight};
                    }
                    const auto now=std::chrono::steady_clock::now();
                    const double dt=std::min(.1,std::chrono::duration<double>(now-previous).count());
                    previous=now;
                    daylight.Advance(dt);
                    if(daylight.Step()!=publishedDaylight)
                    {
                        applyDaylight(daylight.Step());
                        committedSubmit(publication.Active());
                        publishedDaylight=daylight.Step();
                    }
                    double cursorX=0,cursorY=0;
                    glfwGetCursorPos(window,&cursorX,&cursorY);
                    const auto look=mouseLook.Observe(focused&&mouseCaptured,cursorX,cursorY);
                    ApplyCraftLook(yaw,pitch,look,key(GLFW_KEY_RIGHT)-key(GLFW_KEY_LEFT),
                        key(GLFW_KEY_UP)-key(GLFW_KEY_DOWN),dt);
                    const double f=key(GLFW_KEY_W)-key(GLFW_KEY_S),r=key(GLFW_KEY_D)-key(GLFW_KEY_A);
                    const double scale=12*dt/std::max(1.,std::hypot(f,r));
                    const bool jump=actions.Jump.Press(key(GLFW_KEY_SPACE)>0);
                    const bool sprint=nutrition.CanSprint()&&key(GLFW_KEY_LEFT_SHIFT)>0&&(f!=0||r!=0);
                    const auto previousCreatures=creatures.State();
                    const auto previousPickups=pickups.Revision();
                    if(survival)pickups.Advance(dt);
                    if(survival)
                    {
                        const double norm=std::max(1.,std::hypot(f,r));
                        AdvanceVoxelPlayer(player,bodyConfig,
                            {(std::sin(yaw)*f+std::cos(yaw)*r)/norm,
                            (-std::cos(yaw)*f+std::sin(yaw)*r)/norm,
                            jump,sprint},dt,bodyQuery);
                        auto probe=player;probe.Feet[1]-=.000001;
                        const bool support=player.Grounded&&VoxelPlayerBlocked(probe,bodyConfig,
                            [&](VoxelCoordinate c){return query(c).value_or(VoxelAir);});
                        (void)vitals.Observe(player.Feet[1],player.Grounded,support);
                        if(focused)nutrition.Advance(dt,sprint,vitals);
                        if(!vitals.Dead())vitals.Damage(creatures.Advance(dt,player,query));
                        if(vitals.Dead())
                        {
                            player=spawn();vitals.Respawn(player.Feet[1]);mining.Reset();nutrition.Reset();creatures.Reset();
                        }
                        x=player.Feet[0];y=player.Feet[1]+bodyConfig.EyeHeight;z=player.Feet[2];
                    }
                    else
                    {
                        x=std::clamp(x+(std::sin(yaw)*f+std::cos(yaw)*r)*scale,-1e9,1e9);
                        z=std::clamp(z+(-std::cos(yaw)*f+std::sin(yaw)*r)*scale,-1e9,1e9);
                        y=std::clamp(y+(key(GLFW_KEY_SPACE)-key(GLFW_KEY_LEFT_CONTROL))*12*dt,1.,32.);
                    }
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
                        if(actions.RecipeOne.Press(key(GLFW_KEY_R)>0))(void)CraftApplyRecipe(inventory,CraftRecipes[0]);
                        if(actions.RecipeTwo.Press(key(GLFW_KEY_T)>0))(void)CraftApplyRecipe(inventory,CraftRecipes[1]);
                        if(actions.CraftTool.Press(key(GLFW_KEY_F)>0)&&equipment.Craft(inventory.Counts))mining.Reset();
                        if(actions.ToggleTool.Press(key(GLFW_KEY_G)>0)&&equipment.Toggle())mining.Reset();
                        if(actions.Eat.Press(key(GLFW_KEY_E)>0))(void)nutrition.Eat(inventory.Counts);
                        constexpr std::array<const char*,4> labels{"Grass","Dirt","Stone","Building"};
                        std::string title="Kairo Craft | HP "+std::to_string(vitals.Health())+"/20 | Food "+std::to_string(nutrition.Hunger())+"/20 | ";
                        for(unsigned i=0;i<4;++i)
                        {
                            if(i+1u==inventory.Selected)title+="[";
                            title+=std::to_string(i+1u)+":"+labels[i]+" "+std::to_string(inventory.Counts[i]);
                            if(i+1u==inventory.Selected)title+="]";
                            title+="  ";
                        }
                        title+="| Pick "+std::to_string(equipment.Durability)+"/64"+(equipment.Equipped?" active":" stowed")+
                            " | F craft pick, G equip | E forage (2 grass) | R/T craft | F5 save | Tab mouse";
                        title+=" | Sentinels "+std::to_string(creatures.Count());
                        if(title!=previousTitle){glfwSetWindowTitle(window,title.c_str());previousTitle=std::move(title);}
                    }
                    const bool rawLeft=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
                    const bool rawRight=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS;
                    // Do not turn a button held during focus/capture changes
                    // into a new mining or placement action. Require release.
                    if(focused&&mouseCaptured&&!rawLeft&&!rawRight)mouseArmed=true;
                    const bool left=focused&&mouseCaptured&&mouseArmed&&rawLeft;
                    const bool right=focused&&mouseCaptured&&mouseArmed&&rawRight;
                    if(survival)
                    {
                        const auto combatAim=creatures.Aim({x,y,z},direction,query);
                        if(left&&combatAim&&!leftHeld)
                        {
                            const auto combat=CraftStrikeDrop(creatures.At(*combatAim),inventory,pickups);
                            if(combat.Defeated)
                                std::cout<<"Stone sentinel defeated; loot dropped (overflow award "<<combat.StoneAwarded<<")\n";
                        }
                        const auto hit=((left&&!combatAim)||(right&&!rightHeld))?TraceVoxels({x,y,z},direction,8,64,query):std::optional<VoxelHit>{};
                        std::optional<CraftMiningTarget> target;
                        if(hit&&hit->Material>=CraftGrass&&hit->Material<=CraftPlaced)
                            target=CraftMiningTarget{{hit->Cell.X,hit->Cell.Y,hit->Cell.Z},hit->Material};
                        (void)mining.Advance(left&&!combatAim,target,dt,[&](const CraftMiningTarget& completed)
                        {
                            const VoxelCoordinate cell{completed.Cell[0],completed.Cell[1],completed.Cell[2]};
                            if(query(cell)!=completed.Material||!MineCraftBlock(inventory,cell,query,edit))return false;
                            equipment.SuccessfulMine(completed.Material);
                            return true;
                        },target?equipment.Speed(target->Material):1.);
                        if(!left&&right&&!rightHeld&&hit&&hit->Placement)
                            (void)PlaceCraftBlock(inventory,*hit->Placement,query,edit,
                                [&](auto cell){return !PlayerIntersectsBlock(player,bodyConfig,cell)&&!creatures.Occupies(cell);});
                    }
                    else if((left&&!leftHeld)||(right&&!rightHeld))
                    {
                        if(const auto hit=TraceVoxels({x,y,z},direction,8,64,query))
                        {
                            if(left&&!leftHeld)(void)edit(hit->Cell,VoxelAir);
                            else if(hit->Placement)(void)edit(*hit->Placement,CraftPlaced);
                        }
                    }
                    if(survival)CollectCraftPickups(pickups,inventory,{x,y,z},query);
                    leftHeld=left;rightHeld=right;

                    // Re-query after edits, so a removed target disappears
                    // immediately. Placement is preflight feedback, not a
                    // promise that a subsequent native transaction will succeed.
                    std::optional<CraftTargetKey> aim;
                    aimedHealth=survival&&mouseCaptured?creatures.AimedHealth({x,y,z},direction,query):0;
                    if(mouseCaptured&&!aimedHealth)
                        if(const auto pointed=TraceVoxels({x,y,z},direction,8,64,query))
                        {
                            const bool mineable=InStreamCraftWorld(pointed->Cell)&&(!survival||
                                (pointed->Material>=CraftGrass&&pointed->Material<=CraftPlaced&&
                                 inventory.Count(pointed->Material)<CraftInventory::StackLimit));
                            aim=CraftTargetKey{targetCell(pointed->Cell),targetCell(origin),mineable,false,std::nullopt};
                            if(pointed->Placement)
                            {
                                const auto cell=*pointed->Placement;
                                aim->Placement=targetCell(cell);
                                aim->Placeable=InStreamCraftWorld(cell)&&query(cell)==VoxelAir&&(!survival||
                                    (inventory.Count(inventory.Selected)>0&&!PlayerIntersectsBlock(player,bodyConfig,cell)&&!creatures.Occupies(cell)));
                            }
                        }
                    publishTarget(aim);
                    const bool worldActorsChanged=previousCreatures!=creatures.State()||previousPickups!=pickups.Revision();
                    if(worldActorsChanged)committedSubmit(publication.Active());
                    recordPlayer();autosave.Observe(saveStamp());
                    const bool manualSave=actions.Save.Press(key(GLFW_KEY_F5)>0);
                    if(savePath&&(manualSave||autosave.Advance(dt)))(void)checkpoint();
                    if(nativeHud)refreshNativeHud();
                    else if(hudEnabled&&!worldActorsChanged)committedSubmit(publication.Active());
                    renderer.DrawFrame();
                }
                // Exit is a final checkpoint only for an explicitly supplied
                // save path and changed state. Do not silently lose a failed save.
                recordPlayer();autosave.Observe(saveStamp());
                if(savePath&&autosave.Dirty())finalSaveFailed=!checkpoint();
            }
        }
        catch(...){hudEnabled=false;(void)publication.Release(committedSubmit,destroy);throw;}
        hudEnabled=false;
        creatures.Reset();pickups.Clear();
        publishTarget(std::nullopt);
        refreshNativeHud();
        if(publication.Release(committedSubmit,destroy)!=0)throw std::runtime_error("Streamed Craft shutdown retained meshes.");
        if(hudMesh)renderer.DestroyMesh(*hudMesh);
        if(creatureMesh)renderer.DestroyMesh(*creatureMesh);
        return finalSaveFailed?1:0;
    }
    catch(const std::exception& e){std::cerr<<"Streamed Craft failed: "<<e.what()<<'\n';return 1;}
}
