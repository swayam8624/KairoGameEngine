#include <algorithm>
#include "../TerrainGenerationQueue.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <set>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;
using Queue=kairo::player::detail::TerrainGenerationQueue<int,std::vector<int>>;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    constexpr int count=10000;
    constexpr std::size_t capacity=8;
    std::atomic<bool> release=false;
    std::atomic<unsigned> started=0;
    const auto mainThread=std::this_thread::get_id();
    Queue queue(capacity,[&](int coordinate){
        Check(std::this_thread::get_id()!=mainThread,"Generator ran on consumer thread");
        ++started;
        const auto limit=std::chrono::steady_clock::now()+5s;
        while(!release.load())
        {
            if(std::chrono::steady_clock::now()>=limit)throw std::runtime_error("Test producer gate timed out");
            std::this_thread::yield();
        }
        if(coordinate%7==0)throw std::runtime_error("Injected generation failure");
        return std::vector<int>(64,coordinate);
    });
    for(int i=1;i<=static_cast<int>(capacity);++i)
        Check(queue.TrySubmit(i),"Initial slots rejected");
    Check(!queue.TrySubmit(99999)&&queue.Occupied()==capacity,"Queue exceeded capacity");
    release=true;
    int next=static_cast<int>(capacity)+1;
    std::set<int> completed;
    std::size_t failures=0,peak=0;
    const auto deadline=std::chrono::steady_clock::now()+30s;
    while(completed.size()<count)
    {
        Check(std::chrono::steady_clock::now()<deadline,"Queue stalled");
        while(next<=count&&queue.TrySubmit(next))++next;
        const auto occupied=queue.Occupied();
        peak=std::max(peak,occupied);
        Check(occupied<=capacity,"Pending bound violated");
        if(auto result=queue.TryTake())
        {
            Check(completed.insert(result->Cell).second,"Duplicate result");
            if(result->Cell%7==0)
            {
                Check(result->Failure&&!result->Value,"Failed job published data");
                ++failures;
            }
            else
            {
                Check(!result->Failure&&result->Value&&result->Value->size()==64&&
                    result->Value->front()==result->Cell,"Wrong completion payload");
            }
        }
        else std::this_thread::yield();
    }
    Check(queue.Occupied()==0&&started==count&&failures==1428&&peak<=capacity,
        "Completion accounting incorrect");
    // Destruction must join a running producer, even with pending slots.
    std::atomic<bool> entered=false,finished=false;
    {
        Queue closing(2,[&](int coordinate){
            entered=true;
            std::this_thread::sleep_for(10ms);
            finished=true;
            return std::vector<int>{coordinate};
        });
        Check(closing.TrySubmit(1),"Shutdown setup failed");
        const auto limit=std::chrono::steady_clock::now()+5s;
        while(!entered)
        {
            Check(std::chrono::steady_clock::now()<limit,"Worker did not start");
            std::this_thread::yield();
        }
    }
    Check(finished,"Worker outlived queue");
    std::cout<<"10000 background jobs, 1428 failures, capacity 8 and worker join passed\n";
}
