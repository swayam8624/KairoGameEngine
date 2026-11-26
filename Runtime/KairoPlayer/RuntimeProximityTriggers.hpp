#pragma once
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace kairo::player::detail
{
    using TriggerPosition=std::array<double,3>;
    enum class TriggerEdge {Enter,Exit,Once};
    struct TriggerRule
    {
        std::uint32_t ID=0,Target=0;
        TriggerPosition Center{};
        double Radius=1;
        TriggerEdge Edge=TriggerEdge::Enter;
        bool Enable=true;
    };
    struct TriggerProgress{std::uint32_t ID;bool Inside,Fired;};
    struct TriggerEvent{std::uint32_t ID,Target;bool Enable;};
    inline constexpr std::size_t MaximumTriggerBytes=65536,MaximumTriggers=512;
    inline bool ValidTriggerPosition(const TriggerPosition& p)
    {
        return std::all_of(p.begin(),p.end(),[](double x){return std::isfinite(x)&&std::abs(x)<=1e9;});
    }
    inline std::uint32_t TriggerID(std::istream& row)
    {
        std::string token;row>>token;
        std::uint32_t value=0;
        const auto [end,ec]=std::from_chars(token.data(),token.data()+token.size(),value);
        if(ec!=std::errc{}||end!=token.data()+token.size()||!value)
            throw std::invalid_argument("Invalid trigger ID.");
        return value;
    }
    // A versioned declarative format, with no executable expressions.
    // Stable ascending rule IDs define conflict order independently of file order.
    inline std::vector<TriggerRule> ParseTriggerRules(std::string_view source)
    {
        if(source.size()>MaximumTriggerBytes)throw std::invalid_argument("Trigger document too large.");
        std::istringstream in{std::string(source)};in.imbue(std::locale::classic());
        std::string line;
        auto read=[&]{
            if(!std::getline(in,line))return false;
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.find('\r')!=std::string::npos)throw std::invalid_argument("Invalid trigger line.");
            return true;
        };
        if(!read()||line!="kairo-triggers 1")throw std::invalid_argument("Invalid trigger header.");
        std::vector<TriggerRule> rules;
        while(read())
        {
            if(line.empty())continue;
            std::istringstream row(line);row.imbue(std::locale::classic());
            std::string command,edge,action;TriggerRule r;
            if(!(row>>command)||command!="trigger")throw std::invalid_argument("Invalid trigger statement.");
            r.ID=TriggerID(row);
            if(!(row>>r.Center[0]>>r.Center[1]>>r.Center[2]>>r.Radius>>edge))
                throw std::invalid_argument("Invalid trigger geometry.");
            r.Target=TriggerID(row);
            if(!(row>>action))throw std::invalid_argument("Missing trigger action.");
            row>>std::ws;
            if(!row.eof()||!ValidTriggerPosition(r.Center)||!std::isfinite(r.Radius)||r.Radius<=0||r.Radius>1000)
                throw std::invalid_argument("Invalid trigger fields.");
            if(edge=="enter")r.Edge=TriggerEdge::Enter;
            else if(edge=="exit")r.Edge=TriggerEdge::Exit;
            else if(edge=="once")r.Edge=TriggerEdge::Once;
            else throw std::invalid_argument("Invalid trigger edge.");
            if(action=="show")r.Enable=true;
            else if(action=="hide")r.Enable=false;
            else throw std::invalid_argument("Invalid trigger action.");
            if(rules.size()>=MaximumTriggers)throw std::invalid_argument("Too many triggers.");
            rules.push_back(r);
        }
        std::sort(rules.begin(),rules.end(),[](const auto& a,const auto& b){return a.ID<b.ID;});
        if(std::adjacent_find(rules.begin(),rules.end(),[](const auto& a,const auto& b){return a.ID==b.ID;})!=rules.end())
            throw std::invalid_argument("Duplicate trigger ID.");
        return rules;
    }
    // All observations belong to one game-thread sample. Reserve event
    // capacity before mutating state, so invalid input cannot consume edges.
    class RuntimeProximityTriggers
    {
        struct State{TriggerRule Rule;bool Inside=false,Fired=false;};
        std::vector<State> m_States;
    public:
        explicit RuntimeProximityTriggers(std::string_view document)
        {
            for(const auto& rule:ParseTriggerRules(document))m_States.push_back({rule,false,false});
        }
        [[nodiscard]] std::size_t Size() const noexcept{return m_States.size();}
        [[nodiscard]] std::vector<TriggerProgress> Progress() const
        {
            std::vector<TriggerProgress> result;result.reserve(m_States.size());
            for(const auto& s:m_States)result.push_back({s.Rule.ID,s.Inside,s.Fired});
            return result;
        }
        void RestoreProgress(const std::vector<TriggerProgress>& progress)
        {
            if(progress.size()!=m_States.size())throw std::invalid_argument("Saved trigger count differs.");
            for(std::size_t i=0;i<progress.size();++i)
                if(progress[i].ID!=m_States[i].Rule.ID)throw std::invalid_argument("Saved trigger IDs differ.");
            for(std::size_t i=0;i<progress.size();++i)
            {m_States[i].Inside=progress[i].Inside;m_States[i].Fired=progress[i].Fired;}
        }
        std::vector<TriggerEvent> Step(const TriggerPosition& position)
        {
            if(!ValidTriggerPosition(position))throw std::invalid_argument("Invalid trigger sample.");
            std::vector<TriggerEvent> events;events.reserve(m_States.size());
            for(auto& state:m_States)
            {
                const auto& r=state.Rule;
                const bool inside=std::hypot(position[0]-r.Center[0],position[1]-r.Center[1],position[2]-r.Center[2])<=r.Radius;
                const bool enter=inside&&!state.Inside,exit=!inside&&state.Inside;
                const bool fire=(r.Edge==TriggerEdge::Enter&&enter)||(r.Edge==TriggerEdge::Exit&&exit)||
                    (r.Edge==TriggerEdge::Once&&enter&&!state.Fired);
                if(fire){events.push_back({r.ID,r.Target,r.Enable});state.Fired=true;}
                state.Inside=inside;
            }
            return events;
        }
    };
}
