#include "../RuntimePcmPump.hpp"
#include <deque>
#include <iostream>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    RuntimePcmQueue queue(64);RuntimePcmPump<16> pump(queue);
    std::deque<std::array<float,2>> pending,source;
    std::mt19937 rng(692);std::size_t written=0,zeros=0,partial=0,blocked=0;
    for(unsigned trial=0;trial<100000;++trial)
    {
        if(rng()%2)
        {
            std::vector<float> input((rng()%25)*2);
            for(auto& v:input)v=float(int(rng()%1999)-999)/1000.f;
            const auto expected=std::min(input.size()/2,64-source.size());
            Check(queue.Push(input)==expected,"Producer capacity differs");
            for(std::size_t i=0;i<expected;++i)source.push_back({input[2*i],input[2*i+1]});
        }
        if(pending.empty())for(unsigned i=0;i<16;++i)
        {
            if(source.empty()){pending.push_back({0,0});++zeros;}
            else{pending.push_back(source.front());source.pop_front();}
        }
        const auto offered=pending.size();
        const auto accept=std::min<std::size_t>(rng()%20,offered);
        const auto actual=pump.Step([&](std::span<const float> data){
            Check(data.size()==offered*2,"Pending period length differs");
            for(std::size_t i=0;i<offered;++i)Check(data[2*i]==pending[i][0]&&data[2*i+1]==pending[i][1],"Pending sample order differs");
            return accept;
        });
        Check(actual==accept,"Partial count differs");
        for(std::size_t i=0;i<accept;++i)pending.pop_front();
        written+=accept;if(!accept)++blocked;else if(accept<offered)++partial;
    }
    bool rejected=false;try{(void)pump.Step([](auto data){return data.size()/2+1;});}catch(const std::logic_error&){rejected=true;}
    Check(rejected&&partial&&blocked&&zeros,"Invalid device count or randomized coverage missing");
    std::cout<<"100000 pump transactions, "<<written<<" accepted frames, "<<partial<<" partial writes, "<<blocked<<" blocked writes, "<<zeros<<" silent frames passed\n";
}
