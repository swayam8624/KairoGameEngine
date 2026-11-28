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
    using HealthPosition=std::array<double,3>;
    inline constexpr std::size_t MaximumHealthBytes=65536;
    inline bool ValidHealthPosition(const HealthPosition& p)
    {return std::all_of(p.begin(),p.end(),[](double x){return std::isfinite(x)&&std::abs(x)<=100000;});}
    inline double HealthDistance(const HealthPosition& a,const HealthPosition& b)
    {return std::hypot(a[0]-b[0],a[1]-b[1],a[2]-b[2]);}
    struct HealthZone{std::uint32_t ID;HealthPosition Center;double Radius;unsigned Damage;};
    struct HealthRules{unsigned Maximum=0,Cooldown=0;double FallY=0;std::vector<HealthZone> Hazards,Checkpoints;};
    inline unsigned HealthUnsigned(std::istream& row,unsigned maximum)
    {
        std::string token;row>>token;unsigned value=0;
        const auto [end,ec]=std::from_chars(token.data(),token.data()+token.size(),value);
        if(ec!=std::errc{}||end!=token.data()+token.size()||!value||value>maximum)
            throw std::invalid_argument("Invalid health integer.");
        return value;
    }
    // Checkpoints use their center as the authored respawn position. The
    // content author remains responsible for clearance from solid geometry.
    inline HealthRules ParseHealthRules(std::string_view source)
    {
        if(source.size()>MaximumHealthBytes)throw std::invalid_argument("Health document too large.");
        std::istringstream in{std::string(source)};in.imbue(std::locale::classic());
        std::string line;auto read=[&]{if(!std::getline(in,line))return false;
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.find('\r')!=std::string::npos)throw std::invalid_argument("Invalid health line.");
            return true;};
        if(!read()||line!="kairo-health 1")throw std::invalid_argument("Invalid health header.");
        HealthRules rules;
        if(!read())throw std::invalid_argument("Missing health settings.");
        {std::istringstream row(line);row.imbue(std::locale::classic());std::string command;row>>command;
         if(command!="health")throw std::invalid_argument("Missing health settings.");
         rules.Maximum=HealthUnsigned(row,10000);rules.Cooldown=HealthUnsigned(row,600);
         if(!(row>>rules.FallY)||!std::isfinite(rules.FallY)||std::abs(rules.FallY)>100000)
             throw std::invalid_argument("Invalid fall threshold.");
         row>>std::ws;if(!row.eof())throw std::invalid_argument("Trailing health settings.");}
        while(read())
        {
            if(line.empty())continue;
            std::istringstream row(line);row.imbue(std::locale::classic());std::string command;row>>command;
            if(command!="hazard"&&command!="checkpoint")throw std::invalid_argument("Unknown health zone.");
            HealthZone zone{};zone.ID=HealthUnsigned(row,UINT32_MAX);
            if(!(row>>zone.Center[0]>>zone.Center[1]>>zone.Center[2]>>zone.Radius)||
                !ValidHealthPosition(zone.Center)||!std::isfinite(zone.Radius)||zone.Radius<=0||zone.Radius>100)
                throw std::invalid_argument("Invalid health zone geometry.");
            if(command=="hazard")zone.Damage=HealthUnsigned(row,rules.Maximum);
            row>>std::ws;if(!row.eof())throw std::invalid_argument("Trailing health zone fields.");
            auto& zones=command=="hazard"?rules.Hazards:rules.Checkpoints;
            if(zones.size()>=256||std::any_of(zones.begin(),zones.end(),[&](auto z){return z.ID==zone.ID;}))
                throw std::invalid_argument("Duplicate or excess health zone.");
            zones.push_back(zone);
        }
        for(auto* zones:{&rules.Hazards,&rules.Checkpoints})
            std::sort(zones->begin(),zones->end(),[](auto a,auto b){return a.ID<b.ID;});
        return rules;
    }
    struct HealthEvent{unsigned Damage=0;bool Respawn=false;std::uint32_t Checkpoint=0;HealthPosition Position{};};
    // Fixed-tick cooldown; overlapping hazards use the lowest stable ID.
    // Death atomically restores full health and the last checkpoint position.
    class RuntimeHealth
    {
        HealthRules m_Rules;
        HealthPosition m_Respawn;
        unsigned m_Health,m_Cooldown=0;
        std::uint32_t m_Checkpoint=0;
        void ValidateSpawn(const HealthPosition& p) const
        {
            if(!ValidHealthPosition(p)||p[1]<=m_Rules.FallY||
                std::any_of(m_Rules.Hazards.begin(),m_Rules.Hazards.end(),[&](auto h){return HealthDistance(h.Center,p)<=h.Radius;}))
                throw std::invalid_argument("Respawn point lies in a hazard or below fall threshold.");
        }
    public:
        RuntimeHealth(std::string_view document,HealthPosition spawn)
            :m_Rules(ParseHealthRules(document)),m_Respawn(spawn),m_Health(m_Rules.Maximum)
        {ValidateSpawn(spawn);for(const auto& c:m_Rules.Checkpoints)ValidateSpawn(c.Center);}
        [[nodiscard]] unsigned Health() const noexcept{return m_Health;}
        [[nodiscard]] unsigned Maximum() const noexcept{return m_Rules.Maximum;}
        [[nodiscard]] std::uint32_t Checkpoint() const noexcept{return m_Checkpoint;}
        HealthEvent Step(HealthPosition position,unsigned incomingDamage=0)
        {
            if(incomingDamage>10000||!ValidHealthPosition(position))throw std::invalid_argument("Invalid health sample.");
            if(m_Cooldown)--m_Cooldown;
            HealthEvent event{};
            if(position[1]<=m_Rules.FallY)event.Damage=m_Health;
            else if(!m_Cooldown)
                for(const auto& h:m_Rules.Hazards)if(HealthDistance(h.Center,position)<=h.Radius)
                {event.Damage=std::min(h.Damage,m_Health);break;}
            if(!event.Damage&&!m_Cooldown)event.Damage=std::min(incomingDamage,m_Health);
            if(event.Damage)
            {
                m_Health-=event.Damage;m_Cooldown=m_Rules.Cooldown;
                if(!m_Health){m_Health=m_Rules.Maximum;event.Respawn=true;event.Position=m_Respawn;return event;}
            }
            if(!event.Damage)
                for(const auto& c:m_Rules.Checkpoints)if(HealthDistance(c.Center,position)<=c.Radius)
                {if(c.ID!=m_Checkpoint){m_Checkpoint=c.ID;m_Respawn=c.Center;event.Checkpoint=c.ID;}break;}
            return event;
        }
    };
}
