#pragma once
#include <algorithm>
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
    inline constexpr std::size_t MaximumCombatBytes=65536;
    struct CombatTarget{std::uint32_t ID;unsigned Health;};
    struct CombatRules{unsigned Damage=0,Cooldown=0;double Range=0;std::vector<CombatTarget> Targets;};
    inline unsigned CombatInteger(std::istream& row,unsigned max)
    {
        std::string token;row>>token;unsigned value=0;
        const auto [end,ec]=std::from_chars(token.data(),token.data()+token.size(),value);
        if(ec!=std::errc{}||end!=token.data()+token.size()||!value||value>max)
            throw std::invalid_argument("Invalid combat integer.");
        return value;
    }
    inline CombatRules ParseCombatRules(std::string_view source)
    {
        if(source.size()>MaximumCombatBytes)throw std::invalid_argument("Combat document too large.");
        std::istringstream in{std::string(source)};in.imbue(std::locale::classic());
        std::string line;auto read=[&]{if(!std::getline(in,line))return false;
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.find('\r')!=std::string::npos)throw std::invalid_argument("Invalid combat line.");
            return true;};
        if(!read()||line!="kairo-combat 1"||!read())throw std::invalid_argument("Missing combat header/settings.");
        CombatRules rules;
        {std::istringstream row(line);row.imbue(std::locale::classic());std::string command;row>>command;
         if(command!="attack")throw std::invalid_argument("Missing attack settings.");
         rules.Damage=CombatInteger(row,10000);rules.Cooldown=CombatInteger(row,600);
         if(!(row>>rules.Range)||!std::isfinite(rules.Range)||rules.Range<=0||rules.Range>100)
             throw std::invalid_argument("Invalid attack range.");
         row>>std::ws;if(!row.eof())throw std::invalid_argument("Trailing attack fields.");}
        while(read())
        {
            if(line.empty())continue;
            std::istringstream row(line);row.imbue(std::locale::classic());std::string command;row>>command;
            if(command!="target")throw std::invalid_argument("Unknown combat record.");
            const CombatTarget target{CombatInteger(row,UINT32_MAX),CombatInteger(row,10000)};
            row>>std::ws;if(!row.eof()||rules.Targets.size()>=512)throw std::invalid_argument("Invalid combat target.");
            rules.Targets.push_back(target);
        }
        std::sort(rules.Targets.begin(),rules.Targets.end(),[](auto a,auto b){return a.ID<b.ID;});
        if(std::adjacent_find(rules.Targets.begin(),rules.Targets.end(),[](auto a,auto b){return a.ID==b.ID;})!=rules.Targets.end())
            throw std::invalid_argument("Duplicate combat target.");
        return rules;
    }
    struct AttackResult{bool Fired=false;std::uint32_t Target=0;unsigned Damage=0;bool Destroyed=false;};
    // Ray selection belongs to the physics adapter. A press is consumed once,
    // including a miss or a press during cooldown; no automatic repeat.
    class RuntimeCombat
    {
        CombatRules m_Rules;
        unsigned m_Cooldown=0;
        bool m_Pending=false;
    public:
        explicit RuntimeCombat(std::string_view document):m_Rules(ParseCombatRules(document)){}
        [[nodiscard]] double Range() const noexcept{return m_Rules.Range;}
        [[nodiscard]] const std::vector<CombatTarget>& Targets() const noexcept{return m_Rules.Targets;}
        void CapturePress(bool pressed) noexcept{m_Pending=m_Pending||pressed;}
        void ClearInput() noexcept{m_Pending=false;}
        // Called once per fixed tick. Query only on a permitted attack.
        template<class Query> AttackResult Step(Query&& hit)
        {
            if(m_Cooldown)--m_Cooldown;
            const bool fire=m_Pending&&!m_Cooldown;m_Pending=false;
            if(!fire)return {};
            m_Cooldown=m_Rules.Cooldown;
            const std::uint32_t id=hit(m_Rules.Range);
            auto target=std::lower_bound(m_Rules.Targets.begin(),m_Rules.Targets.end(),id,
                [](const auto& t,auto key){return t.ID<key;});
            if(target==m_Rules.Targets.end()||target->ID!=id||!target->Health)return {true,0,0,false};
            const unsigned damage=std::min(m_Rules.Damage,target->Health);
            target->Health-=damage;
            return {true,id,damage,!target->Health};
        }
    };
}
