#include "../RuntimeNetworkProtocol.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()
{
    const NetworkFrame known{NetworkKind::Snapshot,0x01020304,1,-2};
    const NetworkBytes expected{'K','N','E','T',1,2,0,0,1,2,3,4,0,0,0,1,255,255,255,254};
    Check(EncodeNetworkFrame(known)==expected&&DecodeNetworkFrame(expected)==known,"Byte order mismatch");
    std::mt19937 rng(2026);NetworkAuthority authority;int x=0,y=0;
    for(std::uint32_t tick=1;tick<=10000;++tick)
    {
        const int dx=int(rng()%3)-1,dy=int(rng()%3)-1;x+=dx;y+=dy;
        const NetworkFrame f{NetworkKind::Input,tick,dx,dy};
        const auto decoded=DecodeNetworkFrame(EncodeNetworkFrame(f));
        Check(authority.Apply(decoded)==NetworkFrame{NetworkKind::Snapshot,tick,x,y},"Authority mismatch");
        const auto state=authority.Snapshot();bool rejected=false;
        try{(void)authority.Apply(f);}catch(const std::invalid_argument&){rejected=true;}
        Check(rejected&&authority.Snapshot()==state,"Duplicate tick mutated state");
    }
    for(std::size_t i=0;i<expected.size();++i)
    {
        bool rejected=false;try{(void)DecodeNetworkFrame(std::span(expected).first(i));}catch(const std::invalid_argument&){rejected=true;}
        Check(rejected,"Truncated packet accepted");
    }
    auto bad=expected;bad[4]=2;bool rejected=false;
    try{(void)DecodeNetworkFrame(bad);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Unknown version accepted");
    rejected=false;try{(void)authority.Apply({NetworkKind::Input,10002,1,1});}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected&&authority.Snapshot().Tick==10000,"Future tick accepted");
    std::cout<<"10000 authoritative network ticks, byte order and malformed frame tests passed\n";
}
