#include "RuntimeCoopWorker.hpp"
#include "RuntimeCoopView.hpp"
#include <iostream>
#include <memory>
#include <random>
#include <thread>
using namespace kairo::player::detail;
static void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static auto ConnectTwo(std::uint16_t port)
{
    auto a=std::async(std::launch::async,[=]{return std::make_unique<CoopWorker>(port);});
    auto b=std::make_unique<CoopWorker>(port);
    return std::array{a.get(),std::move(b)};
}
static void Await(CoopWorker& client)
{
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(!client.Poll())
    {
        if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Worker poll deadline");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
int main()
{
    NetworkSocketSession sockets;
    {
        auto listener=LoopbackTcp::Listen();auto server=std::async(std::launch::async,[&]{return RunCoopServer(listener);});
        auto clients=ConnectTwo(listener.GetPort());auto& a=*clients[0];auto& b=*clients[1];
        Check(a.Slot()!=b.Slot()&&a.Visible()==b.Visible(),"initial seats");
        auto start=CoopView(a.Visible(),a.Slot(),false,720,700);
        Check(start!=CoopView(a.Visible(),b.Slot(),false,720,700),"seat not visible");
        for(unsigned t=1;t<=6;++t)
        {
            Check(a.Submit(1,0)&&!a.Submit(1,0),"duplicate outstanding turn");
            Check(a.Visible().Tick==t-1&&!a.Poll()&&a.Busy(),"partial turn or blocking poll");
            Check(b.Submit(1,0),"second turn");Await(a);Await(b);
            Check(a.Visible()==b.Visible()&&a.Visible().Tick==t,"complete snapshot mismatch");
        }
        Check(a.Visible().Won()&&server.get()==a.Visible()&&!a.Submit(0,0),"server victory");
        Check(start!=CoopView(a.Visible(),a.Slot(),false,720,700),"victory image unchanged");
        std::mt19937 rng(21);
        for(unsigned i=0;i<5000;++i)
        {
            unsigned w=1+rng()%4000,h=1+rng()%3000;
            const auto view=CoopView(a.Visible(),i%2,i%3==0,w,h);
            Check(view.size()<=4096,"overlay budget");
            for(const auto& r:view)Check(r.Box.X>=0&&r.Box.Y>=0&&r.Box.W>0&&r.Box.H>0&&r.Box.X+r.Box.W<=int(w)&&r.Box.Y+r.Box.H<=int(h),"viewport bounds");
        }
    }
    {
        auto listener=LoopbackTcp::Listen();auto server=std::async(std::launch::async,[&]{return RunCoopServer(listener);});
        auto clients=ConnectTwo(listener.GetPort());Check(clients[0]->Submit(1,0),"cancel submission");
        const auto before=std::chrono::steady_clock::now();clients[0].reset();
        Check(std::chrono::steady_clock::now()-before<std::chrono::seconds(2),"blocked destructor");
        clients[1].reset();bool failed=false;try{(void)server.get();}catch(const std::runtime_error&){failed=true;}Check(failed,"server did not abort");
    }
    {
        auto listener=LoopbackTcp::Listen();auto server=std::async(std::launch::async,[&]{return RunCoopServer(listener);});
        auto clients=ConnectTwo(listener.GetPort());auto& a=*clients[0];Check(a.Submit(1,0),"failure submission");clients[1].reset();
        bool failed=false;try{Await(a);}catch(const std::runtime_error&){failed=true;}
        Check(failed&&a.Failed()&&!a.Busy()&&a.Visible()==CoopState{}&&!a.Submit(1,0),"failure latch/state preservation");
        failed=false;try{(void)server.get();}catch(const std::runtime_error&){failed=true;}Check(failed,"server did not abort");
    }
    std::cout<<"Async co-op turns, cancellation, failure latch and 5000 viewport cases passed\n";
}
