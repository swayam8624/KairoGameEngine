#pragma once
#include <charconv>
#include <cmath>
#include <cstdint>
#include <locale>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace kairo::player::detail
{
    // Optional per-owner tick cadence. Existing projects retain one VM tick
    // per physics step. Begin/input/contact dispatch is never throttled.
    inline std::map<std::uint32_t,unsigned> ParseLogicSchedule(std::string_view source)
    {
        if(source.size()>16384)throw std::invalid_argument("Logic schedule too large.");
        std::istringstream in{std::string(source)};in.imbue(std::locale::classic());
        std::string line;
        const auto read=[&]{if(!std::getline(in,line))return false;
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.find('\r')!=std::string::npos)throw std::invalid_argument("Invalid schedule line.");
            return true;};
        if(!read()||line!="kairo-logic-schedule 1")throw std::invalid_argument("Invalid logic schedule header.");
        std::map<std::uint32_t,unsigned> result;
        while(read())
        {
            if(line.empty())continue;
            std::istringstream row(line);row.imbue(std::locale::classic());
            std::string command,owner,interval,extra;
            if(!(row>>command>>owner>>interval)||command!="tick"||(row>>extra)||result.size()>=512)
                throw std::invalid_argument("Invalid logic schedule row or budget.");
            std::uint32_t id=0;unsigned steps=0;
            const auto a=std::from_chars(owner.data(),owner.data()+owner.size(),id);
            const auto b=std::from_chars(interval.data(),interval.data()+interval.size(),steps);
            if(a.ec!=std::errc{}||a.ptr!=owner.data()+owner.size()||!id||
                b.ec!=std::errc{}||b.ptr!=interval.data()+interval.size()||!steps||steps>60||
                !result.emplace(id,steps).second)
                throw std::invalid_argument("Invalid or duplicate logic schedule binding.");
        }
        return result;
    }

    class LogicLifecycle final
    {
        unsigned interval_,count_=0;
        double elapsed_=0;
        bool begun_=false;
    public:
        explicit LogicLifecycle(unsigned interval=1):interval_(interval)
        {if(!interval||interval>60)throw std::invalid_argument("Invalid logic interval.");}
        [[nodiscard]] bool NeedsBegin() const noexcept{return !begun_;}
        void MarkBegun() noexcept{begun_=true;}
        // Suspend drops partial accumulated time, never replays inactive time,
        // and preserves the VM's once-per-instance BeginPlay contract.
        void Suspend() noexcept{count_=0;elapsed_=0;}
        [[nodiscard]] std::optional<double> Tick(double dt)
        {
            if(!std::isfinite(dt)||dt<=0||dt>1)throw std::invalid_argument("Invalid logic tick delta.");
            elapsed_+=dt;
            if(++count_!=interval_)return {};
            const auto result=elapsed_;Suspend();return result;
        }
    };
}
