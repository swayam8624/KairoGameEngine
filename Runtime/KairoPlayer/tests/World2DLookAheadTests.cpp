#include "../World2DLookAhead.hpp"
#include <atomic>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

using kairo::player::world2d::LookAhead;
static void Check(bool ok){if(!ok)throw std::runtime_error("LookAhead regression");}
int main()
{
    const auto owner=std::this_thread::get_id();
    std::atomic<unsigned> calls=0;
    std::promise<void> release;
    auto gate=release.get_future().share();
    // Release the gated worker before checking any potentially failing assertion.
    LookAhead<unsigned> queue;
    const bool started=queue.Request(1,[&]{++calls;gate.wait();return std::this_thread::get_id()!=owner?42u:0u;});
    const bool duplicate=queue.Request(1,[]{return 99u;});
    const bool replacement=queue.Request(2,[]{return 99u;});
    const auto miss=queue.Take(2,[&]{return std::this_thread::get_id()==owner?7u:0u;});
    release.set_value();
    Check(started&&!duplicate&&!replacement&&miss==7);
    Check(queue.Take(1,[]{return 0u;})==42&&calls==1);
    Check(queue.Take(1,[]{return 12u;})==12);
    Check(queue.Request(3,[]()->unsigned{throw std::runtime_error("worker failure");}));
    bool caught=false;
    try{(void)queue.Take(3,[]{return 0u;});}catch(const std::runtime_error&){caught=true;}
    Check(caught&&queue.Take(3,[]{return 13u;})==13);
    for(unsigned i=0;i<128;++i)
    {
        Check(queue.Request(i,[i]{return i*i;}));
        Check(queue.Take(i,[]{return 999999u;})==i*i);
    }
    std::atomic<bool> completed=false;
    {LookAhead<unsigned> lifetime;Check(lifetime.Request(4,[&]{completed=true;return 4u;}));}
    Check(completed);
    std::cout<<"World2D look-ahead: 128 handoffs, duplicate/miss/error/lifetime fixtures passed\n";
}
