#pragma once
// Include after importing Kairo.Assets.WavImporter, Kairo.Assets.AudioArtifact,
// Kairo.Assets.Importer and Kairo.EngineCore.AudioRuntime.
#include "World2DCampaignProject.hpp"
#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>

namespace kairo::player::world2d
{
    using ClipBank=std::map<std::string,std::shared_ptr<const kairo::engine::AudioClip>>;
    inline std::shared_ptr<const kairo::engine::AudioClip> DecodeWorldWave(std::string_view bytes)
    {
        using namespace kairo::assets;
        audio2d::Check(bytes.size()<=audio2d::MaxSourceBytes,"WAV source exceeds budget.");
        ImportRequest request{};
        request.ExpectedType=AssetType::Audio;
        request.SourceBytes=std::as_bytes(std::span(bytes.data(),bytes.size()));
        auto artifact=ParseAudioDerivedArtifact(WavAudioImporter{}.Import(request));
        audio2d::Check(artifact.DurationSeconds()<=120.,"WAV exceeds 120 seconds.");
        auto clip=std::make_shared<kairo::engine::AudioClip>();
        clip->SampleRate=artifact.SampleRate;clip->Channels=artifact.Channels;clip->Samples=std::move(artifact.Samples);
        clip->Validate();return clip;
    }
    inline ClipBank LoadWorldAudio(const std::filesystem::path& root,const audio2d::Config& config)
    {
        ClipBank bank;std::size_t source=0,decoded=0;
        for(const auto& [name,recipe]:config.Clips)
        {
            std::shared_ptr<const kairo::engine::AudioClip> clip;
            if(recipe.Wave)
            {
                const auto path=CampaignPath(root,std::filesystem::path("Assets/Audio")/(name+".wav"));
                const auto bytes=CampaignRead(path,audio2d::MaxSourceBytes);
                source+=bytes.size();audio2d::Check(source<=audio2d::MaxBankBytes,"Sound bank source budget exceeded.");
                clip=DecodeWorldWave(bytes);
            }
            else
            {
                auto tone=std::make_shared<kairo::engine::AudioClip>();
                tone->Samples=audio2d::TonePCM(recipe);clip=std::move(tone);
            }
            decoded+=clip->Samples.size()*sizeof(float);
            audio2d::Check(decoded<=audio2d::MaxPCMBytes,"Sound bank decoded PCM budget exceeded.");
            bank.emplace(name,std::move(clip));
        }
        return bank;
    }

    // Existing EngineCore mixer owns resampling, looping, panning, distance
    // attenuation, clipping, buses and voice lifetime. This class owns only
    // World2D policy, not platform audio devices.
    class WorldAudio
    {
        using Handle=kairo::engine::AudioVoiceHandle;
        const audio2d::Config& m_Config;
        ClipBank m_Bank;
        kairo::engine::AudioMixer m_Mixer{48000,32};
        Handle m_Music{};
        std::array<Handle,audio2d::MaxEmitters> m_Emitters{};
        std::array<Handle,32> m_Effects{};
        std::size_t m_NextEffect=0;
        std::string m_Level;
        Handle Play(const audio2d::Cue& cue,const char* bus,bool loop,int priority,
            std::optional<kairo::engine::AudioVec3> position={},unsigned radius=0)
        {
            if(cue.ClipID=="-")return {};
            kairo::engine::AudioVoiceSettings settings;
            settings.Gain=double(cue.Gain)/100.;settings.Bus=bus;settings.Loop=loop;settings.Priority=priority;
            if(position)
            {
                settings.Spatial=true;settings.Position=*position;
                settings.Attenuation={kairo::engine::AudioDistanceModel::Linear,1.,double(radius),1.};
            }
            return m_Mixer.Play(m_Bank.at(cue.ClipID),settings);
        }
        static kairo::engine::AudioVec3 Position(Box b){return {b.X+b.W*.5,-b.Y-b.H*.5,0};}
        void StopEmitters()
        {
            for(auto& h:m_Emitters){m_Mixer.Stop(h);h={};}
        }
    public:
        WorldAudio(const audio2d::Config& config,ClipBank bank):m_Config(config),m_Bank(std::move(bank))
        {
            m_Mixer.SetBusState("master",{double(config.Mix[0])/100.,false});
            m_Mixer.DefineBus("effects",{double(config.Mix[1])/100.,false});
            m_Mixer.DefineBus("music",{double(config.Mix[2])/100.,false});
            m_Mixer.DefineBus("ambient",{double(config.Mix[3])/100.,false});
            m_Music=Play(config.Music,"music",true,100);
        }
        void ClearEffects()
        {
            for(auto& h:m_Effects){m_Mixer.Stop(h);h={};}
        }
        void Trigger(unsigned event)
        {
            auto& slot=m_Effects[m_NextEffect++%m_Effects.size()];
            m_Mixer.Stop(slot);slot=Play(m_Config.Events.at(event),"effects",false,10);
        }
        void Update(const Game& game,std::string_view level,bool paused,bool finished)
        {
            if(m_Level!=level){StopEmitters();m_Level=level;}
            m_Mixer.SetListener({Position(game.Player())});
            for(std::size_t i=0;i<m_Config.Emitters.size();++i)
            {
                const auto& e=m_Config.Emitters[i];auto& handle=m_Emitters[i];
                if(finished||e.Level!=level){m_Mixer.Stop(handle);handle={};continue;}
                const auto& authored=game.Definition().Actors;
                const auto it=std::find_if(authored.begin(),authored.end(),[&](const Actor& a){return a.ID==e.Actor;});
                if(it==authored.end())throw std::logic_error("Unvalidated audio actor.");
                const auto& actor=game.Actors().at(static_cast<std::size_t>(it-authored.begin()));
                if(!actor.Active){m_Mixer.Stop(handle);handle={};continue;}
                if(!m_Mixer.IsPlaying(handle))handle=Play(e.Sound,"ambient",true,50,Position(actor.Bounds),e.Radius);
                if(m_Mixer.IsPlaying(handle))
                {
                    m_Mixer.SetVoicePosition(handle,Position(actor.Bounds));
                    m_Mixer.SetPaused(handle,paused);
                }
            }
            if(m_Mixer.IsPlaying(m_Music))m_Mixer.SetPaused(m_Music,paused);
        }
        std::vector<float> Mix(){return m_Mixer.Mix(800);}
        std::size_t Voices() const noexcept{return m_Mixer.ActiveVoiceCount();}
        const kairo::engine::AudioMixStats& Stats() const noexcept{return m_Mixer.Stats();}
    };
}
