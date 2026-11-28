#include "../World2DAudio.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::audio2d;
void Assert(bool ok){if(!ok)throw std::runtime_error("Audio document regression");}
int main()
{
    const std::string source="kairo-audio2d 1\nmix 70 80 35 35\nclips 3\npickup tone 660 90\ntrack wav\nhum tone 220 1000\ncollect pickup 75\ndeath - 0\ncomplete pickup 80\nmusic track 50\nemitters 1\nmain 8 hum 35 96\n";
    auto c=Parse(source);
    Assert(c.Clips.size()==3&&c.Clips.at("track").Wave&&c.Music.ClipID=="track"&&c.Emitters[0].Actor==8&&c.Events[1].ClipID=="-");
    const auto reject=[](std::string_view s){try{(void)Parse(s);}catch(const std::invalid_argument&){return;}throw std::runtime_error("Accepted malformed audio document");};
    reject(source+"junk");reject(source.substr(0,source.size()-4));
    auto bad=source;bad.replace(bad.find("track wav"),9,"../track wav");reject(bad);
    bad=source;bad.replace(bad.find("track wav"),9,"con wav");reject(bad);
    bad=source;bad.replace(bad.find("music track"),11,"music ghost");reject(bad);
    bad=source;bad.replace(bad.find("emitters 1"),10,"emitters 2");reject(bad+"main 8 hum 35 96\n");
    bad=source;bad.replace(bad.find("70 80"),5,"101 80");reject(bad);
    bad=source;bad.replace(bad.find("660 90"),6,"2001 90");reject(bad);
    std::mt19937 rng(2026);
    for(unsigned i=0;i<10000;++i)
    {
        const unsigned hz=40+rng()%1961,ms=10+rng()%91,gain=rng()%101;
        auto bytes=source;bytes.replace(bytes.find("660 90"),6,std::to_string(hz)+" "+std::to_string(ms));
        bytes.replace(bytes.find("pickup 75"),9,"pickup "+std::to_string(gain));
        const auto parsed=Parse(bytes);const auto clip=parsed.Clips.at("pickup");
        Assert(clip.Hz==hz&&clip.Milliseconds==ms&&parsed.Events[0].Gain==gain);
        const auto pcm=TonePCM(clip);Assert(pcm.size()==ms*48u&&pcm.front()==0&&pcm.back()==0);
        double energy=0;for(float v:pcm){Assert(std::isfinite(v)&&std::abs(v)<=.600001f);energy+=v*v;}
        Assert(energy>0);
    }
    const auto longest=TonePCM({false,40,10000});Assert(longest.size()==480000);
    std::cout<<"10000 authored cue and PCM cases; malformed documents, max tone length and path rejection passed\n";
}
