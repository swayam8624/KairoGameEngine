// Native host for authored 2D worlds; the portable fixed-step core and view
// remain independent of GLFW and graphics backend modules.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "RuntimeWorld2DView.hpp"
#include "RuntimeFrameClock.hpp"
#include "World2DCampaignProject.hpp"
#include "RuntimeNativeAudioOutput.hpp"
#include "RuntimeGameLabView.hpp"
#include <memory>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

import Kairo.Renderer;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimeInputBridge;
import Kairo.EngineCore.InputMap;
import Kairo.EngineCore.Input;
import Kairo.Assets.TextureArtifact;
import Kairo.Foundation.Math;
import Kairo.Assets.WavImporter;
import Kairo.Assets.AudioArtifact;
import Kairo.Assets.Importer;
import Kairo.Assets.Types;
import Kairo.EngineCore.AudioRuntime;
import Kairo.EngineCore.LogicBytecode;
import Kairo.EngineCore.Entity;
import Kairo.Foundation.Math.Vector;
#include "World2DNativeLogic.hpp"

#include "World2DNativeAudio.hpp"

#include "World2DNativeAtlas.hpp"

namespace kairo::player::world2d
{
    void ValidateWorldLogic(const CampaignProject& campaign,const kairo::player::RuntimeProject& project)
    {
        if(campaign.Logic)
        {
            ValidateWorldLogicInputs(*campaign.Logic,project.InputMap());
            WorldLogic compiled(*campaign.Logic,[](unsigned){});
        }
    }
    void ValidateWorldAudio(const CampaignProject& campaign,const std::filesystem::path& root)
    {
        if(campaign.Audio)(void)LoadWorldAudio(root,*campaign.Audio);
    }

    int Run(CampaignProject campaign,const kairo::player::RuntimeProject& project,
        kairo::renderer::GraphicsBackend backend,bool smoke,bool rectangles,bool logicTrace)
    {
        using namespace kairo::renderer;
        const auto levelCount=static_cast<unsigned>(campaign.Worlds.size());
        Progress progress;
        bool checkpointWritable=true;
        if(!smoke)
        {
            try{progress=ReadCheckpoint(project.Root(),campaign);}
            catch(const std::exception& e)
            {
                std::cerr<<"World2D checkpoint ignored; NewGame required to replace it: "<<e.what()<<'\n';
                checkpointWritable=false;
            }
        }
        Game game(campaign.Worlds.at(progress.Level));
        bool paused=false;
        EventMixer mixer(campaign.Config);
        std::unique_ptr<WorldAudio> authoredAudio;
        if(campaign.Audio)authoredAudio=std::make_unique<WorldAudio>(*campaign.Audio,LoadWorldAudio(project.Root(),*campaign.Audio));
        const auto clearEffects=[&]{mixer.Clear();if(authoredAudio)authoredAudio->ClearEffects();};
        const auto trigger=[&](unsigned event){if(authoredAudio)authoredAudio->Trigger(event);else mixer.Trigger(event);};
        std::unique_ptr<WorldLogic> logic;
        using LogicEvent=logic2d::Event;
        if(campaign.Logic)
        {
            logic=std::make_unique<WorldLogic>(*campaign.Logic,trigger,logicTrace);
            logic->Restore(progress.ScriptValues);
            logic->SelectLevel(campaign.Config.Levels.at(progress.Level));
        }
        const auto signal=[&](logic2d::Signal event){if(logic)logic->Dispatch(game,event);};
        const auto beginLogic=[&]{if(logic)logic->Begin(game,campaign.Config.Levels.at(progress.Level));};
        if(!progress.Finished)beginLogic();
        std::optional<vfx2d::System> effects;
        if(campaign.VFX)effects.emplace(*campaign.VFX);
        const auto beginEffects=[&]{if(effects)effects->Begin(campaign.Config.Levels.at(progress.Level));};
        beginEffects();
        const auto origin=[&](unsigned id,bool activeOnly)->std::optional<std::pair<int,int>>
        {
            const auto& actors=game.Definition().Actors;
            const auto it=std::find_if(actors.begin(),actors.end(),[&](const Actor& a){return a.ID==id;});
            if(it==actors.end())return {};
            const auto& a=game.Actors().at(static_cast<std::size_t>(it-actors.begin()));
            if(activeOnly&&!a.Active)return {};
            return std::pair{a.Bounds.X+a.Bounds.W/2,a.Bounds.Y+a.Bounds.H/2};
        };
        const auto activeOrigin=[&](unsigned id){return origin(id,true);};
        const auto burst=[&](vfx2d::Event event){if(effects)effects->Burst(event,[&](unsigned id){return origin(id,false);});};
        std::unique_ptr<kairo::player::detail::RuntimeNativeAudioOutput> audio;
        if(!smoke)
        {
            try
            {
                audio=std::make_unique<kairo::player::detail::RuntimeNativeAudioOutput>(48000);
                if(!audio->Enabled())audio.reset();
            }
            catch(const std::exception& e){std::cerr<<"World2D audio unavailable: "<<e.what()<<'\n';}
        }
        const auto save=[&]()
        {
            if(!checkpointWritable)return;
            if(logic)progress.ScriptValues=logic->Values();
            try{WriteCheckpoint(project.Root(),campaign,progress);}
            catch(const std::exception& e){std::cerr<<"World2D checkpoint write failed: "<<e.what()<<'\n';}
        };
        RendererRuntime renderer({project.Descriptor().Name+" - Kairo 2D",1280u,720u,!smoke,backend});
        kairo::player::RuntimeInputBridge input(project.InputMap());
        if(!renderer.SupportsViewportOverlay())
            throw std::runtime_error("World2D currently requires a Metal or Vulkan viewport overlay.");
        const auto [width,height]=renderer.NativeWindow().FramebufferExtent();
        if(!width||!height)throw std::runtime_error("World2D framebuffer unavailable.");
        renderer.ResizeViewport(width,height);
        renderer.SubmitRenderScene(RenderScene{});
        NativeAtlas atlas(renderer);
        unsigned atlasLevel=progress.Level;
        if(!rectangles)atlas.Load(game.Definition());
        std::vector<DrawRect> rects;
        std::vector<ViewportOverlayRect> native;
        rects.reserve(MaxDrawRects);native.reserve(MaxDrawRects);
        ui2d::Pointer pointer;
        ui2d::Keyboard keyboard;
        std::vector<ui2d::Rect> uiRects;
        std::vector<vfx2d::Rect> particleRects;particleRects.reserve(1024);
        const auto page=[&]{return progress.Finished?ui2d::Screen::Complete:paused?ui2d::Screen::Pause:ui2d::Screen::Play;};
        const auto publish=[&](unsigned w,unsigned h)
        {
            if(paused||progress.Finished)
                renderer.SubmitRenderScene(RenderScene{});
            else if(rectangles)BuildView(game,w,h,rects,!campaign.UI);
            else
            {
                if(atlasLevel!=progress.Level){atlas.Load(game.Definition());atlasLevel=progress.Level;}
                atlas.Publish(game,w,h);
                BuildAtlasHud(game,w,h,rects,!campaign.UI);
            }
            if((paused||progress.Finished)&&!campaign.UI)
            {
                // A modal replaces the scene rather than exceeding the native draw budget.
                using namespace kairo::player::detail;
                RuntimeGridCanvas canvas(w,h,240,100);
                GameLabRects menu;
                AddGameRect(menu,canvas.Rect(0,0,240,100),{.05f,.07f,.1f});
                GameLabText(menu,canvas,10,14,220,campaign.Config.Title,{.85f,.9f,1.f});
                GameLabText(menu,canvas,10,36,220,progress.Finished?"CAMPAIGN COMPLETE":"PAUSED",{.4f,.8f,.65f});
                if(!progress.Finished)GameLabText(menu,canvas,10,56,220,"PAUSE TO RESUME",{1,1,1});
                GameLabText(menu,canvas,10,70,220,"NEW GAME / QUIT",{1,1,1});
                rects.clear();
                for(const auto& item:menu)
                {
                    const auto color=item.Tint;
                    const auto rgb=(unsigned(color[0]*255)<<16)|(unsigned(color[1]*255)<<8)|unsigned(color[2]*255);
                    rects.push_back({{item.Box.X,item.Box.Y,item.Box.W,item.Box.H},rgb});
                }
            }
            uiRects.clear();
            if(campaign.UI)
            {
                if(paused||progress.Finished){rects.clear();rects.push_back({{0,0,int(w),int(h)},0x10161f});}
                ui2d::Paint(*campaign.UI,page(),{game.Collected(),game.Total(),game.Deaths(),progress.Level+1,levelCount,campaign.Config.Title,logic?logic->Value():0},pointer,w,h,uiRects,keyboard.Focus());
            }
            // Particles use native opaque overlay rectangles, below the
            // authored HUD/modal widgets. Reserve UI budget first.
            if(effects&&!paused&&(!progress.Finished||campaign.UI))
            {
                const auto c=game.Camera();
                const auto used=rects.size()+uiRects.size();
                effects->Paint(c.X,c.Y,c.W,c.H,w,h,particleRects,used<MaxDrawRects?std::min<std::size_t>(1024,MaxDrawRects-used):0);
                for(const auto& item:particleRects)rects.push_back({{item.X,item.Y,item.W,item.H},item.RGB});
            }
            for(const auto& item:uiRects)rects.push_back({{item.Bounds.X,item.Bounds.Y,item.Bounds.W,item.Bounds.H},item.RGB});
            if(rects.size()>MaxDrawRects)throw std::length_error("Combined World2D/UI rectangle budget exceeded; use the textured renderer or simplify the UI.");
            native.clear();
            for(const auto& rect:rects)
            {
                const auto b=rect.Bounds;const auto rgb=rect.RGB;
                native.push_back({b.X,b.Y,static_cast<unsigned>(b.W),static_cast<unsigned>(b.H),
                    {float((rgb>>16)&255u)/255.f,float((rgb>>8)&255u)/255.f,float(rgb&255u)/255.f}});
            }
            renderer.SetViewportOverlay(native);
        };
        publish(width,height);
        if(smoke)
        {
            const auto captureScreen=[&]()
            {
                renderer.RequestViewportCapture();
                for(unsigned i=0;i<16;++i)
                {
                    renderer.NativeWindow().PollEvents();
                    renderer.DrawFrame();
                    if(auto capture=renderer.TakeViewportCapture())
                    {
                        if(!capture->IsVisuallyNonUniform())throw std::runtime_error("World2D capture is uniform.");
                        if(!rectangles||campaign.UI)VerifyAtlasCapture(game,*capture,renderer.Backend(),rects,!campaign.UI);
                        return;
                    }
                }
                throw std::runtime_error("World2D capture unavailable.");
            };
            captureScreen();
            if(effects)
            {
                // Exercise the real sample effect path as well as the
                // ordinary atlas capture; no checkpoint or device needed.
                burst(vfx2d::Event::Collect);
                for(unsigned i=0;i<8;++i)effects->Step(activeOrigin);
                publish(width,height);
                if(!effects->Particles().empty())captureScreen();
                std::cout<<"Authored VFX smoke: "<<effects->Particles().size()<<" live particles\n";
            }
            if(authoredAudio)
            {
                authoredAudio->Update(game,campaign.Config.Levels.at(progress.Level),false,false);
                authoredAudio->Trigger(0);authoredAudio->Trigger(1);authoredAudio->Trigger(2);
                for(unsigned i=0;i<4;++i)(void)authoredAudio->Mix();
                std::cout<<"Authored audio headless mixer smoke passed\n";
            }
            if(campaign.UI)
            {
                paused=true;publish(width,height);captureScreen();
                paused=false;progress.Finished=true;publish(width,height);captureScreen();
                std::cout<<"Authored play, pause and completion UI captures passed\n";
            }
            std::cout<<"Native World2D "<<(rectangles?"rectangle":"textured atlas")<<" viewport smoke passed\n";
            return 0;
        }
        auto* window=renderer.NativeWindow().NativeHandle();
        const auto updateTitle=[&]()
        {
            const auto title=campaign.Config.Title+" | "+std::to_string(progress.Level+1)+"/"+std::to_string(levelCount);
            glfwSetWindowTitle(window,title.c_str());
        };
        updateTitle();
        kairo::player::detail::RuntimeFrameClock clock;
        const auto start=std::chrono::steady_clock::now();
        unsigned lastW=width,lastH=height;
        double accumulator=0;
        bool jumpQueued=false;
        // Revision increments on any new simulation or modal state; redraw
        // the overlay only when the authoritative visual inputs changed.
        std::uint64_t visualRevision=0,publishedRevision=0;
        while(!renderer.NativeWindow().ShouldClose())
        {
            renderer.NativeWindow().PollEvents();
            const auto [w,h]=renderer.NativeWindow().FramebufferExtent();
            const bool active=w&&h&&glfwGetWindowAttrib(window,GLFW_FOCUSED)==GLFW_TRUE&&
                glfwGetWindowAttrib(window,GLFW_ICONIFIED)!=GLFW_TRUE;
            input.Poll(renderer.NativeWindow());
            const auto action=[&](std::string_view name)
            {
                return input.HasAction(name)?input.Action(name):kairo::engine::InputActionState{};
            };
            const auto jump=action("Jump");
            const auto dt=clock.Sample(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(),active);
            if(!active)
            {
                if(campaign.UI)
                {
                    if(pointer.Hover()||pointer.Pressed()||keyboard.Focus())++visualRevision;
                    (void)keyboard.Update(*campaign.UI,page(),w,h,false,false,false,false);
                    (void)pointer.Update(*campaign.UI,page(),w,h,0,0,glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS,false);
                }
                accumulator=0;jumpQueued=false;clearEffects();
                // No PCM is generated while unfocused; queued frames drain.
                std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;
            }
            ui2d::Action uiAction=ui2d::Action::None;
            if(campaign.UI)
            {
                double mx=0,my=0;int ww=0,wh=0;
                glfwGetCursorPos(window,&mx,&my);glfwGetWindowSize(window,&ww,&wh);
                const auto hover=pointer.Hover(),pressed=pointer.Pressed();
                const bool down=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
                const bool valid=ww>0&&wh>0&&std::isfinite(mx)&&std::isfinite(my)&&mx>=0&&my>=0&&mx<ww&&my<wh;
                // Resizing during a press cancels that capture.
                uiAction=pointer.Update(*campaign.UI,page(),w,h,valid?int(mx*w/ww):-1,
                    valid?int(my*h/wh):-1,down,w==lastW&&h==lastH);
                const auto oldFocus=keyboard.Focus();
                const auto key=[&](int k){return glfwGetKey(window,k)==GLFW_PRESS;};
                const auto keyAction=keyboard.Update(*campaign.UI,page(),w,h,key(GLFW_KEY_TAB),
                    key(GLFW_KEY_LEFT_SHIFT)||key(GLFW_KEY_RIGHT_SHIFT),key(GLFW_KEY_ENTER)||key(GLFW_KEY_KP_ENTER),true);
                if(uiAction!=ui2d::Action::None&&pressed)signal({LogicEvent::UI,campaign.UI->Widgets.at(*pressed).ID});
                else if(keyAction!=ui2d::Action::None&&keyboard.Focus())signal({LogicEvent::UI,campaign.UI->Widgets.at(*keyboard.Focus()).ID});
                if(uiAction==ui2d::Action::None)uiAction=keyAction;
                if(uiAction!=ui2d::Action::None&&logic)++visualRevision;
                if(hover!=pointer.Hover()||pressed!=pointer.Pressed()||oldFocus!=keyboard.Focus())++visualRevision;
            }
            if(logic&&!paused&&!progress.Finished)
                for(const auto& definition:project.InputMap().Actions())
                    if(action(definition.Name).Pressed){signal({LogicEvent::Input,definition.Name});++visualRevision;}
            if(action("Quit").Pressed||uiAction==ui2d::Action::Quit){renderer.NativeWindow().RequestClose();break;}
            if(action("NewGame").Pressed||uiAction==ui2d::Action::NewGame)
            {
                progress={};game=Game(campaign.Worlds.front());paused=false;++visualRevision;
                checkpointWritable=true;updateTitle();accumulator=0;jumpQueued=false;clearEffects();
                if(logic)logic->NewCampaign();
                save();beginLogic();beginEffects();
            }
            if((action("Pause").Pressed||uiAction==ui2d::Action::Pause||uiAction==ui2d::Action::Resume)&&!progress.Finished){paused=!paused;++visualRevision;accumulator=0;jumpQueued=false;clearEffects();}
            if((action("Restart").Pressed||uiAction==ui2d::Action::Restart)&&!progress.Finished){game.Reset();++visualRevision;accumulator=0;jumpQueued=false;clearEffects();beginLogic();beginEffects();}
            if(w!=lastW||h!=lastH){renderer.ResizeViewport(w,h);lastW=w;lastH=h;++visualRevision;}
            if(jump.Pressed&&!paused)jumpQueued=true;
            accumulator+=dt;
            const auto movement=action("Move").Value;
            const auto sign=[](float x){return (x>.25f)-(x<-.25f);};
            while(accumulator>=1./60.)
            {
                if(!paused&&!progress.Finished)
                {
                    const auto before=game.Collected(),deaths=game.Deaths();
                    if(logic)logic->Tick(game);
                    game.Step({sign(movement.X),sign(movement.Y),jump.Down||jumpQueued});
                    ++visualRevision;
                    jumpQueued=false;
                    if(effects)effects->Step(activeOrigin);
                    if(game.Deaths()!=deaths){trigger(1);signal({LogicEvent::Death});beginLogic();burst(vfx2d::Event::Death);}
                    else if(game.Collected()>before){trigger(0);signal({LogicEvent::Collect});burst(vfx2d::Event::Collect);}
                    if(logic)logic->Sensors(game);
                    if(game.Won())
                    {
                        trigger(2);signal({LogicEvent::Complete});burst(vfx2d::Event::Complete);
                        progress=Advance(progress,levelCount);
                        save();updateTitle();
                        if(!progress.Finished){game=Game(campaign.Worlds.at(progress.Level));beginLogic();beginEffects();}
                        // New level begins on the next frame, never with a
                        // remainder of the previous level's input backlog.
                        accumulator=1./60.;
                    }
                }
                if(progress.Finished&&effects&&!effects->Particles().empty()){effects->Step(activeOrigin,false);++visualRevision;}
                if(audio&&audio->Failed())audio.reset();
                if(authoredAudio)
                {
                    authoredAudio->Update(game,campaign.Config.Levels.at(progress.Level),paused,progress.Finished);
                    const auto pcm=authoredAudio->Mix();
                    if(audio)(void)audio->Push(pcm);
                }
                else
                {
                    const auto pcm=mixer.Mix();
                    if(audio)(void)audio->Push(pcm);
                }
                accumulator-=1./60.;
            }
            if(publishedRevision!=visualRevision){publish(w,h);publishedRevision=visualRevision;}
            renderer.DrawFrame();
        }
        renderer.SetViewportOverlay({});
        return 0;
    }
}
