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
    std::cout<<"700000 randomized key samples and focus resets passed; "<<accepted<<" actions accepted\n";
}
