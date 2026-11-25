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
    },4);
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
    // Hold four workers in the generator, then cancel only the four
    // remaining queued slots. This proves actual overlap without timing
    // assumptions and ensures cancellation cannot steal a running slot.
    std::atomic<unsigned> active=0;
    std::atomic<bool> proceed=false;
    Queue pool(8,[&](int coordinate){
        ++active;
        const auto until=std::chrono::steady_clock::now()+5s;
        while(!proceed)
        {
            if(std::chrono::steady_clock::now()>until)
                throw std::runtime_error("Parallel test gate timed out");
            std::this_thread::yield();
        }
        return std::vector<int>{coordinate};
    },4);
    for(int i=1;i<=8;++i)Check(pool.TrySubmit(i),"Pool initial submission failed");
    const auto overlapDeadline=std::chrono::steady_clock::now()+3s;
    while(active<4)
    {
        Check(std::chrono::steady_clock::now()<overlapDeadline,"Four workers did not overlap");
        std::this_thread::yield();
    }
    std::set<int> cancelled,parallelCompleted;
    while(auto cell=pool.CancelQueued([](int){return true;}))
        Check(cancelled.insert(*cell).second,"Duplicate cancellation");
    Check(cancelled.size()==4&&pool.Occupied()==4,"Cancellation stole running work");
    for(int i=9;i<=12;++i)Check(pool.TrySubmit(i),"Cancelled slot not reclaimed");
    proceed=true;
    const auto poolDeadline=std::chrono::steady_clock::now()+5s;
    while(parallelCompleted.size()<8)
    {
        Check(std::chrono::steady_clock::now()<poolDeadline,"Parallel completion stalled");
        if(auto done=pool.TryTake())
        {
            Check(!done->Failure&&done->Value&&done->Value->front()==done->Cell,
                "Parallel completion corrupted");
            Check(!cancelled.contains(done->Cell),"Cancelled job executed");
            Check(parallelCompleted.insert(done->Cell).second,"Parallel duplicate completion");
        }
        else std::this_thread::yield();
    }
    Check(active==8&&pool.Occupied()==0,"Parallel accounting differs");
    bool rejected=false;
    try{Queue invalid(2,[](int c){return std::vector<int>{c};},3);}
    catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Worker count exceeded capacity");
    std::cout<<"10000 four-worker background jobs, 1428 failures, capacity 8 and worker join passed\n"
        <<"4 simultaneous workers, 4 queued cancellations, 8 parallel completions passed\n";
}
