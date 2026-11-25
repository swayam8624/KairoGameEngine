#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "CraftCaveSession.hpp"
#include "CraftRecipesHud.hpp"
#include "CraftHudProjection.hpp"
#include "CraftHudPublication.hpp"
#include "CraftVitals.hpp"
#include "CraftNutrition.hpp"
#include "CraftMining.hpp"
#include "CraftEquipment.hpp"
#include "CraftMouseLook.hpp"
#include "CraftActionLatch.hpp"
#include "CraftTargetFeedback.hpp"
#include "CraftEquipmentSave.hpp"
#include "CraftSaveFile.hpp"
#include "CraftAutosave.hpp"
#include "CraftCheckpoint.hpp"
#include "CraftCreature.hpp"
#include "CraftCombatReward.hpp"
#include "TerrainScreenError.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include "CraftMeshBatch.hpp"
#include "VoxelMeshPublication.hpp"
#include "VoxelPlayer.hpp"
#include "CraftCaveSpawn.hpp"
#include "TerrainRenderOrigin.hpp"
#include <chrono>
#include <thread>
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
        std::optional<std::filesystem::path> savePath;
        std::uint64_t seed=42;
        auto backend=GraphicsBackend::Automatic;
        for(int i=1;i<argc;++i)
        {
            const std::string_view arg=argv[i];
            if(arg=="--smoke")smoke=true;
            else if(arg=="--survival")survival=true;
            else if(arg=="--save"&&i+1<argc)savePath=std::filesystem::path(argv[++i]);
            else if(arg=="--renderer"&&i+1<argc)backend=ParseGraphicsBackend(argv[++i]);
            else if(arg=="--seed"&&i+1<argc)
            {
                const std::string_view s=argv[++i];
                const auto result=std::from_chars(s.data(),s.data()+s.size(),seed);
                if(result.ec!=std::errc{}||result.ptr!=s.data()+s.size())throw std::invalid_argument("Invalid seed.");
            }
            else throw std::invalid_argument("KairoCraftCaves [--seed N] [--renderer backend] [--survival] [--save path] [--smoke]");
        }
        if(smoke&&savePath)throw std::invalid_argument("Smoke mode does not touch user saves.");
        RendererRuntime renderer({"Kairo Craft Caves",960u,540u,!smoke,backend});
        CraftHudViewport hudViewport;
        const bool nativeHud=renderer.SupportsViewportOverlay();
        renderer.ResizeViewport(hudViewport.Width,hudViewport.Height);
        renderer.SetViewportShadingMode(ViewportShadingMode::Lit);
        using P=CaveWindowPlan<1>;
        using W=CaveCraftWorldEdits;
        using Pub=VoxelMeshPublication<MeshHandle,P::Capacity>;
        auto world=std::make_shared<W>(CaveCraftBaseline{seed},CraftSaveMaximumEdits);
        CraftInventory inventory;
        CraftEquipment equipment;
        CraftAutosave autosave;
        std::optional<CraftCheckpoint> playerCheckpoint;
        std::uint64_t worldRevision=0;
        bool finalSaveFailed=false;
        const bool existingSave=savePath&&std::filesystem::exists(*savePath);
        if(existingSave)
        {
            const auto bytes=ReadCraftSaveFile(*savePath,CaveCraftMaximumBytes+CraftEquipmentOverhead+CraftCheckpointOverhead);
            std::span<const std::uint8_t> payload=bytes;
            if(HasCraftCheckpoint(payload))
            {
                const auto decoded=DecodeCraftCheckpoint(payload,CaveCraftMaximumBytes+CraftEquipmentOverhead);
                playerCheckpoint=decoded.State;payload=decoded.Payload;
            }
            if(HasCraftEquipmentEnvelope(payload))
            {
                const auto decoded=DecodeCraftEquipmentEnvelope(payload,CaveCraftMaximumBytes);
                equipment=decoded.Equipment;payload=decoded.Payload;
            }
            auto decoded=DecodeCaveCraftSession(payload);
            seed=decoded.Seed;world=std::make_shared<W>(std::move(decoded.World));inventory=decoded.Inventory;
        }
        // Save only when a path is supplied, using the same single-writer replacement policy
        // as the streamed surface world. A failed write leaves the old save.
        const auto save=[&]()
        {
            if(!savePath)return;
            const auto payload=EncodeCaveCraftSession(seed,*world,inventory);
            auto bytes=EncodeCraftEquipmentEnvelope(payload,equipment,CaveCraftMaximumBytes);
            if(playerCheckpoint)bytes=EncodeCraftCheckpoint(bytes,*playerCheckpoint,CaveCraftMaximumBytes+CraftEquipmentOverhead);
            WriteCraftSaveFile(*savePath,bytes,CaveCraftMaximumBytes+CraftEquipmentOverhead+CraftCheckpointOverhead);
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
        CraftMeshBatch<1,P,CaveCraftBaseline,CaveChunk> workers(4);
        Pub pub;
        P::Slots coords{};
        std::optional<CaveChunk> committed,pending;
        std::optional<P> plan;
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
        std::size_t loads=0,evictions=0,retained=0,peakUploads=0;
        RenderScene authored;
        RenderLight light;light.Type=RenderLightType::Directional;
        light.Direction={.6f,1.f,.4f};light.Intensity=3;light.CastShadows=false;
        authored.AddLight(light);
        std::optional<MeshHandle> hudMesh;
        std::optional<CameraPose> hudPose;
        bool hudEnabled=false;
        CraftHudPublication hudPublication;
        CraftVitals vitals;
        CraftNutrition nutrition;
        CraftMining mining;
        CraftCreature creature;
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
            if(creatureMesh&&creature.Body())
            {
                const auto creatureConfig=CraftCreature::Config();
                const auto minimum=PlayerMinimum(*creature.Body(),creatureConfig);
                MeshDraw draw;draw.Mesh=*creatureMesh;draw.Model=Mat4f::Identity();
                draw.Model(0,0)=static_cast<float>(2*creatureConfig.Radius);
                draw.Model(1,1)=static_cast<float>(creatureConfig.Height);
                draw.Model(2,2)=static_cast<float>(2*creatureConfig.Radius);
                draw.Model(0,3)=TerrainRelativeFloat(minimum[0],base.X);
                draw.Model(1,3)=TerrainRelativeFloat(minimum[1],base.Y);
                draw.Model(2,3)=TerrainRelativeFloat(minimum[2],base.Z);
                draw.Material.BaseColor={.95f,.18f,.12f};
                draw.Material.Emissive={.12f,.02f,.01f};
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
        const auto createCreature=[&]()
        {
            if(creatureMesh)return;
            auto cube=BuildCraftMesh(BuildVoxelChunkSurface({0,0,0},1,[](VoxelCoordinate cell){
                return cell==VoxelCoordinate{}?CraftStone:VoxelAir;}));
            for(auto& vertex:cube.Vertices)vertex.Color={1,1,1};
            creatureMesh=upload(std::move(cube));
            if(!creatureMesh)throw std::runtime_error("Creature mesh missing.");
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
            world.swap(next);++worldRevision;return true;
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
        const auto camera=[&](double x,double y,double z,const std::array<double,3>& d)
        {
            CameraPose pose;
            pose.Position={TerrainRelativeFloat(x,origin.X),TerrainRelativeFloat(y,origin.Y),TerrainRelativeFloat(z,origin.Z)};
            pose.Target={TerrainRelativeFloat(x+d[0],origin.X),TerrainRelativeFloat(y+d[1],origin.Y),TerrainRelativeFloat(z+d[2],origin.Z)};
            renderer.SetCameraPose(pose);
            hudPose=pose;
            if(nativeHud)refreshNativeHud();
            else if(hudEnabled)oldSubmit(pub.Active());
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
                            return *image;
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
                if(material==VoxelAir)throw std::runtime_error("Missing cave mining target.");
                const CraftMiningTarget target{{block.X,block.Y,block.Z},material};
                inventory.Counts={0,2,3,0};
                if(!equipment.Craft(inventory.Counts)||equipment.Speed(material)!=2.)
                    throw std::runtime_error("Native cave pick craft failed.");
                const auto speed=equipment.Speed(material);
                const auto holds=static_cast<unsigned>(std::ceil(CraftMiningSeconds(material)/speed/.1-1e-9));
                for(unsigned step=0;step<holds;++step)
                {
                    const bool mined=mining.Advance(true,target,.1,[&](const auto& completed){
                        if(completed!=target||!MineCraftBlock(inventory,block,query,edit))return false;
                        equipment.SuccessfulMine(completed.Material);
                        return true;
                    },speed);
                    const bool last=step+1==holds;
                    if(mined!=last||inventory.Count(material)!=(last?1u:0u)||
                        query(block)!=(last?VoxelAir:material))
                        throw std::runtime_error("Native cave timed mining transaction differed.");
                }
                inventory.Selected=material;
                const auto inner=EncodeCaveCraftSession(seed,*world,inventory);
                const auto envelope=EncodeCraftEquipmentEnvelope(inner,equipment,CaveCraftMaximumBytes);
                const auto equippedSave=DecodeCraftEquipmentEnvelope(envelope,CaveCraftMaximumBytes);
                const auto decoded=DecodeCaveCraftSession(equippedSave.Payload);
                if(equippedSave.Equipment.Durability!=63||!equippedSave.Equipment.Equipped)
                    throw std::runtime_error("Native cave pick durability/save differed.");
                if(decoded.Seed!=seed||decoded.World.Get(block)!=VoxelAir||decoded.Inventory.Count(material)!=1)
                    throw std::runtime_error("Cave session roundtrip failed.");
                settle(CaveObserver(1e9,-24,-1e9));
                capture(1000000008.5,-23.5,-999999991.5,{0,0,-1});
                settle({0,-1,0});
                if(query(block)!=VoxelAir||!PlaceCraftBlock(inventory,block,query,edit,[](auto){return true;})||
                    inventory.Count(material)!=0||!world->Overrides().empty())
                    throw std::runtime_error("Cave revisit failed.");
                const auto baseline=capture(8.5,-7.5,8.5,{0,0,-1});
                createHud();hudEnabled=true;
                const auto normalHud=capture(8.5,-7.5,8.5,{0,0,-1});
                if(normalHud.RGBA==baseline.RGBA)
                    throw std::runtime_error("Cave HUD did not appear.");
                aimedHealth=12;
                if(capture(8.5,-7.5,8.5,{0,0,-1}).RGBA==normalHud.RGBA)
                    throw std::runtime_error("Cave combat HUD capture unchanged.");
                aimedHealth=0;
                if(capture(8.5,-7.5,8.5,{0,0,-1}).RGBA!=normalHud.RGBA)
                    throw std::runtime_error("Cave combat HUD clear did not restore normal HUD.");
                hudEnabled=false;oldSubmit(pub.Active());
                if(capture(8.5,-7.5,8.5,{0,0,-1}).RGBA!=baseline.RGBA)
                    throw std::runtime_error("Removing cave HUD changed world.");
                // Exercise actual native target resize in both aspect directions,
                // independently of interactive window manager behavior.
                for(const CraftHudViewport extent: {CraftHudViewport{480,720},CraftHudViewport{1280,480}})
                {
                    renderer.ResizeViewport(extent.Width,extent.Height);hudViewport=extent;
                    const auto without=capture(8.5,-7.5,8.5,{0,0,-1});
                    if(without.Width!=extent.Width||without.Height!=extent.Height)
                        throw std::runtime_error("HUD resize capture extent differs.");
                    hudEnabled=true;
                    const auto with=capture(8.5,-7.5,8.5,{0,0,-1});
                    if(with.Width!=extent.Width||with.Height!=extent.Height||with.RGBA==without.RGBA)
                        throw std::runtime_error("HUD did not render after viewport resize.");
                    hudEnabled=false;oldSubmit(pub.Active());
                    if(capture(8.5,-7.5,8.5,{0,0,-1}).RGBA!=without.RGBA)
                        throw std::runtime_error("Resized HUD removal changed world.");
                }
                renderer.ResizeViewport(960,540);hudViewport={};
                if(capture(8.5,-7.5,8.5,{0,0,-1}).RGBA!=baseline.RGBA)
                    throw std::runtime_error("Restored viewport changed baseline.");
                std::cout<<"Native portrait and wide HUD resize captures passed\n";
                if(loads!=90||evictions!=63||peakUploads>4)throw std::runtime_error("Cave streaming accounting.");
                std::cout<<"Cave native: 90 loads, 63 evictions, 18 vertical retained, 6 captures, HUD and inventory roundtrip, peak uploads "<<peakUploads<<"; edit revisit passed\n";
            }
            else
            {
                double x=8.5,y=12,z=8.5,yaw=0,pitch=-.5;
                VoxelPlayer body{{8.5,12,8.5}};
                VoxelPlayerConfig config;
                const auto respawn=[&]()
                {
                    const auto spawn=FindCaveSpawn(config,[&](VoxelCoordinate c)->std::optional<VoxelMaterial>{
                        if(!InCaveWorld(c))return {};
                        return world->Get(c);
                    });
                    if(!spawn)throw std::runtime_error("No supported clear cave spawn near the origin.");
                    settle(CaveObserver(spawn->Feet[0],spawn->Feet[1],spawn->Feet[2]));
                    body=*spawn;vitals.Respawn(body.Feet[1]);mining.Reset();nutrition.Reset();creature.Reset();
                };
                if(survival)
                {
                    std::optional<VoxelPlayer> restored;
                    if(playerCheckpoint&&std::abs(playerCheckpoint->Feet[0])<999999900&&
                        std::abs(playerCheckpoint->Feet[2])<999999900&&playerCheckpoint->Feet[1]>CaveBottom&&
                        playerCheckpoint->Feet[1]+config.Height<=CaveTop)
                    {
                        restored=RestoreCraftCheckpoint(*playerCheckpoint,config,
                            [&](VoxelCoordinate c)->std::optional<VoxelMaterial>{
                                if(!InCaveWorld(c))return {};
                                return world->Get(c);});
                        if(restored)
                        {
                            settle(CaveObserver(restored->Feet[0],restored->Feet[1],restored->Feet[2]));
                            if(VoxelPlayerBlocked(*restored,config,query))restored.reset();
                        }
                    }
                    if(restored){body=*restored;vitals.Respawn(body.Feet[1]);}
                    else respawn();
                    if(playerCheckpoint)
                    {
                        vitals.Restore(playerCheckpoint->Health,body.Feet[1]);nutrition.Restore(playerCheckpoint->Hunger);
                        yaw=playerCheckpoint->Yaw;pitch=playerCheckpoint->Pitch;
                    }
                    x=body.Feet[0];y=body.Feet[1]+config.EyeHeight;z=body.Feet[2];
                    createHud();hudEnabled=true;createCreature();
                }
                const auto bodyQuery=[&](VoxelCoordinate c)->std::optional<VoxelMaterial>{
                    if(c.Y<CaveBottom||c.Y>=CaveTop||std::abs(double(c.X))>=999999950||std::abs(double(c.Z))>=999999950)return CraftStone;
                    return query(c);
                };
                std::string previousTitle;
                const auto recordPlayer=[&](){if(survival)playerCheckpoint=CraftCheckpoint{body.Feet,yaw,pitch,playerCheckpoint?playerCheckpoint->DayPhase:.25,vitals.Health(),nutrition.Hunger()};};
                recordPlayer();
                CraftActionInputs actions;
                bool leftHeld=false,rightHeld=false;
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
                    const double dt=std::min(.1,std::chrono::duration<double>(now-previous).count());previous=now;
                    double cursorX=0,cursorY=0;
                    glfwGetCursorPos(window,&cursorX,&cursorY);
                    const auto look=mouseLook.Observe(focused&&mouseCaptured,cursorX,cursorY);
                    ApplyCraftLook(yaw,pitch,look,key(GLFW_KEY_RIGHT)-key(GLFW_KEY_LEFT),
                        key(GLFW_KEY_UP)-key(GLFW_KEY_DOWN),dt);
                    const double f=key(GLFW_KEY_W)-key(GLFW_KEY_S),r=key(GLFW_KEY_D)-key(GLFW_KEY_A),n=std::max(1.,std::hypot(f,r));
                    const double dx=(std::sin(yaw)*f+std::cos(yaw)*r)/n,dz=(-std::cos(yaw)*f+std::sin(yaw)*r)/n;
                    const bool jump=actions.Jump.Press(key(GLFW_KEY_SPACE)>0);
                    const bool sprint=nutrition.CanSprint()&&key(GLFW_KEY_LEFT_SHIFT)>0&&(f!=0||r!=0);
                    const auto previousCreature=creature.Body();
                    if(survival)
                    {
                        AdvanceVoxelPlayer(body,config,{dx,dz,jump,sprint},dt,bodyQuery);
                        auto probe=body;probe.Feet[1]-=.000001;
                        const bool support=body.Grounded&&VoxelPlayerBlocked(probe,config,
                            [&](VoxelCoordinate c){return query(c).value_or(VoxelAir);});
                        (void)vitals.Observe(body.Feet[1],body.Grounded,support);
                        if(focused)nutrition.Advance(dt,sprint,vitals);
                        if(!vitals.Dead())vitals.Damage(creature.Advance(dt,body,query));
                        if(vitals.Dead())respawn();
                        x=body.Feet[0];y=body.Feet[1]+config.EyeHeight;z=body.Feet[2];
                    }
                    else
                    {
                        x=std::clamp(x+12*dt*dx,double(origin.X)-15.,double(origin.X)+30.);
                        z=std::clamp(z+12*dt*dz,double(origin.Z)-15.,double(origin.Z)+30.);
                        y=std::clamp(y+12*dt*(key(GLFW_KEY_SPACE)-key(GLFW_KEY_LEFT_CONTROL)),
                            std::max(-63.,double(origin.Y)-15.),std::min(31.,double(origin.Y)+30.));
                    }
                    (void)update(CaveObserver(x,survival?body.Feet[1]:y,z));
                    const std::array d{std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
                    camera(x,y,z,d);
                    if(survival)
                    {
                        for(unsigned slot=0;slot<4;++slot)
                            if(key(GLFW_KEY_1+static_cast<int>(slot)))inventory.Selected=static_cast<VoxelMaterial>(slot+1);
                        if(actions.RecipeOne.Press(key(GLFW_KEY_R)>0))(void)CraftApplyRecipe(inventory,CraftRecipes[0]);
                        if(actions.RecipeTwo.Press(key(GLFW_KEY_T)>0))(void)CraftApplyRecipe(inventory,CraftRecipes[1]);
                        if(actions.CraftTool.Press(key(GLFW_KEY_F)>0)&&equipment.Craft(inventory.Counts))mining.Reset();
                        if(actions.ToggleTool.Press(key(GLFW_KEY_G)>0)&&equipment.Toggle())mining.Reset();
                        if(actions.Eat.Press(key(GLFW_KEY_E)>0))(void)nutrition.Eat(inventory.Counts);
                        std::string title="Kairo Caves | HP "+std::to_string(vitals.Health())+"/20 | Food "+std::to_string(nutrition.Hunger())+"/20 | ";
                        for(unsigned i=0;i<4;++i)
                        {
                            if(inventory.Selected==i+1u)title+="[";
                            title+=std::to_string(i+1u)+":"+std::to_string(inventory.Counts[i]);
                            if(inventory.Selected==i+1u)title+="]";
                            title+=" ";
                        }
                        title+="| Pick "+std::to_string(equipment.Durability)+"/64"+(equipment.Equipped?" active":" stowed")+
                            " | F craft pick, G equip | E forage (2 grass) | R/T craft | F5 save | Tab mouse";
                        if(creature.Body())title+=" | Sentinel HP "+std::to_string(creature.Health())+"/12";
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
                        const bool combatAim=creature.Aimed({x,y,z},d,query);
                        if(left&&combatAim&&!leftHeld)
                        {
                            const auto combat=CraftStrikeReward(creature,inventory.Counts);
                            if(combat.Defeated)
                                std::cout<<"Stone sentinel defeated; "<<combat.StoneAwarded<<" stone salvaged\n";
                        }
                        const auto hit=((left&&!combatAim)||(right&&!rightHeld))?TraceVoxels({x,y,z},d,8,64,query):std::optional<VoxelHit>{};
                        std::optional<CraftMiningTarget> target;
                        if(hit&&hit->Material>=CraftGrass&&hit->Material<=CraftPlaced&&hit->Cell.Y>CaveBottom)
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
                                [&](auto cell){return !PlayerIntersectsBlock(body,config,cell)&&!creature.Occupies(cell);});
                    }
                    else if((left&&!leftHeld)||(right&&!rightHeld))
                    {
                        if(const auto hit=TraceVoxels({x,y,z},d,8,64,query))
                        {
                            if(left&&!leftHeld)(void)edit(hit->Cell,VoxelAir);
                            else if(hit->Placement)(void)edit(*hit->Placement,CraftPlaced);
                        }
                    }
                    leftHeld=left;rightHeld=right;
                    // Re-query after edits, so a removed target disappears
                    // immediately. Placement is preflight feedback, not a
                    // promise that a subsequent native transaction will succeed.
                    std::optional<CraftTargetKey> aim;
                    aimedHealth=survival&&mouseCaptured&&creature.Aimed({x,y,z},d,query)?creature.Health():0;
                    if(mouseCaptured&&!aimedHealth)
                        if(const auto pointed=TraceVoxels({x,y,z},d,8,64,query))
                        {
                            const bool mineable=InCaveWorld(pointed->Cell)&&(!survival||
                                (pointed->Material>=CraftGrass&&pointed->Material<=CraftPlaced&&pointed->Cell.Y>CaveBottom&&
                                 inventory.Count(pointed->Material)<CraftInventory::StackLimit));
                            aim=CraftTargetKey{targetCell(pointed->Cell),targetCell(origin),mineable,false,std::nullopt};
                            if(pointed->Placement)
                            {
                                const auto cell=*pointed->Placement;
                                aim->Placement=targetCell(cell);
                                aim->Placeable=InCaveWorld(cell)&&query(cell)==VoxelAir&&(!survival||
                                    (inventory.Count(inventory.Selected)>0&&!PlayerIntersectsBlock(body,config,cell)&&!creature.Occupies(cell)));
                            }
                        }
                    publishTarget(aim);
                    const bool creatureChanged=bool(previousCreature)!=bool(creature.Body())||
                        (previousCreature&&creature.Body()&&previousCreature->Feet!=creature.Body()->Feet);
                    if(creatureChanged)oldSubmit(pub.Active());
                    recordPlayer();autosave.Observe(saveStamp());
                    const bool manualSave=actions.Save.Press(key(GLFW_KEY_F5)>0);
                    if(savePath&&(manualSave||autosave.Advance(dt)))(void)checkpoint();
                    if(nativeHud)refreshNativeHud();
                    else if(hudEnabled&&!creatureChanged)oldSubmit(pub.Active());
                    renderer.DrawFrame();
                }
                // Exit is a final checkpoint only for an explicitly supplied
                // save path and changed state. Do not silently lose a failed save.
                recordPlayer();autosave.Observe(saveStamp());
                if(savePath&&autosave.Dirty())finalSaveFailed=!checkpoint();
            }
        }
        catch(...){hudEnabled=false;(void)pub.Release(oldSubmit,destroy);throw;}
        hudEnabled=false;
        creature.Reset();
        publishTarget(std::nullopt);
        refreshNativeHud();
        if(pub.Release(oldSubmit,destroy)!=0)throw std::runtime_error("Cave cleanup failed.");
        if(hudMesh)renderer.DestroyMesh(*hudMesh);
        if(creatureMesh)renderer.DestroyMesh(*creatureMesh);
        return finalSaveFailed?1:0;
    }
    catch(const std::exception& e){std::cerr<<"Craft Caves failed: "<<e.what()<<'\n';return 1;}
}
