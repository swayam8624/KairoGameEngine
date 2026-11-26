#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace kairo::player::detail
{
    using EnemyPosition=std::array<double,3>;
    enum class EnemyMode{Patrol,Pursue,Attack,Search};
    struct EnemySettings{double Speed=2,Sight=8,Reach=1.5;unsigned Memory=120,Cooldown=45,Damage=10;};
    struct EnemyCommand{EnemyMode Mode;EnemyPosition Velocity;unsigned Damage;EnemyPosition Goal{};};
    inline bool EnemyFinite(const EnemyPosition& p)
    {return std::all_of(p.begin(),p.end(),[](double v){return std::isfinite(v)&&std::abs(v)<=100000;});}
    inline double EnemyDistance(const EnemyPosition& a,const EnemyPosition& b)
    {return std::hypot(a[0]-b[0],a[1]-b[1],a[2]-b[2]);}
    // Pure fixed-step decision policy: visibility is supplied by the physics
    // adapter; the motor, not this policy, resolves movement and collisions.
    class RuntimeEnemy
    {
        EnemySettings m_Settings;
        std::vector<EnemyPosition> m_Patrol;
        std::size_t m_Waypoint=0;
        EnemyPosition m_LastSeen{};
        unsigned m_Memory=0,m_Cooldown=0;
    public:
        RuntimeEnemy(EnemySettings settings,std::vector<EnemyPosition> patrol)
            :m_Settings(settings),m_Patrol(patrol)
        {
            if(!std::isfinite(settings.Speed)||settings.Speed<=0||settings.Speed>10||
               !std::isfinite(settings.Sight)||settings.Sight<=0||settings.Sight>100||
               !std::isfinite(settings.Reach)||settings.Reach<=0||settings.Reach>settings.Sight||
               !settings.Memory||settings.Memory>3600||!settings.Cooldown||settings.Cooldown>600||
               !settings.Damage||settings.Damage>10000||patrol.empty()||patrol.size()>64||
               !std::all_of(patrol.begin(),patrol.end(),EnemyFinite))
                throw std::invalid_argument("Invalid enemy settings or patrol.");
        }
        EnemyCommand Step(EnemyPosition self,EnemyPosition player,bool visible,double dt)
        {
            if(!EnemyFinite(self)||!EnemyFinite(player)||!std::isfinite(dt)||dt<=0||dt>.25)
                throw std::invalid_argument("Invalid enemy sample.");
            if(m_Cooldown)--m_Cooldown;
            if(m_Memory)--m_Memory;
            const double distance=EnemyDistance(self,player);
            visible=visible&&distance<=m_Settings.Sight;
            EnemyPosition goal{};EnemyMode mode;
            if(visible)
            {
                m_LastSeen=player;m_Memory=m_Settings.Memory;
                if(distance<=m_Settings.Reach)
                {
                    unsigned damage=0;
                    if(!m_Cooldown){damage=m_Settings.Damage;m_Cooldown=m_Settings.Cooldown;}
                    return {EnemyMode::Attack,{},damage,self};
                }
                goal=player;mode=EnemyMode::Pursue;
            }
            else if(m_Memory){goal=m_LastSeen;mode=EnemyMode::Search;}
            else
            {
                if(std::hypot(self[0]-m_Patrol[m_Waypoint][0],self[2]-m_Patrol[m_Waypoint][2])<=.2)
                    m_Waypoint=(m_Waypoint+1)%m_Patrol.size();
                goal=m_Patrol[m_Waypoint];mode=EnemyMode::Patrol;
            }
            const double x=goal[0]-self[0],z=goal[2]-self[2],length=std::hypot(x,z);
            if(length<=1e-9)return {mode,{},0,goal};
            const double speed=std::min(m_Settings.Speed,length/dt);
            return {mode,{x*speed/length,0,z*speed/length},0,goal};
        }
    };
}
