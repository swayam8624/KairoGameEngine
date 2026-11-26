#include "RuntimeCoopSession.hpp"
#include <charconv>
#include <iostream>
#include <string_view>
using namespace kairo::player::detail;
static std::uint16_t Port(std::string_view value)
{
    unsigned port=0;const auto parsed=std::from_chars(value.data(),value.data()+value.size(),port);
    if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size()||!port||port>65535)throw std::invalid_argument("Invalid port.");
    return static_cast<std::uint16_t>(port);
}
static void Print(std::string_view label,CoopState s)
{std::cout<<label<<' '<<s.Tick<<' '<<s.Actors[0].X<<' '<<s.Actors[0].Y<<' '<<s.Actors[1].X<<' '<<s.Actors[1].Y<<' '<<s.Won()<<std::endl;}
int main(int argc,char** argv)
{
    try
    {
        NetworkSocketSession session;
        if(argc==2&&std::string_view(argv[1])=="server")
        {
            auto listener=LoopbackTcp::Listen();std::cout<<"READY "<<listener.GetPort()<<std::endl;
            Print("SERVER",RunCoopServer(listener));
        }
        else if(argc==3&&std::string_view(argv[1])=="client")
        {
            CoopClient client(Port(argv[2]));std::cout<<"SLOT "<<client.Slot()<<std::endl;
            for(unsigned i=0;i<6;++i)Print("TURN",client.Step(1,0));
            if(!client.State().Won())throw std::runtime_error("Co-op objective failed.");
        }
        else throw std::invalid_argument("Use server or client PORT.");
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
