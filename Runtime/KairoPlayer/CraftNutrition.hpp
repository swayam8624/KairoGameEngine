#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace kairo::player::detail
{
    // Fixed 20 Hz survival clock. Frame partitions do not change the policy
    // for equal elapsed time and constant activity. Long stalls are capped.
    class CraftNutrition
    {
        // Hunger, exertion ticks and recovery ticks are simulation state;
        // remainder carries sub-tick seconds between rendered frames.
        std::uint32_t m_Hunger=20,m_Exertion=0,m_Recovery=0;
        double m_Remainder=0;
    public:
        // Maximum integer hunger used by save validation and the HUD.
        static constexpr std::uint32_t MaximumHunger=20;
        // Read-only UI/checkpoint value.
        [[nodiscard]] std::uint32_t Hunger() const noexcept{return m_Hunger;}
        // Sprint policy reserves the last four hunger points.
        [[nodiscard]] bool CanSprint() const noexcept{return m_Hunger>4;}
        // Restore constructor defaults and clear all accumulated fractional time.
        void Reset() noexcept{*this=CraftNutrition{};}
        // Only hunger is persisted; exertion/recovery clocks restart on load.
        void Restore(std::uint32_t hunger)
        {
            if(hunger>MaximumHunger)throw std::invalid_argument("Invalid restored hunger.");
            Reset();m_Hunger=hunger;
        }
        // Prototype forage: two harvested grass blocks yield one meal.
        // Full hunger and insufficient inputs are no-op transactions.
        // counts is Craft's four-slot inventory. Validate every stack before
        // deducting grass and increasing hunger; no changes on failed forage.
        bool Eat(std::array<std::uint32_t,4>& counts)
        {
            if(std::any_of(counts.begin(),counts.end(),[](auto n){return n>999;}))
                throw std::invalid_argument("Invalid forage inventory.");
            if(m_Hunger==MaximumHunger||counts[0]<2)return false;
            counts[0]-=2;m_Hunger=std::min(MaximumHunger,m_Hunger+6);m_Recovery=0;
            return true;
        }
        // Vitals supplies Dead/Health/Damage/Heal; CraftVitals is the normal
        // collaborator. Fixed 0.05 s steps avoid dependence on render partition.
        // Exertion accrues 1 or 3 units per tick; 600 removes one hunger.
        // Every 80 eligible ticks heal one HP (costing hunger) or damage starvation.
        template<class Vitals>
        void Advance(double dt,bool sprinting,Vitals& vitals)
        {
            if(!std::isfinite(dt)||dt<0)throw std::invalid_argument("Invalid nutrition elapsed time.");
            if(vitals.Dead())return;
            m_Remainder+=std::min(dt,.1);
            while(m_Remainder+1e-10>=.05)
            {
                m_Remainder=std::max(0.,m_Remainder-.05);
                m_Exertion+=sprinting&&CanSprint()?3u:1u;
                if(m_Exertion>=600){m_Exertion-=600;if(m_Hunger)--m_Hunger;}
                const bool starving=m_Hunger==0;
                const bool healing=m_Hunger>=16&&vitals.Health()<20;
                if(!starving&&!healing){m_Recovery=0;continue;}
                if(++m_Recovery<80)continue;
                m_Recovery=0;
                if(starving){vitals.Damage(1);if(vitals.Dead())break;}
                else{vitals.Heal(1);--m_Hunger;}
            }
        }
    };
}
