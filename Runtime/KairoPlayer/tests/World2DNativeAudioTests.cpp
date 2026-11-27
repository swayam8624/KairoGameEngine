#include "../World2DCampaignProject.hpp"
#include <iostream>
#include <numeric>
import Kairo.Assets.WavImporter;
import Kairo.Assets.AudioArtifact;
import Kairo.Assets.Importer;
import Kairo.Assets.Types;
import Kairo.EngineCore.AudioRuntime;
#include "../World2DNativeAudio.hpp"

using namespace kairo::player;
void Assert(bool ok){if(!ok)throw std::runtime_error("Native World2D audio regression");}
template<class F>void Reject(F f){try{f();}catch(const std::exception&){return;}throw std::runtime_error("Invalid audio accepted");}
std::string Wave()
{
    std::string bytes="RIFF";
    const auto u16=[&](unsigned v){for(unsigned i=0;i<2;++i)bytes.push_back(static_cast<char>((v>>(8*i))&255));};
    const auto u32=[&](unsigned v){for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<char>((v>>(8*i))&255));};
    u32(36+240*4);bytes+="WAVEfmt ";u32(16);u16(1);u16(2);u32(24000);u32(96000);u16(4);u16(16);
    bytes+="data";u32(240*4);for(unsigned i=0;i<240;++i){u16(8192);u16(0xf000);}
    return bytes;
}
int main(int argc,char** argv)
{
    Assert(argc==2);
    const auto root=std::filesystem::canonical(argv[1]);
    const auto campaign=world2d::LoadCampaignProject(root);
    Assert(campaign.Audio.has_value());
    const auto wave=Wave();const auto clip=world2d::DecodeWorldWave(wave);
    Assert(clip->Channels==2&&clip->SampleRate==24000&&clip->FrameCount()==240&&clip->Samples[0]==.25f&&clip->Samples[1]==-.125f);
    Reject([&]{(void)world2d::DecodeWorldWave(wave.substr(0,wave.size()-1));});
    auto bad=*campaign.Audio;bad.Emitters.front().Actor=999999;
    Reject([&]{world2d::ValidateAudioBindings(campaign,bad);});
    bad=*campaign.Audio;bad.Clips.emplace("nonexistent",audio2d::Clip{true,0,0});
    Reject([&]{(void)world2d::LoadWorldAudio(root,bad);});
    world2d::Game game(campaign.Worlds.front());
    auto bank=world2d::LoadWorldAudio(root,*campaign.Audio);
    Assert(bank.size()==4);
    world2d::WorldAudio audio(*campaign.Audio,bank);
    audio.Update(game,"main",false,false);
    const auto ambient=audio.Mix();Assert(ambient.size()==1600&&audio.Voices()==1);
    double left=0,right=0;for(std::size_t i=0;i<ambient.size();i+=2){left+=ambient[i]*ambient[i];right+=ambient[i+1]*ambient[i+1];}
    Assert(left>0&&right>left);
    audio.Update(game,"main",true,false);
    const auto paused=audio.Mix();Assert(std::all_of(paused.begin(),paused.end(),[](float v){return v==0;}));
    audio.Update(game,"main",false,false);
    for(unsigned i=0;i<100;++i)audio.Trigger(0);
    Assert(audio.Voices()<=32&&audio.Stats().StolenVoices>0);
    for(unsigned i=0;i<60;++i)
    {
        const auto pcm=audio.Mix();
        Assert(std::all_of(pcm.begin(),pcm.end(),[](float v){return std::isfinite(v)&&std::abs(v)<=1;}));
    }
    Assert(audio.Voices()==1);
    audio.Update(game,"main",false,true);Assert(audio.Voices()==0);
    auto music=*campaign.Audio;music.Emitters.clear();music.Music={"track",100};music.Mix={100,100,100,100};
    bank.emplace("track",clip);world2d::WorldAudio loop(music,std::move(bank));
    for(unsigned i=0;i<100;++i)
    {
        loop.Update(game,"main",false,false);
        const auto pcm=loop.Mix();
        for(std::size_t j=0;j<pcm.size();j+=2)Assert(pcm[j]==.25f&&pcm[j+1]==-.125f);
    }
    Assert(loop.Voices()==1);
    loop.Update(game,"main",true,false);
    const auto silence=loop.Mix();Assert(std::all_of(silence.begin(),silence.end(),[](float v){return v==0;}));
    std::cout<<"Shared WAV decode, stereo resampling and music loop, spatial attenuation/pan, voice budget, focus/pause and validation fixtures passed\n";
}
