#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "CraftCaveSession.hpp"
#include "CraftRecipesHud.hpp"
#include "CraftHudProjection.hpp"
#include "CraftHudPublication.hpp"
#include "CraftVitals.hpp"
#include "CraftMining.hpp"
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
        if(savePath&&std::filesystem::exists(*savePath))
        {
            const auto size=std::filesystem::file_size(*savePath);
            if(size>CaveCraftMaximumBytes)throw std::length_error("Cave save size cap.");
            std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
            std::ifstream file(*savePath,std::ios::binary);
            if(!file||!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))||
                file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Cave save read failed.");
            auto decoded=DecodeCaveCraftSession(bytes);
            seed=decoded.Seed;world=std::make_shared<W>(std::move(decoded.World));inventory=decoded.Inventory;
        }
        // Explicit save only, using the same single-writer replacement policy
        // as the streamed surface world. A failed write leaves the old save.
        const auto save=[&]()
        {
            if(!savePath)return;
            const auto bytes=EncodeCaveCraftSession(seed,*world,inventory);
            auto temp=*savePath;temp+=".tmp";
            if(std::filesystem::exists(temp))throw std::runtime_error("Temporary cave save already exists.");
            try
            {
                std::ofstream file(temp,std::ios::binary|std::ios::trunc);
                if(!file||!file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
                    throw std::runtime_error("Cave save write failed.");
                file.close();
                if(!file)throw std::runtime_error("Cave save close failed.");
                std::filesystem::rename(temp,*savePath);
            }
            catch(...)
            {
                std::error_code ignored;std::filesystem::remove(temp,ignored);throw;
            }
        };
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
        std::optional<MeshHandle> hudMesh;
        std::optional<CameraPose> hudPose;
        bool hudEnabled=false;
        CraftHudPublication hudPublication;
        CraftVitals vitals;
        CraftMining mining;
        const auto refreshNativeHud=[&]()
        {
            if(!nativeHud)return;
            const CraftHudKey key{hudEnabled,inventory.Counts,inventory.Selected,hudViewport.Width,hudViewport.Height,vitals.Health(),mining.Progress()};
            (void)hudPublication.Refresh(key,[&]()
            {
                std::vector<ViewportOverlayRect> rects;
                if(hudEnabled)
                {
                    const auto layout=BuildCraftHud(inventory,vitals.Health(),mining.Progress());
                    rects.reserve(layout.size());
                    // Paint opaque backing first, then icons and numerals.
                    // No camera, world origin or depth state is involved.
                    for(bool backing:{true,false})for(const auto& rect:layout)
                    {
                        if(rect.Background!=backing)continue;
                        const auto p=QuantizeCraftHudRect(hudViewport,rect.X,rect.Y,rect.W,rect.H,rect.Y>=450);
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
            if(!nativeHud&&hudEnabled&&hudMesh&&hudPose)
            {
                const auto& pose=*hudPose;
                const ScreenVec eye{pose.Position.x,pose.Position.y,pose.Position.z};
                const auto forward=ScreenUnit(ScreenVec{double(pose.Target.x)-eye[0],
                    double(pose.Target.y)-eye[1],double(pose.Target.z)-eye[2]});
                const auto right=ScreenUnit(ScreenCross(forward,{pose.Up.x,pose.Up.y,pose.Up.z}));
                const auto up=ScreenUnit(ScreenCross(right,forward));
                for(const auto& rect:BuildCraftHud(inventory,vitals.Health(),mining.Progress()))
                {
                    const double depth=rect.Background?.16:.14;
                    const auto projected=ProjectCraftHudRect(hudViewport,rect.X,rect.Y,rect.W,rect.H,
                        pose.VerticalFovRadians,depth,rect.Y>=450);
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
                const auto holds=static_cast<unsigned>(std::ceil(CraftMiningSeconds(material)/.1-1e-9));
                for(unsigned step=0;step<holds;++step)
                {
                    const bool mined=mining.Advance(true,target,.1,[&](const auto& completed){
                        return completed==target&&MineCraftBlock(inventory,block,query,edit);
                    });
                    const bool last=step+1==holds;
                    if(mined!=last||inventory.Count(material)!=(last?1u:0u)||
                        query(block)!=(last?VoxelAir:material))
                        throw std::runtime_error("Native cave timed mining transaction differed.");
                }
                inventory.Selected=material;
                const auto decoded=DecodeCaveCraftSession(EncodeCaveCraftSession(seed,*world,inventory));
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
                if(capture(8.5,-7.5,8.5,{0,0,-1}).RGBA==baseline.RGBA)
                    throw std::runtime_error("Cave HUD did not appear.");
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
                    body=*spawn;vitals.Respawn(body.Feet[1]);mining.Reset();
                };
                if(survival)
                {
                    respawn();
                    x=body.Feet[0];y=body.Feet[1]+config.EyeHeight;z=body.Feet[2];
                    createHud();hudEnabled=true;
                }
                const auto bodyQuery=[&](VoxelCoordinate c)->std::optional<VoxelMaterial>{
                    if(c.Y<CaveBottom||c.Y>=CaveTop||std::abs(double(c.X))>=999999950||std::abs(double(c.Z))>=999999950)return CraftStone;
                    return query(c);
                };
                std::string previousTitle;
                bool leftHeld=false,rightHeld=false,jumpHeld=false,recipeOneHeld=false,recipeTwoHeld=false,saveHeld=false;
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
                    const double dt=std::min(.1,std::chrono::duration<double>(now-previous).count());previous=now;
                    yaw=std::remainder(yaw+(key(GLFW_KEY_RIGHT)-key(GLFW_KEY_LEFT))*1.8*dt,6.283185307179586);
                    pitch=std::clamp(pitch+(key(GLFW_KEY_UP)-key(GLFW_KEY_DOWN))*1.4*dt,-1.45,1.45);
                    const double f=key(GLFW_KEY_W)-key(GLFW_KEY_S),r=key(GLFW_KEY_D)-key(GLFW_KEY_A),n=std::max(1.,std::hypot(f,r));
                    const double dx=(std::sin(yaw)*f+std::cos(yaw)*r)/n,dz=(-std::cos(yaw)*f+std::sin(yaw)*r)/n;
                    const bool jump=key(GLFW_KEY_SPACE)>0;
                    if(survival)
                    {
                        AdvanceVoxelPlayer(body,config,{dx,dz,jump&&!jumpHeld,key(GLFW_KEY_LEFT_SHIFT)>0},dt,bodyQuery);
                        auto probe=body;probe.Feet[1]-=.000001;
                        const bool support=body.Grounded&&VoxelPlayerBlocked(probe,config,
                            [&](VoxelCoordinate c){return query(c).value_or(VoxelAir);});
                        (void)vitals.Observe(body.Feet[1],body.Grounded,support);
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
                    jumpHeld=jump;
                    (void)update(CaveObserver(x,survival?body.Feet[1]:y,z));
                    const std::array d{std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
                    camera(x,y,z,d);
                    if(survival)
                    {
                        for(unsigned slot=0;slot<4;++slot)
                            if(key(GLFW_KEY_1+static_cast<int>(slot)))inventory.Selected=static_cast<VoxelMaterial>(slot+1);
                        const bool one=key(GLFW_KEY_R)>0,two=key(GLFW_KEY_T)>0;
                        if(one&&!recipeOneHeld)(void)CraftApplyRecipe(inventory,CraftRecipes[0]);
                        if(two&&!recipeTwoHeld)(void)CraftApplyRecipe(inventory,CraftRecipes[1]);
                        recipeOneHeld=one;recipeTwoHeld=two;
                        std::string title="Kairo Caves | HP "+std::to_string(vitals.Health())+"/20 | ";
                        for(unsigned i=0;i<4;++i)
                        {
                            if(inventory.Selected==i+1u)title+="[";
                            title+=std::to_string(i+1u)+":"+std::to_string(inventory.Counts[i]);
                            if(inventory.Selected==i+1u)title+="]";
                            title+=" ";
                        }
                        title+="| Hold LMB to mine | R/T craft | F5 save";
                        if(title!=previousTitle){glfwSetWindowTitle(window,title.c_str());previousTitle=std::move(title);}
                    }
                    const bool saveDown=key(GLFW_KEY_F5)>0;
                    if(saveDown&&!saveHeld)
                    {
                        try{save();}catch(const std::exception& ex){std::cerr<<"Cave save failed: "<<ex.what()<<'\n';}
                    }
                    saveHeld=saveDown;
                    const bool left=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
                    const bool right=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS;
                    if(survival)
                    {
                        const auto hit=(left||(right&&!rightHeld))?TraceVoxels({x,y,z},d,8,64,query):std::optional<VoxelHit>{};
                        std::optional<CraftMiningTarget> target;
                        if(hit&&hit->Material>=CraftGrass&&hit->Material<=CraftPlaced&&hit->Cell.Y>CaveBottom)
                            target=CraftMiningTarget{{hit->Cell.X,hit->Cell.Y,hit->Cell.Z},hit->Material};
                        (void)mining.Advance(left,target,dt,[&](const CraftMiningTarget& completed)
                        {
                            const VoxelCoordinate cell{completed.Cell[0],completed.Cell[1],completed.Cell[2]};
                            if(query(cell)!=completed.Material)return false;
                            return MineCraftBlock(inventory,cell,query,edit);
                        });
                        if(!left&&right&&!rightHeld&&hit&&hit->Placement)
                            (void)PlaceCraftBlock(inventory,*hit->Placement,query,edit,
                                [&](auto cell){return !PlayerIntersectsBlock(body,config,cell);});
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
                    if(nativeHud)refreshNativeHud();
                    else if(hudEnabled)oldSubmit(pub.Active());
                    renderer.DrawFrame();
                }
            }
        }
        catch(...){hudEnabled=false;(void)pub.Release(oldSubmit,destroy);throw;}
        hudEnabled=false;
        refreshNativeHud();
        if(pub.Release(oldSubmit,destroy)!=0)throw std::runtime_error("Cave cleanup failed.");
        if(hudMesh)renderer.DestroyMesh(*hudMesh);
        return 0;
    }
    catch(const std::exception& e){std::cerr<<"Craft Caves failed: "<<e.what()<<'\n';return 1;}
}
