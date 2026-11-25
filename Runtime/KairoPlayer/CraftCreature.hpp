#pragma once
#include "VoxelPlayer.hpp"

namespace kairo::player::detail
{
    // A deliberately bounded, session-local hostile creature. The voxel
    // solver owns movement; unknown cells are solid for collision and LOS.
    class CraftCreature
    {
        std::optional<VoxelPlayer> m_Body;
        unsigned m_Health=0,m_Respawn=60,m_Attack=0,m_Hit=0;
        double m_Remainder=0;
        static constexpr unsigned MaxHealth=12;
        static double Distance(const std::array<double,3>& a,const std::array<double,3>& b) noexcept
        {return std::hypot(a[0]-b[0],a[1]-b[1],a[2]-b[2]);}
        template<class Query>
        static bool Clear(std::array<double,3> from,std::array<double,3> to,Query&& query)
        {
            const std::array direction{to[0]-from[0],to[1]-from[1],to[2]-from[2]};
            const double length=Distance(from,to);
            if(length<1e-9)return true;
            return !TraceVoxels(from,direction,length,64,[&](VoxelCoordinate c){
                return query(c).value_or(VoxelMaterial{65535});});
        }
        template<class Query>
        void Spawn(const VoxelPlayer& player,Query&& query)
        {
            const int x=static_cast<int>(std::floor(player.Feet[0])),y=static_cast<int>(std::floor(player.Feet[1])),z=static_cast<int>(std::floor(player.Feet[2]));
            const auto config=Config();
            for(int radius=5;radius<=8;++radius)
                for(int dz=-radius;dz<=radius;++dz)for(int dx=-radius;dx<=radius;++dx)
                {
                    if(std::max(std::abs(dx),std::abs(dz))!=radius)continue;
                    for(int dy=2;dy>=-2;--dy)
                    {
                        VoxelPlayer candidate{{x+dx+.5,double(y+dy),z+dz+.5}};
                        if(std::abs(candidate.Feet[0])>1e9||std::abs(candidate.Feet[2])>1e9||std::abs(candidate.Feet[1])>1e9)continue;
                        const auto support=query(VoxelCoordinate{x+dx,y+dy-1,z+dz});
                        if(!support||*support==VoxelAir||VoxelPlayerBlocked(candidate,config,query))continue;
                        candidate.Grounded=true;m_Body=candidate;m_Health=MaxHealth;m_Attack=0;m_Hit=0;return;
                    }
                }
            m_Respawn=20; // No valid resident spawn: try again after one second.
        }
        void Remove() noexcept{m_Body.reset();m_Health=0;m_Respawn=300;m_Attack=0;m_Hit=0;}
    public:
        static VoxelPlayerConfig Config() noexcept
        {VoxelPlayerConfig c;c.Radius=.36;c.Height=1.20;c.EyeHeight=.9;c.Speed=2.0;return c;}
        [[nodiscard]] const std::optional<VoxelPlayer>& Body() const noexcept{return m_Body;}
        [[nodiscard]] unsigned Health() const noexcept{return m_Health;}
        void Reset() noexcept{*this=CraftCreature{};}
        [[nodiscard]] bool Occupies(VoxelCoordinate cell) const noexcept
        {return m_Body&&PlayerIntersectsBlock(*m_Body,Config(),cell);}
        // At most two simulation ticks per frame. One attack per second.
        template<class Query>
        unsigned Advance(double dt,const VoxelPlayer& player,Query&& query)
        {
            if(!std::isfinite(dt)||dt<0)throw std::invalid_argument("Invalid creature elapsed time.");
            ValidateVoxelPlayer(player,VoxelPlayerConfig{});
            unsigned damage=0;m_Remainder+=std::min(dt,.1);
            const auto config=Config();
            while(m_Remainder+1e-10>=.05)
            {
                m_Remainder=std::max(0.,m_Remainder-.05);
                if(m_Attack)--m_Attack;
                if(m_Hit)--m_Hit;
                if(!m_Body){if(m_Respawn)--m_Respawn;if(!m_Respawn)Spawn(player,query);continue;}
                if(Distance(m_Body->Feet,player.Feet)>24||VoxelPlayerBlocked(*m_Body,config,query))
                {Remove();continue;}
                const double dx=player.Feet[0]-m_Body->Feet[0],dz=player.Feet[2]-m_Body->Feet[2];
                const double horizontal=std::hypot(dx,dz);
                VoxelPlayerInput input;
                if(horizontal>1.05){input.X=dx/horizontal;input.Z=dz/horizontal;}
                AdvanceVoxelPlayer(*m_Body,config,input,.05,query);
                const std::array from{m_Body->Feet[0],m_Body->Feet[1]+.8,m_Body->Feet[2]};
                const std::array to{player.Feet[0],player.Feet[1]+.8,player.Feet[2]};
                if(!m_Attack&&Distance(from,to)<=1.4&&Clear(from,to,query))
                {damage+=2;m_Attack=20;}
            }
            return damage;
        }
        // Ray-box intersection in absolute double coordinates. Test the
        // world segment before accepting a hit, including unknown cells.
        template<class Query>
        [[nodiscard]] bool Aimed(std::array<double,3> eye,std::array<double,3> direction,Query&& query) const
        {
            if(!m_Body)return false;
            const double norm=std::hypot(direction[0],direction[1],direction[2]);
            if(!std::isfinite(norm)||norm<=1e-12)return false;
            const auto lo=PlayerMinimum(*m_Body,Config()),hi=PlayerMaximum(*m_Body,Config());
            double enter=0,exit=3;
            for(unsigned i=0;i<3;++i)
            {
                if(!std::isfinite(eye[i]))return false;
                direction[i]/=norm;
                if(std::abs(direction[i])<1e-12){if(eye[i]<lo[i]||eye[i]>hi[i])return false;continue;}
                double a=(lo[i]-eye[i])/direction[i],b=(hi[i]-eye[i])/direction[i];
                if(a>b)std::swap(a,b);
                enter=std::max(enter,a);exit=std::min(exit,b);
                if(enter>exit)return false;
            }
            const std::array point{eye[0]+direction[0]*enter,eye[1]+direction[1]*enter,eye[2]+direction[2]*enter};
            return Clear(eye,point,query);
        }
        // The caller has just tested aim; one hit per half-second, three
        // unarmed hits to defeat. No loot or tool wear in this prototype.
        bool Strike() noexcept
        {
            if(!m_Body||m_Hit)return false;
            m_Health-=std::min(4u,m_Health);m_Hit=10;
            if(!m_Health)Remove();
            return true;
        }
    };
}
