#include "RuntimeLoopbackTcp.hpp"
#include <fcntl.h>
#include <iostream>
#include <thread>
using namespace kairo::player::detail;
using namespace std::chrono_literals;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()
{
    // Duplicate a real listening socket above FD_SETSIZE. A POSIX fd_set
    // cannot represent it safely; poll handles its integer descriptor.
    const int raw=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    Check(raw>=0,"Socket creation");
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    Check(::bind(raw,reinterpret_cast<const sockaddr*>(&address),sizeof(address))==0&&::listen(raw,1)==0,"Bind/listen");
    const int high=::fcntl(raw,F_DUPFD,FD_SETSIZE+1);::close(raw);
    Check(high>FD_SETSIZE,"High descriptor fixture unavailable");
    LoopbackTcp listener(high);auto client=LoopbackTcp::Connect(listener.GetPort());auto peer=listener.Accept();
    const NetworkFrame frame{NetworkKind::Input,1,1,-1};client.Send(frame);Check(peer.Receive()==frame,"High descriptor frame exchange");
    // A socketpair supplies actual fragmented stream bytes. Three fragments
    // arrive within the per-call limit but collectively exceed the budget.
    int pair[2];Check(::socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0,"Socketpair");
    LoopbackTcp receiver(pair[0]);const auto bytes=EncodeNetworkFrame(frame);
    std::jthread sender([&]{for(unsigned i=0;i<3;++i){(void)::send(pair[1],bytes.data()+i,1,0);std::this_thread::sleep_for(100ms);}::close(pair[1]);});
    const auto start=std::chrono::steady_clock::now();bool rejected=false;
    try{(void)receiver.Receive(180ms);}catch(const std::runtime_error&){rejected=true;}
    Check(rejected&&std::chrono::steady_clock::now()-start<280ms,"Partial read extended whole-frame budget");
    bool invalid=false;try{(void)receiver.Receive(0ms);}catch(const std::invalid_argument&){invalid=true;}
    Check(invalid,"Zero timeout accepted");
    std::cout<<"High descriptor loopback and fragmented frame deadline passed\n";
}
