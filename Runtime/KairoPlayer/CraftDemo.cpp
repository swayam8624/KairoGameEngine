#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "CraftWorld.hpp"
#include "CraftSave.hpp"
#include "VoxelPlayer.hpp"
#include <filesystem>
#include <fstream>
#include "VoxelMeshPublication.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <charconv>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

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
        std::optional<std::filesystem::path> savePath;
        GraphicsBackend backend=GraphicsBackend::Automatic;
        for(int i=1;i<argc;++i)
        {
            const std::string_view arg=argv[i];
            if(arg=="--smoke")smoke=true;
            else if(arg=="--survival")survival=true;
            else if(arg=="--save"&&i+1<argc)savePath=std::filesystem::path(argv[++i]);
            else if(arg=="--renderer"&&i+1<argc)backend=ParseGraphicsBackend(argv[++i]);
            else if(arg=="--seed"&&i+1<argc)
            {
                const std::string_view value=argv[++i];
                const auto result=std::from_chars(value.data(),value.data()+value.size(),seed);
                if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size())
                    throw std::invalid_argument("Invalid world seed.");
            }
            else throw std::invalid_argument("Usage: KairoCraft [--seed N] [--renderer backend] [--save path] [--survival] [--smoke]");
        }
        RendererRuntime renderer({"Kairo Craft: WASD / arrows / Space / Ctrl / mouse buttons / Esc",960u,540u,!smoke,backend});
        renderer.ResizeViewport(960,540);
        renderer.SetViewportShadingMode(ViewportShadingMode::Lit);
        using World=VoxelEdits<CraftBaseline>;
        auto world=std::make_unique<World>(CraftBaseline{seed});
        if(smoke&&savePath)throw std::invalid_argument("Smoke mode does not modify user save files.");
        if(savePath&&std::filesystem::exists(*savePath))
        {
            const auto length=std::filesystem::file_size(*savePath);
            if(length>CraftSaveMaximumBytes)throw std::length_error("Craft save exceeds size cap.");
            std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
            std::ifstream file(*savePath,std::ios::binary);
            if(!file||!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))||
                file.peek()!=std::char_traits<char>::eof())
                throw std::runtime_error("Craft save read failed.");
            auto loaded=DecodeCraftSave(bytes);
            seed=loaded.Seed;
            world=std::make_unique<World>(std::move(loaded.World));
        }
        const VoxelPlayerConfig bodyConfig;
        const auto bodyQuery=[&](VoxelCoordinate c)->std::optional<VoxelMaterial>
        {
            // Invisible containment at the finite sandbox boundary; the
            // floor remains solid if every editable block is removed.
            if(c.X<CraftFirst||c.X>=CraftLast||c.Z<CraftFirst||c.Z>=CraftLast||c.Y<0)return CraftStone;
            return world->Get(c);
        };
        const auto spawn=[&]()
        {
            VoxelPlayer player{{8.5,16.,12.5}};
            for(int y=CraftExtent-1;y>=0;--y)
                if(world->Get({8,y,12})!=VoxelAir){player.Feet[1]=y+1.;break;}
            AdvanceVoxelPlayer(player,bodyConfig,{},0,bodyQuery);
            return player;
        };
        const auto save=[&]()
        {
            if(!savePath)return;
            const auto bytes=EncodeCraftSave(seed,*world);
            auto temporary=*savePath;temporary+=".tmp";
            if(std::filesystem::exists(temporary))
                throw std::runtime_error("Craft temporary save already exists; inspect it before retrying.");
            // Single-writer sample. Same-directory rename avoids deleting
            // the previous save if writing the replacement fails.
            try
            {
                std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
                if(!file||!file.write(reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size())))
                    throw std::runtime_error("Craft temporary save write failed.");
                file.close();
                if(!file)throw std::runtime_error("Craft temporary save close failed.");
                std::filesystem::rename(temporary,*savePath);
            }
            catch(...)
            {
                std::error_code ignored;std::filesystem::remove(temporary,ignored);
                throw;
            }
            std::cout<<"Craft saved "<<world->Overrides().size()<<" edits\n";
        };
        using Publication=VoxelMeshPublication<MeshHandle,CraftChunkCount>;
        Publication publication;
        RenderScene authored;
        RenderLight sun;
        sun.Type=RenderLightType::Directional;
        sun.Direction={.6f,1.f,.4f};sun.Intensity=3.f;sun.CastShadows=false;
        authored.AddLight(sun);
        const auto submit=[&](const Publication::Slots& slots)
        {
            auto scene=authored;
            for(std::size_t i=0;i<slots.size();++i)if(slots[i])
            {
                const auto origin=CraftChunkOrigin(i);
                MeshDraw draw;
                draw.Mesh=*slots[i];
                draw.Model=MakeTranslation(Vec3f{float(origin.X),float(origin.Y),float(origin.Z)});
                draw.CastShadows=false;
                scene.Add(draw);
            }
            renderer.SubmitRenderScene(scene);
        };
        const auto destroy=[&](MeshHandle h){renderer.DestroyMesh(h);};
        const auto upload=[&](const World& source,std::size_t i)->std::optional<MeshHandle>
        {
            const auto surface=BuildVoxelChunkSurface(CraftChunkOrigin(i),CraftExtent,
                [&](VoxelCoordinate c){return source.Get(c);});
            const auto mesh=BuildCraftMesh(surface);
            if(mesh.Indices.empty())return {};
            std::vector<MeshVertex> vertices;
            vertices.reserve(mesh.Vertices.size());
            for(const auto& v:mesh.Vertices)
                vertices.push_back({{v.Position[0],v.Position[1],v.Position[2]},
                    {v.Color[0],v.Color[1],v.Color[2]},
                    {v.Normal[0],v.Normal[1],v.Normal[2]},
                    {v.UV[0],v.UV[1]}});
            return renderer.CreateMesh(Mesh(std::move(vertices),mesh.Indices));
        };
        const auto edit=[&](VoxelCoordinate cell,VoxelMaterial material)
        {
            if(!InCraftWorld(cell)||world->Get(cell)==material)return false;
            auto candidate=std::make_unique<World>(*world);
            if(!candidate->Set(cell,material))return false;
            if(!publication.Apply(CraftDirtyChunks(cell),
                [&](std::size_t i){return upload(*candidate,i);},submit,destroy))return false;
            world.swap(candidate); // commit occupancy only after mesh publication
            return true;
        };
        // Explicit cleanup before the renderer leaves scope, including when
        // an upload or partially published scene raises an exception.
        try
        {
            std::array<bool,CraftChunkCount> all;all.fill(true);
            if(!publication.Apply(all,[&](std::size_t i){return upload(*world,i);},submit,destroy))
                throw std::runtime_error("Initial voxel mesh publication failed.");
            if(smoke)
            {
                if(survival)
                {
                    auto player=spawn();
                    const double startY=player.Feet[1];
                    if(!player.Grounded)throw std::runtime_error("Survival spawn not supported.");
                    double peak=0;
                    for(unsigned i=0;i<240;++i)
                    {
                        AdvanceVoxelPlayer(player,bodyConfig,{0,0,i==0,false},1./120,bodyQuery);
                        peak=std::max(peak,player.Feet[1]-startY);
                        if(VoxelPlayerBlocked(player,bodyConfig,bodyQuery))throw std::runtime_error("Survival penetration.");
                    }
                    if(!player.Grounded||peak<1.2||peak>1.3)throw std::runtime_error("Survival jump failed.");
                    CameraPose view;
                    view.Position={float(player.Feet[0]),float(player.Feet[1]+bodyConfig.EyeHeight),float(player.Feet[2])};
                    view.Target={view.Position.x,view.Position.y-.3f,view.Position.z-1.f};
                    renderer.SetCameraPose(view);
                    renderer.RequestViewportCapture();
                    bool captured=false;
                    for(unsigned i=0;i<12;++i)
                    {
                        renderer.DrawFrame();
                        if(auto image=renderer.TakeViewportCapture())
                        {captured=image->IsVisuallyNonUniform();break;}
                    }
                    if(!captured)throw std::runtime_error("Survival camera capture failed.");
                    std::cout<<"Craft survival jump: "<<peak<<" m; grounded capture passed\n";
                }
                CameraPose camera;
                camera.Position={.5f,12.f,.5f};
                camera.Target={.5f,0.f,.5f};camera.Up={0.f,0.f,-1.f};
                renderer.SetCameraPose(camera);
                const auto capture=[&]()
                {
                    renderer.RequestViewportCapture();
                    for(unsigned i=0;i<12;++i)
                    {
                        renderer.DrawFrame();
                        if(auto image=renderer.TakeViewportCapture())
                        {
                            if(!image->IsVisuallyNonUniform())throw std::runtime_error("Craft capture blank.");
                            return *image;
                        }
                    }
                    throw std::runtime_error("Craft capture unavailable.");
                };
                const auto ray=[&]{return TraceVoxels({.5,12,.5},{0,-1,0},12,32,
                    [&](VoxelCoordinate c){return world->Get(c);});};
                const auto initial=ray();
                if(!initial||initial->Material!=CraftGrass)throw std::runtime_error("Craft initial pick failed.");
                const auto before=capture();
                if(!edit(initial->Cell,VoxelAir))throw std::runtime_error("Craft removal failed.");
                const auto removed=capture();
                const auto hit=ray();
                if(!hit||hit->Placement!=initial->Cell||world->Get(initial->Cell)!=VoxelAir)
                    throw std::runtime_error("Craft exposed placement failed.");
                if(before.RGBA==removed.RGBA)throw std::runtime_error("Removal did not change native image.");
                if(!edit(*hit->Placement,CraftPlaced))throw std::runtime_error("Craft placement failed.");
                const auto saved=DecodeCraftSave(EncodeCraftSave(seed,*world));
                if(saved.Seed!=seed||saved.World.Overrides()!=world->Overrides())
                    throw std::runtime_error("Craft edited-world save roundtrip failed.");
                const auto placed=capture();
                if(placed.RGBA==removed.RGBA||placed.RGBA==before.RGBA)
                    throw std::runtime_error("Placement image did not change.");
                if(!edit(initial->Cell,initial->Material)||!world->Overrides().empty())
                    throw std::runtime_error("Craft baseline restore failed.");
                const auto restored=capture();
                if(restored.Width!=before.Width||restored.Height!=before.Height||restored.RGBA!=before.RGBA)
                    throw std::runtime_error("Craft restoration image differs.");
                std::cout<<"Craft: 9 native chunks, boundary remove/place/restore, 4 captures, exact restored image passed\n";
            }
            else
            {
                // Creative fly mode for the bounded sandbox. No claim of
                // player-body collision, infinite streaming or persistence.
                double x=8,y=12,z=12,yaw=0,pitch=-.5;
                auto player=spawn();
                bool jumpHeld=false;
                bool leftHeld=false,rightHeld=false,saveHeld=false;
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
                    const double scale=8*dt/std::max(1.,std::hypot(f,r));
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
                        x=std::clamp(x+(std::sin(yaw)*f+std::cos(yaw)*r)*scale,-24.,40.);
                        z=std::clamp(z+(-std::cos(yaw)*f+std::sin(yaw)*r)*scale,-24.,40.);
                        y=std::clamp(y+(key(GLFW_KEY_SPACE)-key(GLFW_KEY_LEFT_CONTROL))*8*dt,1.,32.);
                    }
                    jumpHeld=jump;
                    const std::array direction{std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
                    CameraPose camera;
                    camera.Position={float(x),float(y),float(z)};
                    camera.Target={float(x+direction[0]),float(y+direction[1]),float(z+direction[2])};
                    renderer.SetCameraPose(camera);
                    const bool left=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
                    const bool right=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS;
                    if((left&&!leftHeld)||(right&&!rightHeld))
                    {
                        const auto hit=TraceVoxels({x,y,z},direction,8,64,[&](VoxelCoordinate c){return world->Get(c);});
                        if(hit)
                        {
                            if(left&&!leftHeld)(void)edit(hit->Cell,VoxelAir);
                            else if(hit->Placement&&(!survival||!PlayerIntersectsBlock(player,bodyConfig,*hit->Placement)))
                                (void)edit(*hit->Placement,CraftPlaced);
                        }
                    }
                    leftHeld=left;rightHeld=right;
                    const bool saveDown=key(GLFW_KEY_F5)>0;
                    if(saveDown&&!saveHeld)
                    {
                        try{save();}catch(const std::exception& error){std::cerr<<"Save failed: "<<error.what()<<'\n';}
                    }
                    saveHeld=saveDown;
                    renderer.DrawFrame();
                }
            }
        }
        catch(...)
        {
            (void)publication.Release(submit,destroy);
            throw;
        }
        if(publication.Release(submit,destroy)!=0)throw std::runtime_error("Craft cleanup retained handles.");
        std::cout<<"Craft shutdown passed\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<"Craft failed: "<<e.what()<<'\n';return 1;}
}
