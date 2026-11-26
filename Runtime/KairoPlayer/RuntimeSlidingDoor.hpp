#pragma once
#include "RuntimeProximityTriggers.hpp"
#include <utility>

namespace kairo::player::detail
{
    enum class DoorControl {Automatic,HoldOpen,Locked};
    struct DoorRule{std::uint32_t Target=0;double Lift=0,Speed=0,Radius=0;DoorControl Initial=DoorControl::Automatic;};
    inline std::vector<DoorRule> ParseDoorRules(std::string_view source)
    {
        if(source.size()>MaximumTriggerBytes)throw std::invalid_argument("Door document too large.");
        std::istringstream input{std::string(source)};input.imbue(std::locale::classic());
        std::string line;
        auto read=[&]{
            if(!std::getline(input,line))return false;
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.find('\r')!=std::string::npos)throw std::invalid_argument("Invalid door line.");
            return true;
        };
        if(!read()||line!="kairo-doors 1")throw std::invalid_argument("Invalid door header.");
        std::vector<DoorRule> rules;
        while(read())
        {
            if(line.empty())continue;
            std::istringstream row(line);row.imbue(std::locale::classic());
            std::string command;DoorRule rule;
            if(!(row>>command)||command!="door")throw std::invalid_argument("Invalid door statement.");
            rule.Target=TriggerID(row);
            if(!(row>>rule.Lift>>rule.Speed>>rule.Radius))throw std::invalid_argument("Invalid door fields.");
            row>>std::ws;
            if(!row.eof())
            {
                std::string mode;row>>mode;
                if(mode=="locked")rule.Initial=DoorControl::Locked;
                else if(mode=="open")rule.Initial=DoorControl::HoldOpen;
                else if(mode!="auto")throw std::invalid_argument("Unknown door control mode.");
                row>>std::ws;
            }
            if(!row.eof()||!std::isfinite(rule.Lift)||rule.Lift<=0||rule.Lift>20||
                !std::isfinite(rule.Speed)||rule.Speed<=0||rule.Speed>10||
                !std::isfinite(rule.Radius)||rule.Radius<=0||rule.Radius>100||rules.size()>=128)
                throw std::invalid_argument("Invalid door geometry or budget.");
            rules.push_back(rule);
        }
        std::sort(rules.begin(),rules.end(),[](auto a,auto b){return a.Target<b.Target;});
        if(std::adjacent_find(rules.begin(),rules.end(),[](auto a,auto b){return a.Target==b.Target;})!=rules.end())
            throw std::invalid_argument("Duplicate door target.");
        return rules;
    }

    // Vertical travel only. Horizontal proximity keeps a door open even
    // while a player jumps. A conservative player/door bounding radius plus
    // margin defines the minimum permitted activation radius at binding time.
    // This is not swept collision avoidance for other moving bodies.
    class RuntimeSlidingDoor
    {
        DoorRule m_Rule;
        TriggerPosition m_Closed;
        double m_Offset=0;
        DoorControl m_Control;
    public:
        RuntimeSlidingDoor(DoorRule rule,TriggerPosition closed,double guard)
            :m_Rule(rule),m_Closed(closed),m_Control(rule.Initial)
        {
            if(!rule.Target||!ValidTriggerPosition(closed)||!std::isfinite(guard)||guard<=0||
                !std::isfinite(rule.Lift)||rule.Lift<=0||rule.Lift>20||
                !std::isfinite(rule.Speed)||rule.Speed<=0||rule.Speed>10||
                !std::isfinite(rule.Radius)||rule.Radius<=guard||rule.Radius>100||
                !ValidTriggerPosition({closed[0],closed[1]+rule.Lift,closed[2]}))
                throw std::invalid_argument("Door activation radius must exceed conservative clearance.");
        }
        void SetControl(DoorControl control) noexcept{m_Control=control;}
        [[nodiscard]] double Offset() const noexcept{return m_Offset;}
        [[nodiscard]] TriggerPosition Position() const noexcept{return {m_Closed[0],m_Closed[1]+m_Offset,m_Closed[2]};}
        TriggerPosition Step(TriggerPosition player,double dt)
        {
            if(!ValidTriggerPosition(player)||!std::isfinite(dt)||dt<0||dt>.25)
                throw std::invalid_argument("Invalid fixed door sample.");
            const bool near=std::hypot(player[0]-m_Closed[0],player[2]-m_Closed[2])<=m_Rule.Radius;
            const double travel=m_Rule.Speed*dt;
            // A locked, closed gate stays closed even when approached. If
            // already moving/open, defer closing while the player is nearby.
            const bool open=m_Control==DoorControl::HoldOpen||
                (near&&(m_Control==DoorControl::Automatic||m_Offset>0));
            m_Offset=open?std::min(m_Rule.Lift,m_Offset+travel):std::max(0.,m_Offset-travel);
            return Position();
        }
    };
}
