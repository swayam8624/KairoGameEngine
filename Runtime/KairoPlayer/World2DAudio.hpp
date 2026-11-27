#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kairo::player::audio2d
{
    inline constexpr std::size_t MaxBytes=8192,MaxClips=32,MaxEmitters=16;
    inline constexpr std::size_t MaxSourceBytes=32*1024*1024,MaxBankBytes=64*1024*1024,MaxPCMBytes=128*1024*1024;
    inline void Check(bool ok,const char* message){if(!ok)throw std::invalid_argument(message);}
    inline bool Name(std::string_view s)
    {
        if(s.empty()||s.size()>32)return false;
        for(char c:s)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return false;
        return s!="con"&&s!="prn"&&s!="aux"&&s!="nul"&&
            !(s.size()==4&&(s.starts_with("com")||s.starts_with("lpt"))&&s[3]>='1'&&s[3]<='9');
    }
    struct Clip {bool Wave=false;unsigned Hz=0,Milliseconds=0;};
    struct Cue {std::string ClipID="-";unsigned Gain=100;};
    struct Emitter {std::string Level;unsigned Actor=0;Cue Sound;unsigned Radius=0;};
    struct Config
    {
        std::array<unsigned,4> Mix{70,80,50,50}; // master, effects, music, ambient
        std::map<std::string,Clip> Clips;
        std::array<Cue,3> Events;
        Cue Music;
        std::vector<Emitter> Emitters;
    };
    inline Config Parse(std::string_view bytes)
    {
        Check(!bytes.empty()&&bytes.size()<=MaxBytes,"Invalid audio document size.");
        std::istringstream in{std::string(bytes)};in.imbue(std::locale::classic());
        Config c;std::string tag;int version=0,count=0;
        Check(bool(in>>tag>>version)&&tag=="kairo-audio2d"&&version==1,"Unsupported audio document.");
        Check(bool(in>>tag)&&tag=="mix","Missing audio bus levels.");
        for(auto& gain:c.Mix)Check(bool(in>>gain)&&gain<=100,"Invalid audio bus gain.");
        Check(bool(in>>tag>>count)&&tag=="clips"&&count>=1&&count<=int(MaxClips),"Invalid sound bank count.");
        for(int i=0;i<count;++i)
        {
            std::string name,type;Clip clip;
            Check(bool(in>>name>>type)&&Name(name)&&(type=="wav"||type=="tone"),"Invalid sound bank entry.");
            clip.Wave=type=="wav";
            if(!clip.Wave)Check(bool(in>>clip.Hz>>clip.Milliseconds)&&clip.Hz>=40&&clip.Hz<=2000&&
                clip.Milliseconds>=10&&clip.Milliseconds<=10000,"Invalid original tone recipe.");
            Check(c.Clips.emplace(name,clip).second,"Duplicate sound clip.");
        }
        const auto cue=[&](Cue& sound)
        {
            Check(bool(in>>sound.ClipID>>sound.Gain)&&sound.Gain<=100&&
                (sound.ClipID=="-"||c.Clips.contains(sound.ClipID)),"Invalid audio cue.");
        };
        constexpr std::array<std::string_view,3> names{"collect","death","complete"};
        for(std::size_t i=0;i<names.size();++i)
        {
            Check(bool(in>>tag)&&tag==names[i],"Missing gameplay sound event.");
            cue(c.Events[i]);
        }
        Check(bool(in>>tag)&&tag=="music","Missing music cue.");cue(c.Music);
        Check(bool(in>>tag>>count)&&tag=="emitters"&&count>=0&&count<=int(MaxEmitters),"Invalid emitter count.");
        std::set<std::pair<std::string,unsigned>> seen;
        for(int i=0;i<count;++i)
        {
            Emitter e;long long id=0;
            Check(bool(in>>e.Level>>id)&&Name(e.Level)&&id>0&&id<=0xffffffffll,"Invalid emitter actor reference.");
            e.Actor=static_cast<unsigned>(id);cue(e.Sound);
            Check(e.Sound.ClipID!="-"&&bool(in>>e.Radius)&&e.Radius>=8&&e.Radius<=4096&&
                seen.emplace(e.Level,e.Actor).second,"Invalid or duplicate emitter.");
            c.Emitters.push_back(std::move(e));
        }
        Check(!(in>>tag),"Trailing audio document data.");
        return c;
    }
    // Original synthesized clips share the same PCM playback path as WAV.
    // A short envelope avoids clicks at the loop seam and at one-shot edges.
    inline std::vector<float> TonePCM(const Clip& clip)
    {
        Check(!clip.Wave&&clip.Hz>=40&&clip.Hz<=2000&&clip.Milliseconds>=10&&clip.Milliseconds<=10000,"Invalid tone.");
        std::vector<float> pcm(clip.Milliseconds*48u);
        for(std::size_t i=0;i<pcm.size();++i)
        {
            const double fade=std::min(1.,double(i)/96.)*std::min(1.,double(pcm.size()-1-i)/240.);
            pcm[i]=static_cast<float>(.6*fade*std::sin(6.283185307179586*clip.Hz*double(i)/48000.));
        }
        return pcm;
    }
}
