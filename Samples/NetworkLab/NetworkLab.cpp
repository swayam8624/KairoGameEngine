#include "../../Runtime/KairoPlayer/RuntimeLoopbackTcp.hpp"
#include <iostream>
#include <string>
using namespace kairo::player::detail;
// Separate server and client processes exchange ten authoritative ticks.
// Client sends only input; server sends the resulting snapshot after each tick.
int main(int argc,char** argv)
{
    try
    {
        NetworkSocketSession session;
        if(argc==2&&std::string(argv[1])=="server")
        {
            auto listener=LoopbackTcp::Listen();std::cout<<"READY "<<listener.GetPort()<<std::endl;
            auto peer=listener.Accept();NetworkAuthority authority;
            for(unsigned i=0;i<10;++i)peer.Send(authority.Apply(peer.Receive()));
            const auto state=authority.Snapshot();std::cout<<"SERVER "<<state.Tick<<' '<<state.X<<' '<<state.Y<<'\n';
        }
        else if(argc==3&&std::string(argv[1])=="client")
        {
            const auto port=std::stoul(argv[2]);if(!port||port>65535)throw std::invalid_argument("Invalid port.");
            auto peer=LoopbackTcp::Connect(static_cast<std::uint16_t>(port));
            NetworkFrame snapshot{NetworkKind::Snapshot,0,0,0};
            for(std::uint32_t tick=1;tick<=10;++tick)
            {
                peer.Send({NetworkKind::Input,tick,1,tick%2?1:-1});snapshot=peer.Receive();
                if(snapshot!=NetworkFrame{NetworkKind::Snapshot,tick,static_cast<std::int32_t>(tick),tick%2?1:0})
                    throw std::runtime_error("Authoritative snapshot mismatch.");
            }
            std::cout<<"CLIENT "<<snapshot.Tick<<' '<<snapshot.X<<' '<<snapshot.Y<<'\n';
        }
        else throw std::invalid_argument("Use server or client PORT.");
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
