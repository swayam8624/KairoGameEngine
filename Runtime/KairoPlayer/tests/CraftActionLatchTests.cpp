#include "../CraftActionLatch.hpp"
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
int main()
{
    CraftActionInputs input;
    Check(!input.Eat.Press(true),"Held startup key fired");
    Check(!input.Eat.Press(false)&&input.Eat.Press(true)&&!input.Eat.Press(true),"Press/release contract");
    input.Reset();Check(!input.Eat.Press(true)&&!input.Eat.Press(true),"Held key fired after focus reset");
    Check(!input.Eat.Press(false)&&input.Eat.Press(true),"Rearmed key failed");
    Check(!input.Save.Press(true)&&!input.Save.Press(false)&&input.Save.Press(true),"Latches not independent");
    std::mt19937 rng(107);
    std::array<CraftActionLatch,7> latches{};
    std::array<unsigned,7> lastRelease{},lastPress{};
    unsigned lastReset=1,accepted=0;
    for(unsigned frame=2;frame<100002;++frame)
    {
        if(rng()%13==0){for(auto& l:latches)l.Reset();lastReset=frame;}
        for(unsigned k=0;k<latches.size();++k)
        {
            const bool down=rng()%2;
            const bool expected=down&&lastRelease[k]>=lastReset&&lastRelease[k]>lastPress[k];
            const bool pressed=latches[k].Press(down);
            Check(pressed==expected,"Input edge differs from timestamp oracle");
            if(!down)lastRelease[k]=frame;
            if(pressed){lastPress[k]=frame;++accepted;}
        }
    }
    for(unsigned mask=0;mask<16;++mask)
    {
        const bool focus=(mask&1)!=0,minimized=(mask&2)!=0;
        const auto width=(mask&4)?960u:0u,height=(mask&8)?540u:0u;
        Check(CraftFrameActive(focus,minimized,width,height)==(mask==13),"Frame gate truth table differs");
    }
    CraftActionLatch action;bool active=false,armed=false;
    unsigned inactiveFrames=0,resumeFrames=0,frameActions=0;
    for(unsigned frame=0;frame<100000;++frame)
    {
        const unsigned mask=rng()%16;
        const bool now=CraftFrameActive(mask&1,mask&2,(mask&4)?960u:0u,(mask&8)?540u:0u);
        if(now!=active){action.Reset();armed=false;if(now)++resumeFrames;active=now;}
        const bool down=rng()%2;
        if(!active){++inactiveFrames;continue;}
        const bool expected=armed&&down;
        Check(action.Press(down)==expected,"Frame transition synthesized or lost input");
        armed=!down;if(expected)++frameActions;
    }
    Check(inactiveFrames>0&&resumeFrames>0&&frameActions>0,"Frame gate regression lacks coverage");
    std::cout<<"700000 randomized key samples and focus resets passed; "<<accepted<<" actions accepted\n";
    std::cout<<"100000 frame transitions; "<<inactiveFrames<<" inactive frames, "<<resumeFrames<<" resumes and "<<frameActions<<" valid actions\n";
}
