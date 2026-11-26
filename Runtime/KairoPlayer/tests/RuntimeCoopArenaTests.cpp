#include "RuntimeCoopArena.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
static void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static NetworkFrame Input(unsigned t,int x,int y){return {NetworkKind::Input,t,x,y};}
int main()
{
    CoopAuthority a;
    auto s=a.Apply({Input(1,0,1),Input(1,0,-1)});
    Check(s.Actors==std::array<CoopCell,2>{{{1,2},{1,4}}},"approach");
    s=a.Apply({Input(2,0,1),Input(2,0,-1)});
    Check(s.Actors==std::array<CoopCell,2>{{{1,2},{1,4}}},"contested cell");
    s=a.Apply({Input(3,0,1),Input(3,0,0)});
    s=a.Apply({Input(4,0,1),Input(4,0,-1)});
    Check(s.Actors==std::array<CoopCell,2>{{{1,3},{1,4}}},"swap rejected");
    auto prior=a.State();bool rejected=false;
    try{(void)a.Apply({Input(5,1,0),Input(6,0,0)});}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected&&a.State()==prior,"partial invalid turn");
    rejected=false;try{(void)a.Apply({Input(5,1,1),Input(5,0,0)});}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected&&a.State()==prior,"diagonal accepted");
    auto frames=EncodeCoopState(prior);frames[1].Tick++;
    rejected=false;try{(void)DecodeCoopState(frames,prior.Tick);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"mixed tick snapshot");
    CoopAuthority win;for(unsigned t=1;t<=6;++t)s=win.Apply({Input(t,1,0),Input(t,1,0)});
    Check(s.Won(),"co-op objective");
    rejected=false;try{(void)win.Apply({Input(7,0,0),Input(7,0,0)});}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected&&win.State()==s,"post-win input");
    constexpr std::array<CoopCell,5> moves{{{0,0},{1,0},{-1,0},{0,1},{0,-1}}};
    std::mt19937 rng(384);unsigned cases=0;
    for(unsigned trial=0;trial<400;++trial)
    {
        CoopAuthority authority;CoopState oracle;
        for(unsigned t=1;t<=250&&!oracle.Won();++t)
        {
            const auto d0=moves[rng()%5],d1=moves[rng()%5];
            // Independent oracle enumerates collision cases rather than
            // mutating one actor and checking the other against new state.
            auto p=oracle.Actors;std::array<CoopCell,2> targets{{{p[0].X+d0.X,p[0].Y+d0.Y},{p[1].X+d1.X,p[1].Y+d1.Y}}};
            for(unsigned i=0;i<2;++i){const auto c=targets[i];if(c.X<1||c.X>7||c.Y<1||c.Y>5||(c.X==4&&(c.Y==2||c.Y==4))||c==p[1-i])targets[i]=p[i];}
            if(targets[0]!=targets[1])oracle.Actors=targets;
            oracle.Tick=t;
            const auto actual=authority.Apply({Input(t,d0.X,d0.Y),Input(t,d1.X,d1.Y)});
            Check(actual==oracle&&DecodeCoopState(EncodeCoopState(actual),t)==actual,"authority/codec oracle mismatch");++cases;
        }
    }
    CoopAuthority limit;for(unsigned t=1;t<=CoopTickLimit;++t)(void)limit.Apply({Input(t,0,0),Input(t,0,0)});
    rejected=false;try{(void)limit.Apply({Input(CoopTickLimit+1,0,0),Input(CoopTickLimit+1,0,0)});}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"turn budget exceeded");
    std::cout<<cases<<" two-player authority oracle turns, collision, tick and terminal-state tests passed\n";
}
