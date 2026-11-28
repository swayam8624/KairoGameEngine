#include "../World2DCampaign.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::world2d;
void Check(bool ok){if(!ok)throw std::runtime_error("Campaign regression");}
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Check(rejected);}
int main()
{
    const std::string source="kairo-campaign 1\ntitle \"TWO WORLDS\"\nlevels 2\nmain\nclimb\nmix 70 80\ncollect 660 90 70\ndeath 160 160 85\ncomplete 880 250 70\n";
    const auto config=ParseCampaign(source);
    Check(config.Levels.size()==2&&config.Levels[1]=="climb"&&config.Title=="TWO WORLDS");
    for(const std::string invalid:{"../escape","main","UPPER"})
    {
        auto bad=source;bad.replace(bad.find("climb"),5,invalid);
        Reject([&]{(void)ParseCampaign(bad);});
    }
    Reject([&]{(void)ParseCampaign(source+"trailing");});
    auto bad=source;bad.replace(bad.find("70 80"),5,"101 80");
    Reject([&]{(void)ParseCampaign(bad);});
    std::uint64_t revision=14695981039346656037ull;
    RevisionAppend(revision,source);RevisionAppend(revision,"level1");RevisionAppend(revision,"level2");
    auto changed=14695981039346656037ull;
    std::uint64_t other=changed;
    RevisionAppend(other,source);RevisionAppend(other,"level1");RevisionAppend(other,"level3");
    Check(revision!=other);
    auto p=Advance({},2);Check(p.Level==1&&!p.Finished);
    p=Advance(p,2);Check(p.Level==1&&p.Finished);
    Check(Advance(p,2).Finished);
    Reject([&]{(void)DecodeProgress(EncodeProgress(revision,p),other,2);});
    Reject([&]{(void)DecodeProgress(EncodeProgress(revision,{0,true}),revision,2);});
    Reject([&]{(void)DecodeProgress(EncodeProgress(revision,{2,false}),revision,2);});
    std::mt19937_64 rng(2026);
    for(int i=0;i<10000;++i)
    {
        const auto key=rng();const unsigned count=1+static_cast<unsigned>(rng()%16);
        const unsigned index=static_cast<unsigned>(rng()%count);
        Progress current{index,index+1==count&&bool(rng()%2)};
        if(i%2)for(auto& value:current.ScriptValues)value=static_cast<int>(rng()%2000001)-1000000;
        const auto restored=DecodeProgress(EncodeProgress(key,current),key,count);
        Check(restored.Level==current.Level&&restored.Finished==current.Finished&&restored.ScriptValues==current.ScriptValues);
        const auto next=Advance(restored,count);
        Check(next.Level<count&&(!next.Finished||next.Level+1==count)&&next.ScriptValues==current.ScriptValues);
        Reject([&]{(void)DecodeProgress(EncodeProgress(key,current),key^1u,count);});
    }
    Progress extreme{1,true};extreme.ScriptValues.fill(-1000000);
    const auto extended=EncodeProgress(revision,extreme);
    Check(extended.size()<=256&&extended.starts_with("kairo-world-progress 2\n"));
    Check(DecodeProgress(extended,revision,2).ScriptValues==extreme.ScriptValues);
    Check(EncodeProgress(revision,{}).starts_with("kairo-world-progress 1\n"));
    Reject([&]{(void)DecodeProgress(extended+"1",revision,2);});
    extreme.ScriptValues[0]=1000001;Reject([&]{(void)EncodeProgress(revision,extreme);});
    EventMixer mixer(config);
    for(float sample:mixer.Mix())Check(sample==0.f);
    mixer.Trigger(0);mixer.Trigger(1);mixer.Trigger(2);
    bool audible=false;
    for(unsigned block=0;block<16;++block)
    {
        const auto pcm=mixer.Mix();
        for(unsigned i=0;i<pcm.size();i+=2)
        {
            Check(std::isfinite(pcm[i])&&std::abs(pcm[i])<=1.f&&pcm[i]==pcm[i+1]);
            audible|=std::abs(pcm[i])>.01f;
        }
    }
    Check(audible);
    for(float sample:mixer.Mix())Check(sample==0.f);
    auto mute=config;mute.Master=0;EventMixer muted(mute);muted.Trigger(0);
    for(float sample:muted.Mix())Check(sample==0.f);
    mixer.Trigger(2);mixer.Clear();
    for(float sample:mixer.Mix())Check(sample==0.f);
    Reject([&]{mixer.Trigger(3);});
    std::cout<<"10000 checkpoint round trips, campaign transition and audio mix fixtures passed\n";
}
