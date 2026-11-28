#pragma once
#include <algorithm>
#include <array>
#include <utility>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace kairo::player::world2d
{
    // Campaign data is separate from geometry. Stable level names resolve
    // only under Config/Worlds; the first level is always Config/World.k2d.
    struct Tone {unsigned Hz=0,Milliseconds=0,Gain=0;};
    struct Campaign
    {
        std::vector<std::string> Levels{"main"};
        std::string Title="KAIRO ADVENTURE";
        unsigned Master=70,Effects=80;
        std::array<Tone,3> Sounds{{{660,90,70},{160,160,85},{880,250,70}}};
    };
    inline bool CampaignName(std::string_view name)
    {
        if(name.empty()||name.size()>32)return false;
        for(char c:name)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return false;
        return true;
    }
    inline void CampaignCheck(bool ok,const char* message)
    {if(!ok)throw std::invalid_argument(message);}
    inline Campaign ParseCampaign(std::string_view bytes)
    {
        CampaignCheck(!bytes.empty()&&bytes.size()<=8192,"Invalid campaign byte count.");
        std::istringstream in{std::string(bytes)};in.imbue(std::locale::classic());
        Campaign c;std::string tag;int version=0,count=0;
        CampaignCheck(bool(in>>tag>>version)&&tag=="kairo-campaign"&&version==1,"Invalid campaign version.");
        CampaignCheck(bool(in>>tag>>std::quoted(c.Title))&&tag=="title"&&!c.Title.empty()&&c.Title.size()<=40,"Invalid campaign title.");
        for(char ch:c.Title)CampaignCheck(ch>=32&&ch<=126,"Campaign title must be printable ASCII.");
        CampaignCheck(bool(in>>tag>>count)&&tag=="levels"&&count>=1&&count<=16,"Invalid level count.");
        c.Levels.clear();
        for(int i=0;i<count;++i)
        {
            std::string name;CampaignCheck(bool(in>>name)&&CampaignName(name),"Invalid level name.");
            for(const auto& prior:c.Levels)CampaignCheck(prior!=name,"Duplicate level name.");
            c.Levels.push_back(name);
        }
        CampaignCheck(c.Levels.front()=="main","Campaign must start at main.");
        CampaignCheck(bool(in>>tag>>c.Master>>c.Effects)&&tag=="mix"&&c.Master<=100&&c.Effects<=100,"Invalid mixer levels.");
        for(unsigned i=0;i<c.Sounds.size();++i)
        {
            static constexpr std::array<std::string_view,3> names{"collect","death","complete"};
            auto& tone=c.Sounds[i];
            CampaignCheck(bool(in>>tag>>tone.Hz>>tone.Milliseconds>>tone.Gain)&&tag==names[i]&&
                tone.Hz>=40&&tone.Hz<=2000&&tone.Milliseconds>=10&&tone.Milliseconds<=1000&&tone.Gain<=100,"Invalid event sound.");
        }
        CampaignCheck(!(in>>tag),"Trailing campaign data.");
        return c;
    }
    // Noncryptographic revision tag: ordered length-delimited exact source
    // bytes. Prevents accidental use of a checkpoint after authored edits.
    inline void RevisionAppend(std::uint64_t& hash,std::string_view bytes)
    {
        auto add=[&](unsigned char b){hash^=b;hash*=1099511628211ull;};
        auto size=static_cast<std::uint64_t>(bytes.size());
        for(unsigned i=0;i<8;++i){add(static_cast<unsigned char>(size&255));size>>=8;}
        for(unsigned char b:bytes)add(b);
    }
    struct Progress
    {
        unsigned Level=0;bool Finished=false;
        std::array<int,16> ScriptValues{};
    };
    inline std::string EncodeProgress(std::uint64_t revision,Progress progress)
    {
        std::ostringstream out;out.imbue(std::locale::classic());
        const bool extended=std::any_of(progress.ScriptValues.begin(),progress.ScriptValues.end(),[](int v){return v!=0;});
        out<<"kairo-world-progress "<<(extended?2:1)<<'\n'<<revision<<' '<<progress.Level<<' '<<int(progress.Finished)<<'\n';
        if(extended)
        {
            for(int v:progress.ScriptValues)
            {
                CampaignCheck(v>=-1000000&&v<=1000000,"Invalid checkpoint script value.");
                out<<v<<' ';
            }
            out<<'\n';
        }
        return out.str();
    }
    inline Progress DecodeProgress(std::string_view bytes,std::uint64_t revision,unsigned levelCount)
    {
        CampaignCheck(bytes.size()<=256&&levelCount>=1&&levelCount<=16,"Invalid checkpoint bounds.");
        std::istringstream in{std::string(bytes)};in.imbue(std::locale::classic());
        std::string magic,extra;int version=0,finished=0;std::uint64_t key=0;Progress p;
        CampaignCheck(bool(in>>magic>>version>>key>>p.Level>>finished)&&magic=="kairo-world-progress"&&(version==1||version==2)&&
            key==revision&&p.Level<levelCount&&(finished==0||finished==1),"Invalid or stale campaign checkpoint.");
        if(version==2)
            for(auto& v:p.ScriptValues)
                CampaignCheck(bool(in>>v)&&v>=-1000000&&v<=1000000,"Invalid checkpoint script values.");
        CampaignCheck(!(in>>extra),"Trailing checkpoint data.");
        p.Finished=finished!=0;
        CampaignCheck(!p.Finished||p.Level+1==levelCount,"Invalid finished checkpoint.");
        return p;
    }
    inline Progress Advance(Progress current,unsigned levelCount)
    {
        CampaignCheck(levelCount>=1&&levelCount<=16&&current.Level<levelCount&&
            (!current.Finished||current.Level+1==levelCount),"Invalid campaign state.");
        if(!current.Finished)
        {
            if(current.Level+1==levelCount)current.Finished=true;
            else ++current.Level;
        }
        return current;
    }
    // A tiny bounded event synthesizer. A fixed 800-frame block per 60 Hz
    // simulation tick at 48 kHz; three independent retriggerable voices.
    class EventMixer
    {
        struct Voice {unsigned Position=0,Length=0;};
        Campaign m_Config;
        std::array<Voice,3> m_Voices{};
    public:
        explicit EventMixer(Campaign config):m_Config(std::move(config)){}
        void Trigger(unsigned event)
        {
            if(event>=m_Voices.size())throw std::invalid_argument("Invalid sound event.");
            m_Voices[event]={0,m_Config.Sounds[event].Milliseconds*48};
        }
        void Clear() noexcept{m_Voices={};}
        std::array<float,1600> Mix()
        {
            std::array<float,1600> pcm{};
            for(unsigned frame=0;frame<800;++frame)
            {
                double sum=0;
                for(unsigned i=0;i<m_Voices.size();++i)
                {
                    auto& v=m_Voices[i];const auto& t=m_Config.Sounds[i];
                    if(v.Position>=v.Length)continue;
                    const double envelope=std::min(1.,double(v.Position)/96.)*
                        std::min(1.,double(v.Length-v.Position)/240.);
                    sum+=std::sin(6.283185307179586*double(t.Hz)*v.Position/48000.)*
                        envelope*double(t.Gain)*m_Config.Master*m_Config.Effects/3000000.;
                    ++v.Position;
                }
                pcm[frame*2]=pcm[frame*2+1]=static_cast<float>(sum);
            }
            return pcm;
        }
    };
}
