#pragma once
#include "RuntimeWorld2D.hpp"
#include "World2DCampaign.hpp"
#include "CraftSaveFile.hpp"
#include "World2DUI.hpp"
#include "World2DAudio.hpp"
#include "World2DLogic.hpp"
#include "World2DVFX.hpp"
#include <optional>

namespace kairo::player::world2d
{
    // All authored campaign paths are generated from validated level names.
    // Reject every linked component, including links within the project.
    inline std::filesystem::path CampaignPath(const std::filesystem::path& root,
        const std::filesystem::path& relative)
    {
        auto path=root;
        for(const auto& part:relative)
        {
            CampaignCheck(part!=".."&&part!="."&&!part.has_root_path(),"Invalid campaign path.");
            path/=part;
            CampaignCheck(!std::filesystem::is_symlink(path),"Linked campaign path rejected.");
        }
        return path;
    }
    inline std::string CampaignRead(const std::filesystem::path& path,std::size_t limit)
    {
        const auto bytes=kairo::player::detail::ReadCraftSaveFile(path,limit);
        return {reinterpret_cast<const char*>(bytes.data()),bytes.size()};
    }
    struct CampaignProject
    {
        Campaign Config;
        std::vector<World> Worlds;
        std::optional<ui2d::Document> UI;
        std::optional<audio2d::Config> Audio;
        std::optional<logic2d::Document> Logic;
        std::optional<vfx2d::Config> VFX;
        std::uint64_t Revision=14695981039346656037ull;
    };
    inline void ValidateAudioBindings(const CampaignProject& project,const audio2d::Config& config)
    {
        for(const auto& emitter:config.Emitters)
        {
            const auto level=std::find(project.Config.Levels.begin(),project.Config.Levels.end(),emitter.Level);
            CampaignCheck(level!=project.Config.Levels.end(),"Audio emitter references missing level.");
            const auto& actors=project.Worlds.at(static_cast<std::size_t>(level-project.Config.Levels.begin())).Actors;
            CampaignCheck(std::any_of(actors.begin(),actors.end(),[&](const Actor& actor){return actor.ID==emitter.Actor;}),
                "Audio emitter references missing actor.");
        }
    }
    inline void ValidateLogicBindings(const CampaignProject& project,const logic2d::Document& doc)
    {
        using namespace logic2d;
        for(const auto& r:doc.Rules)
        {
            const auto level=std::find(project.Config.Levels.begin(),project.Config.Levels.end(),r.Level);
            CampaignCheck(level!=project.Config.Levels.end(),"Logic references missing level.");
            const auto& world=project.Worlds.at(static_cast<std::size_t>(level-project.Config.Levels.begin()));
            const auto body=[&](unsigned id)->const BodyRecord*
            {
                if(world.Physics)for(const auto& b:world.Physics->Bodies)if(b.Actor==id)return &b;
                return nullptr;
            };
            if(r.When==Event::SensorBegin||r.When==Event::SensorEnd)
            {
                const auto* b=body(r.Selector);
                CampaignCheck(b&&b->Body.Sensor,"Logic requires an authored sensor actor.");
            }
            if(r.When==Event::UI)
                CampaignCheck(project.UI&&std::any_of(project.UI->Widgets.begin(),project.UI->Widgets.end(),
                    [&](const ui2d::Widget& w){return w.Type==ui2d::Kind::Button&&w.ID==r.Argument;}),
                    "Logic references missing UI button.");
            if(r.Command==Op::Position||r.Command==Op::Impulse)
            {
                const auto actor=std::find_if(world.Actors.begin(),world.Actors.end(),[&](const Actor& a){return a.ID==r.Actor;});
                CampaignCheck(actor!=world.Actors.end(),"Logic references missing actor.");
                const auto* b=body(r.Actor);
                if(r.Command==Op::Impulse)CampaignCheck(b&&b->Body.Type==p2::Motion::Dynamic,"Logic impulse requires dynamic actor.");
                else CampaignCheck(actor->Type==Kind::Decor&&!b&&r.X>=0&&r.Y>=0&&
                    std::lround(r.X)+actor->Bounds.W<=world.Width*Tile&&std::lround(r.Y)+actor->Bounds.H<=world.Height*Tile,
                    "Logic position requires an unbound in-bounds decor actor.");
            }
        }
    }
    inline void ValidateVFXBindings(const CampaignProject& project,const vfx2d::Config& config)
    {
        for(const auto& emitter:config.Emitters)
        {
            const auto level=std::find(project.Config.Levels.begin(),project.Config.Levels.end(),emitter.Level);
            CampaignCheck(level!=project.Config.Levels.end(),"VFX emitter references missing level.");
            const auto& world=project.Worlds.at(static_cast<std::size_t>(level-project.Config.Levels.begin()));
            CampaignCheck(std::any_of(world.Actors.begin(),world.Actors.end(),
                [&](const Actor& a){return a.ID==emitter.Actor;}),"VFX emitter references missing actor.");
        }
    }
    inline CampaignProject LoadCampaignProject(const std::filesystem::path& root,
        std::optional<std::string_view> candidate=std::nullopt,
        std::optional<std::string_view> audioCandidate=std::nullopt,
        std::optional<std::string_view> logicCandidate=std::nullopt,
        std::optional<std::string_view> vfxCandidate=std::nullopt)
    {
        CampaignProject result;
        const auto config=CampaignPath(root,"Config/Campaign.kcampaign");
        if(candidate)
        {
            result.Config=ParseCampaign(*candidate);RevisionAppend(result.Revision,*candidate);
        }
        else if(std::filesystem::exists(config))
        {
            const auto bytes=CampaignRead(config,8192);
            result.Config=ParseCampaign(bytes);RevisionAppend(result.Revision,bytes);
        }
        else RevisionAppend(result.Revision,"default-campaign-v1");
        for(const auto& name:result.Config.Levels)
        {
            const auto relative=name=="main"?std::filesystem::path("Config/World.k2d"):
                std::filesystem::path("Config/Worlds")/(name+".k2d");
            const auto bytes=CampaignRead(CampaignPath(root,relative),MaxBytes);
            RevisionAppend(result.Revision,bytes);
            result.Worlds.push_back(Parse(bytes));
        }
        const auto ui=CampaignPath(root,"Config/UI.kui");
        if(std::filesystem::exists(ui))result.UI=ui2d::Parse(CampaignRead(ui,ui2d::MaxBytes));
        const auto audio=CampaignPath(root,"Config/Audio.kaudio");
        if(audioCandidate||std::filesystem::exists(audio))
        {
            result.Audio=audio2d::Parse(audioCandidate?*audioCandidate:CampaignRead(audio,audio2d::MaxBytes));
            ValidateAudioBindings(result,*result.Audio);
        }
        const auto logic=CampaignPath(root,"Config/Logic.klogic2d");
        if(logicCandidate||std::filesystem::exists(logic))
        {
            const auto bytes=logicCandidate?std::string(*logicCandidate):CampaignRead(logic,logic2d::MaxBytes);
            result.Logic=logic2d::Parse(bytes);ValidateLogicBindings(result,*result.Logic);
            RevisionAppend(result.Revision,bytes);
        }
        const auto vfx=CampaignPath(root,"Config/VFX.kvfx");
        if(vfxCandidate||std::filesystem::exists(vfx))
        {
            result.VFX=vfx2d::Parse(vfxCandidate?*vfxCandidate:CampaignRead(vfx,vfx2d::MaxBytes));
            ValidateVFXBindings(result,*result.VFX);
        }
        // Cosmetic UI/audio/VFX edits deliberately do not invalidate gameplay checkpoints.
        return result;
    }
    inline std::filesystem::path CheckpointPath(const std::filesystem::path& root)
    {return CampaignPath(root,".kairo/progress/world2d.kprogress");}
    inline Progress ReadCheckpoint(const std::filesystem::path& root,const CampaignProject& project)
    {
        const auto path=CheckpointPath(root);
        if(!std::filesystem::exists(path))return {};
        return DecodeProgress(CampaignRead(path,256),project.Revision,static_cast<unsigned>(project.Worlds.size()));
    }
    inline void WriteCheckpoint(const std::filesystem::path& root,const CampaignProject& project,Progress progress)
    {
        const auto path=CheckpointPath(root);
        const auto bytes=EncodeProgress(project.Revision,progress);
        (void)DecodeProgress(bytes,project.Revision,static_cast<unsigned>(project.Worlds.size()));
        std::filesystem::create_directories(path.parent_path());
        kairo::player::detail::WriteCraftSaveFile(path,
            {reinterpret_cast<const std::uint8_t*>(bytes.data()),bytes.size()},256);
    }
}
