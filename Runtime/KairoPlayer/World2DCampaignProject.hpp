#pragma once
#include "RuntimeWorld2D.hpp"
#include "World2DCampaign.hpp"
#include "CraftSaveFile.hpp"
#include "World2DUI.hpp"
#include "World2DAudio.hpp"
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
    inline CampaignProject LoadCampaignProject(const std::filesystem::path& root,
        std::optional<std::string_view> candidate=std::nullopt,
        std::optional<std::string_view> audioCandidate=std::nullopt)
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
        // Cosmetic UI/audio edits deliberately do not invalidate gameplay checkpoints.
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
