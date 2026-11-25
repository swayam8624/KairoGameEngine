#include "../RuntimePcmQueue.hpp"
#include <deque>
#include <iostream>
#include <limits>
#include <random>
#include <thread>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    RuntimePcmQueue queue(64);std::deque<std::array<float,2>> oracle;
    std::mt19937 rng(728);std::size_t total=0,overflow=0,underflow=0;
    for(unsigned t=0;t<100000;++t)
    {
        const auto n=rng()%100;
        if(rng()%2)
        {
            std::vector<float> samples(n*2);
            for(auto& v:samples)v=float(int(rng()%401)-200)/100.f;
            const auto expected=std::min<std::size_t>(n,64-oracle.size());
            Check(queue.Push(samples)==expected,"Queue overflow differs");
            for(std::size_t i=0;i<expected;++i)oracle.push_back({std::clamp(samples[2*i],-1.f,1.f),std::clamp(samples[2*i+1],-1.f,1.f)});
            overflow+=n-expected;
        }
        else
        {
            std::vector<float> samples(n*2,99);
            const auto expected=std::min<std::size_t>(n,oracle.size());
            Check(queue.Pop(samples)==expected,"Queue underflow differs");
            for(std::size_t i=0;i<expected;++i){const auto v=oracle.front();oracle.pop_front();Check(samples[2*i]==v[0]&&samples[2*i+1]==v[1],"PCM frame order differs");}
            for(std::size_t i=2*expected;i<samples.size();++i)Check(samples[i]==0,"Underflow not silent");
            underflow+=n-expected;total+=expected;
        }
    }
    RuntimePcmQueue bad(2);const std::array invalid{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()};
    std::array<float,3> silence{1,1,1};Check(bad.Push(invalid)==1&&bad.Pop(silence)==1&&silence==std::array{0.f,0.f,0.f},"Nonfinite or incomplete output differs");
    bool rejected=false;try{RuntimePcmQueue wrong(3);}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"Invalid capacity accepted");
    rejected=false;try{(void)bad.Push(std::span(silence).first(1));}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"Incomplete input accepted");
    RuntimePcmQueue threaded(256);constexpr unsigned count=1000000;
    std::atomic<bool> mismatch=false;
    std::thread producer([&]{for(unsigned i=0;i<count;){const float v=float(i%1000)/1000.f;const std::array stereo{v,-v};if(threaded.Push(stereo))++i;else std::this_thread::yield();}});
    std::thread consumer([&]{for(unsigned i=0;i<count;){std::array<float,2> stereo{};if(threaded.Pop(stereo)){const float v=float(i%1000)/1000.f;if(stereo!=std::array{v,-v})mismatch=true;++i;}else std::this_thread::yield();}});
    producer.join();consumer.join();Check(!mismatch,"Concurrent frame order differs");
    std::cout<<"100000 randomized transactions, "<<total<<" oracle frames, "<<overflow<<" dropped, "<<underflow<<" silent, 1000000 concurrent stereo frames passed\n";
}
