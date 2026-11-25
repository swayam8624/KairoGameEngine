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
        std::uint32_t m_Hunger=20,m_Exertion=0,m_Recovery=0;
        double m_Remainder=0;
    public:
        static constexpr std::uint32_t MaximumHunger=20;
        [[nodiscard]] std::uint32_t Hunger() const noexcept{return m_Hunger;}
        [[nodiscard]] bool CanSprint() const noexcept{return m_Hunger>4;}
        void Reset() noexcept{*this=CraftNutrition{};}
        void Restore(std::uint32_t hunger)
        {
            if(hunger>MaximumHunger)throw std::invalid_argument("Invalid restored hunger.");
            Reset();m_Hunger=hunger;
        }
        // Prototype forage: two harvested grass blocks yield one meal.
        // Full hunger and insufficient inputs are no-op transactions.
        bool Eat(std::array<std::uint32_t,4>& counts)
        {
            if(std::any_of(counts.begin(),counts.end(),[](auto n){return n>999;}))
                throw std::invalid_argument("Invalid forage inventory.");
            if(m_Hunger==MaximumHunger||counts[0]<2)return false;
            counts[0]-=2;m_Hunger=std::min(MaximumHunger,m_Hunger+6);m_Recovery=0;
            return true;
        }
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
