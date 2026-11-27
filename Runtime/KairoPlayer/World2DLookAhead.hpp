#pragma once
// Single-owner CPU preparation queue. Worker code must not touch the renderer,
// mutable game state or this queue. Its source must outlive the queue (the
// std::async future joins on destruction, including during exception unwind).
#include <chrono>
#include <cstddef>
#include <future>
#include <functional>
#include <system_error>
#include <utility>

namespace kairo::player::world2d
{
    template<class T> class LookAhead final
    {
        std::size_t key_=0;
        std::future<T> pending_;
    public:
        LookAhead()=default;
        LookAhead(const LookAhead&)=delete;
        LookAhead& operator=(const LookAhead&)=delete;

        // Never block or start another worker to replace an in-flight job.
        // A finished obsolete result can be discarded without observing its
        // exception; only the requested level's failure is authoritative.
        template<class Prepare> bool Request(std::size_t key,Prepare&& prepare)
        {
            if(pending_.valid()&&(key_==key||pending_.wait_for(std::chrono::seconds(0))!=std::future_status::ready))
                return false;
            try
            {
                auto next=std::async(std::launch::async,std::forward<Prepare>(prepare));
                pending_=std::move(next);
                key_=key;
                return true;
            }
            catch(const std::system_error&){return false;} // synchronous fallback at consumption
        }

        // The owner alone consumes. An early transition may wait for the
        // matching worker; unrelated results never block a synchronous miss.
        template<class Prepare> T Take(std::size_t key,Prepare&& prepare)
        {
            if(pending_.valid()&&key_==key)return pending_.get();
            return std::invoke(std::forward<Prepare>(prepare));
        }
    };
}
