#include "RuntimeNativeAudioOutput.hpp"
#include "World2DCampaignProject.hpp"
#include <chrono>
#include <iostream>
#include <thread>

import Kairo.Assets.WavImporter;
import Kairo.Assets.AudioArtifact;
import Kairo.Assets.Importer;
import Kairo.Assets.Types;
import Kairo.EngineCore.AudioRuntime;

#include "World2DNativeAudio.hpp"

int main(int argc,char** argv)
{
    using namespace kairo::player;
    try
    {
        const std::string_view mode=argc>1?argv[1]:"";
        if(mode=="--wave"&&argc==3)
        {
            const auto clip=world2d::DecodeWorldWave(world2d::CampaignRead(argv[2],audio2d::MaxSourceBytes));
            std::cout<<clip->SampleRate<<" Hz, "<<clip->Channels<<" channels, "<<clip->DurationSeconds()<<" seconds\n";
            return 0;
        }
        if((mode=="--validate"&&argc==4)||(mode=="--preview"&&argc==5))
        {
            const auto root=std::filesystem::canonical(argv[2]);
            const auto candidate=world2d::CampaignRead(argv[3],audio2d::MaxBytes);
            const auto campaign=world2d::LoadCampaignProject(root,std::nullopt,candidate);
            const auto bank=world2d::LoadWorldAudio(root,*campaign.Audio);
            if(mode=="--validate"){std::cout<<"Validated "<<bank.size()<<" audio clips\n";return 0;}
            const auto clip=bank.at(argv[4]);
            kairo::engine::AudioMixer mixer(48000,1);
            mixer.SetBusState("master",{.65,false});
            (void)mixer.Play(clip);
            detail::RuntimeNativeAudioOutput output(48000);
            if(!output.Enabled())throw std::runtime_error("Native audio output unavailable.");
            const auto start=std::chrono::steady_clock::now();
            // At most three seconds, plus a short bounded device drain.
            for(unsigned i=0;i<180&&mixer.ActiveVoiceCount();++i)
            {
                if(output.Failed())throw std::runtime_error("Native preview output failed.");
                const auto pcm=mixer.Mix(800);
                if(output.Push(pcm)!=800)throw std::runtime_error("Native preview queue full.");
                std::this_thread::sleep_until(start+std::chrono::microseconds((i+1)*1000000ull/60));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if(output.Failed())throw std::runtime_error("Native preview output failed.");
            return 0;
        }
        throw std::invalid_argument("Usage: KairoWorld2DAudioTool --wave <wav> | --validate <project-root> <candidate> | --preview <project-root> <candidate> <clip>");
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
