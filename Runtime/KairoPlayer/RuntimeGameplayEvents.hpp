#pragma once
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace kairo::player::detail
{
    enum class GameplayEvent {Begin,Collected,Trigger,Defeated,Complete};
    enum class GameplayActionKind {Visibility,DoorOpen,DoorAutomatic,DoorLock};
    struct GameplayRule
    {
        std::uint32_t ID,Source,Threshold,Target;
        GameplayEvent Event;
        bool Once,Enable;
        GameplayActionKind Kind=GameplayActionKind::Visibility;
    };
    struct GameplayAction{std::uint32_t Rule,Target;bool Enable;GameplayActionKind Kind=GameplayActionKind::Visibility;};
    inline std::uint32_t EventInteger(std::istream& row,std::uint32_t max,bool zero=false)
    {
        std::string token;row>>token;std::uint32_t n=0;
        const auto [end,ec]=std::from_chars(token.data(),token.data()+token.size(),n);
        if(ec!=std::errc{}||end!=token.data()+token.size()||n>max||(!zero&&!n))
            throw std::invalid_argument("Invalid event integer.");
        return n;
    }
    inline std::vector<GameplayRule> ParseGameplayEvents(std::string_view source)
    {
        if(source.size()>65536)throw std::invalid_argument("Event document too large.");
        std::istringstream in{std::string(source)};in.imbue(std::locale::classic());
        std::string line;auto read=[&]{if(!std::getline(in,line))return false;
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.find('\r')!=std::string::npos)throw std::invalid_argument("Invalid event line.");
            return true;};
        if(!read()||line!="kairo-events 1")throw std::invalid_argument("Invalid event header.");
        std::vector<GameplayRule> rules;
        while(read())
        {
            if(line.empty())continue;
            std::istringstream row(line);row.imbue(std::locale::classic());
            std::string command,event,mode,action;GameplayRule r{};
            if(!(row>>command)||command!="on"||rules.size()>=512)throw std::invalid_argument("Invalid event rule or budget.");
            r.ID=EventInteger(row,UINT32_MAX);row>>event;
            if(event=="begin")r.Event=GameplayEvent::Begin;
            else if(event=="collected")r.Event=GameplayEvent::Collected;
            else if(event=="trigger")r.Event=GameplayEvent::Trigger;
            else if(event=="defeated")r.Event=GameplayEvent::Defeated;
            else if(event=="complete")r.Event=GameplayEvent::Complete;
            else throw std::invalid_argument("Unknown gameplay event.");
            r.Source=EventInteger(row,UINT32_MAX,true);
            if((r.Event==GameplayEvent::Begin||r.Event==GameplayEvent::Complete)!=(r.Source==0))
                throw std::invalid_argument("Invalid event source.");
            r.Threshold=EventInteger(row,100000);row>>mode>>action;
            if(mode=="once")r.Once=true;
            else if(mode=="repeat")r.Once=false;
            else throw std::invalid_argument("Invalid event repeat mode.");
            if(action=="show")r.Enable=true;
            else if(action=="hide")r.Enable=false;
            else if(action=="door-open")r.Kind=GameplayActionKind::DoorOpen;
            else if(action=="door-auto")r.Kind=GameplayActionKind::DoorAutomatic;
            else if(action=="door-lock")r.Kind=GameplayActionKind::DoorLock;
            else throw std::invalid_argument("Invalid event action.");
            r.Target=EventInteger(row,UINT32_MAX);row>>std::ws;
            if(!row.eof())throw std::invalid_argument("Trailing event fields.");
            rules.push_back(r);
        }
        std::sort(rules.begin(),rules.end(),[](auto a,auto b){return a.ID<b.ID;});
        if(std::adjacent_find(rules.begin(),rules.end(),[](auto a,auto b){return a.ID==b.ID;})!=rules.end())
            throw std::invalid_argument("Duplicate event rule ID.");
        return rules;
    }
    // No recursion, arbitrary code, or implicit emitted events. Each matching
    // rule owns its bounded occurrence counter and emits in ascending ID order.
    class RuntimeGameplayEvents
    {
        struct State{GameplayRule Rule;std::uint32_t Count=0;bool Fired=false;};
        std::vector<State> m_Rules;
    public:
        explicit RuntimeGameplayEvents(std::string_view document)
        {for(auto r:ParseGameplayEvents(document))m_Rules.push_back({r});}
        std::vector<GameplayAction> Dispatch(GameplayEvent event,std::uint32_t source)
        {
            if((event==GameplayEvent::Begin||event==GameplayEvent::Complete)!=(source==0))
                throw std::invalid_argument("Invalid event dispatch source.");
            std::vector<GameplayAction> actions;actions.reserve(m_Rules.size());
            for(auto& state:m_Rules)
            {
                const auto& r=state.Rule;
                if(r.Event!=event||r.Source!=source||(r.Once&&state.Fired))continue;
                if(++state.Count!=r.Threshold)continue;
                actions.push_back({r.ID,r.Target,r.Enable,r.Kind});state.Count=0;state.Fired=true;
            }
            return actions;
        }
    };
}
