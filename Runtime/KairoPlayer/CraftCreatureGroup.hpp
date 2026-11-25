#pragma once
#include "CraftCreature.hpp"

namespace kairo::player::detail
{
    inline bool CraftCreatureOverlap(const VoxelPlayer& a,const VoxelPlayer& b)
    {
        const auto config=CraftCreature::Config();
        const auto al=PlayerMinimum(a,config),ah=PlayerMaximum(a,config);
        const auto bl=PlayerMinimum(b,config),bh=PlayerMaximum(b,config);
        for(unsigned axis=0;axis<3;++axis)
            if(ah[axis]<=bl[axis]||bh[axis]<=al[axis])return false;
        return true;
    }

    // The group owns one shared fixed clock, not four independently
    // partitioned frame loops. Stable slot order makes peer resolution and
    // nearest-target ties deterministic. No native handles are stored here.
    class CraftCreatureGroup
    {
    public:
        static constexpr std::size_t Capacity=4;
        struct ActorState
        {
            std::optional<std::array<double,3>> Feet;
            CraftCreatureVisual Visual;
            unsigned Health=0;
            bool operator==(const ActorState&) const=default;
        };
        using Snapshot=std::array<ActorState,Capacity>;
        [[nodiscard]] const auto& Members() const noexcept{return m_Actors;}
        [[nodiscard]] CraftCreature& At(std::size_t index){return m_Actors.at(index);}
        [[nodiscard]] const CraftCreature& At(std::size_t index) const{return m_Actors.at(index);}
        [[nodiscard]] std::size_t Count() const noexcept
        {
            return static_cast<std::size_t>(std::count_if(m_Actors.begin(),m_Actors.end(),
                [](const auto& actor){return actor.Body().has_value();}));
        }
        [[nodiscard]] Snapshot State() const noexcept
        {
            Snapshot result{};
            for(std::size_t i=0;i<Capacity;++i)
            {
                const auto& actor=m_Actors[i];
                if(actor.Body())result[i].Feet=actor.Body()->Feet;
                result[i].Visual=actor.Visual();result[i].Health=actor.Health();
            }
            return result;
        }
        void Reset() noexcept
        {
            for(auto& actor:m_Actors)actor.Reset();
            m_Remainder=0;
        }
        [[nodiscard]] bool Occupies(VoxelCoordinate cell) const noexcept
        {
            return std::any_of(m_Actors.begin(),m_Actors.end(),
                [&](const auto& actor){return actor.Occupies(cell);});
        }
        template<class Query>
        unsigned Advance(double dt,const VoxelPlayer& player,Query&& query)
        {
            if(!std::isfinite(dt)||dt<0)throw std::invalid_argument("Invalid group elapsed time.");
            ValidateVoxelPlayer(player,VoxelPlayerConfig{});
            m_Remainder+=std::min(dt,.1);unsigned damage=0;
            while(m_Remainder+1e-10>=.05)
            {
                m_Remainder=std::max(0.,m_Remainder-.05);
                for(std::size_t i=0;i<Capacity;++i)
                {
                    const auto blocked=[&](const VoxelPlayer& candidate)
                    {
                        for(std::size_t j=0;j<Capacity;++j)
                            if(j!=i&&m_Actors[j].Body()&&CraftCreatureOverlap(candidate,*m_Actors[j].Body()))
                                return true;
                        return false;
                    };
                    damage+=m_Actors[i].Advance(.05,player,query,blocked);
                }
            }
            return damage;
        }
        // Use actual visible ray entry distance, not center distance. A
        // farther actor cannot steal a hit from a nearer actor on the ray.
        template<class Query>
        [[nodiscard]] std::optional<std::size_t> Aim(std::array<double,3> eye,
            std::array<double,3> direction,Query&& query) const
        {
            std::optional<std::size_t> result;
            double nearest=4.;
            for(std::size_t i=0;i<Capacity;++i)
                if(const auto distance=m_Actors[i].AimDistance(eye,direction,query);distance&&*distance<nearest)
                {nearest=*distance;result=i;}
            return result;
        }
        template<class Query>
        [[nodiscard]] unsigned AimedHealth(std::array<double,3> eye,
            std::array<double,3> direction,Query&& query) const
        {
            const auto index=Aim(eye,direction,query);
            return index?m_Actors[*index].Health():0;
        }
    private:
        std::array<CraftCreature,Capacity> m_Actors{
            CraftCreature{0,60},CraftCreature{1,80},CraftCreature{2,100},CraftCreature{3,120}};
        double m_Remainder=0;
    };
}
