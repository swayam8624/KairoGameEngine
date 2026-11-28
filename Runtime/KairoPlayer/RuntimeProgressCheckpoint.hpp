#pragma once
#include "CraftSaveFile.hpp"
#include "RuntimeCollectibleRun.hpp"
#include "RuntimeProximityTriggers.hpp"
#include <iomanip>
#include <map>
#include <optional>
#include <utility>

namespace kairo::player::detail
{
    inline constexpr std::size_t MaximumProgressBytes=256*1024;
    using TriggerVisuals=std::map<std::uint32_t,bool>;

    // Bind a checkpoint to authored source bytes, not just numeric entity IDs.
    // FNV-1a detects accidental version mismatches; it is not authentication.
    inline std::uint64_t ProgressIdentity(const std::vector<std::filesystem::path>& sources)
    {
        std::uint64_t hash=14695981039346656037ull;
        auto mix=[&](std::uint8_t byte){hash=(hash^byte)*1099511628211ull;};
        for(const auto& path:sources)
        {
            const auto bytes=ReadCraftSaveFile(path,64*1024*1024);
            const auto size=static_cast<std::uint64_t>(bytes.size());
            for(unsigned i=0;i<8;++i)mix(static_cast<std::uint8_t>(size>>(i*8)));
            for(auto byte:bytes)mix(byte);
        }
        return hash;
    }
    inline void SaveProgress(const std::filesystem::path& path,std::uint64_t identity,
        const RuntimeCollectibleRun& goals,const std::optional<RuntimeProximityTriggers>& triggers,
        const TriggerVisuals& visuals)
    {
        std::vector<std::uint32_t> collected;
        for(const auto& goal:goals.Goals())if(goal.Collected)collected.push_back(goal.ID);
        std::sort(collected.begin(),collected.end());
        const auto states=triggers?triggers->Progress():std::vector<TriggerProgress>{};
        if(visuals.size()>MaximumTriggers)throw std::invalid_argument("Too many saved visuals.");
        std::ostringstream out;out.imbue(std::locale::classic());
        out<<"kairo-progress 1\n"<<identity<<' '<<std::setprecision(17)<<goals.Elapsed()<<'\n';
        out<<collected.size()<<'\n';for(auto id:collected)out<<id<<'\n';
        out<<states.size()<<'\n';for(const auto& s:states)out<<s.ID<<' '<<s.Inside<<' '<<s.Fired<<'\n';
        out<<visuals.size()<<'\n';for(const auto& [id,enabled]:visuals)out<<id<<' '<<enabled<<'\n';
        const auto data=out.str();
        WriteCraftSaveFile(path,{reinterpret_cast<const std::uint8_t*>(data.data()),data.size()},MaximumProgressBytes);
    }
    inline std::uint64_t ProgressUnsigned(std::istream& in)
    {
        std::string token;in>>token;std::uint64_t value=0;
        const auto [end,ec]=std::from_chars(token.data(),token.data()+token.size(),value);
        if(ec!=std::errc{}||end!=token.data()+token.size())throw std::invalid_argument("Invalid progress integer.");
        return value;
    }
    inline bool ProgressBool(std::istream& in)
    {
        const auto value=ProgressUnsigned(in);
        if(value>1)throw std::invalid_argument("Invalid progress flag.");
        return value!=0;
    }
    // Parse and restore into copies first. Any malformed file, unknown ID or
    // source mismatch leaves every live subsystem unchanged.
    inline void LoadProgress(const std::filesystem::path& path,std::uint64_t identity,
        RuntimeCollectibleRun& goals,std::optional<RuntimeProximityTriggers>& triggers,TriggerVisuals& visuals)
    {
        const auto bytes=ReadCraftSaveFile(path,MaximumProgressBytes);
        std::istringstream in{std::string(bytes.begin(),bytes.end())};in.imbue(std::locale::classic());
        std::string header;std::getline(in,header);
        if(header!="kairo-progress 1"||ProgressUnsigned(in)!=identity)
            throw std::invalid_argument("Progress belongs to another authored revision.");
        double elapsed=0;if(!(in>>elapsed))throw std::invalid_argument("Invalid progress elapsed time.");
        const auto count=ProgressUnsigned(in);
        if(count>goals.Total())throw std::invalid_argument("Too many saved goals.");
        std::vector<std::uint32_t> collected;
        for(std::uint64_t i=0;i<count;++i)collected.push_back(TriggerID(in));
        const auto stateCount=ProgressUnsigned(in);
        if(stateCount!=(triggers?triggers->Size():0))throw std::invalid_argument("Saved trigger count differs.");
        std::vector<TriggerProgress> states;
        for(std::uint64_t i=0;i<stateCount;++i)states.push_back({TriggerID(in),ProgressBool(in),ProgressBool(in)});
        const auto visualCount=ProgressUnsigned(in);
        if(visualCount!=visuals.size())throw std::invalid_argument("Saved visual count differs.");
        TriggerVisuals restored;
        for(std::uint64_t i=0;i<visualCount;++i)
        {
            const auto id=TriggerID(in);const auto enabled=ProgressBool(in);
            if(!visuals.contains(id)||!restored.emplace(id,enabled).second)
                throw std::invalid_argument("Unknown or duplicate saved visual.");
        }
        in>>std::ws;if(!in.eof())throw std::invalid_argument("Trailing progress fields.");
        auto newGoals=goals;auto newTriggers=triggers;
        newGoals.RestoreProgress(elapsed,collected);
        if(newTriggers)newTriggers->RestoreProgress(states);
        goals=std::move(newGoals);triggers=std::move(newTriggers);visuals=std::move(restored);
    }
}
