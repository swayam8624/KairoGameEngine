#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace kairo::player::detail
{
    // Renderer-neutral objective state. Authored IDs remain stable after
    // collection so replay/score consumers do not depend on vector erasure.
    class RuntimeCollectibleRun
    {
    public:
        using Position=std::array<double,3>;
        struct Goal{std::uint32_t ID;Position Center;bool Collected=false;};
        static constexpr std::size_t MaximumGoals=4096;
        void Add(std::uint32_t id,Position center)
        {
            if(m_Started||!id||m_Goals.size()>=MaximumGoals||
                std::any_of(m_Goals.begin(),m_Goals.end(),[&](const auto& g){return g.ID==id;}))
                throw std::invalid_argument("Invalid or duplicate collectible.");
            Validate(center);m_Goals.push_back({id,center,false});
        }
        [[nodiscard]] std::size_t Total() const noexcept{return m_Goals.size();}
        [[nodiscard]] std::size_t Score() const noexcept{return m_Score;}
        [[nodiscard]] bool Won() const noexcept{return Total()!=0&&m_Score==Total();}
        [[nodiscard]] double Elapsed() const noexcept{return m_Elapsed;}
        [[nodiscard]] const std::vector<Goal>& Goals() const noexcept{return m_Goals;}
        std::vector<std::uint32_t> Step(Position position,double dt,double radius=.85)
        {
            Validate(position);
            if(!std::isfinite(dt)||dt<0||!std::isfinite(radius)||radius<=0||radius>8)
                throw std::invalid_argument("Invalid collectible simulation input.");
            if(Won()||!Total())return {};
            if(!std::isfinite(m_Elapsed+dt))throw std::overflow_error("Collectible elapsed time overflow.");
            std::vector<std::uint32_t> result;
            result.reserve(m_Goals.size()-m_Score);
            for(const auto& g:m_Goals)
                if(!g.Collected&&std::hypot(g.Center[0]-position[0],g.Center[1]-position[1],
                    g.Center[2]-position[2])<=radius)result.push_back(g.ID);
            m_Started=true;m_Elapsed+=dt;
            // IDs in result follow goal order. Publish in a second linear
            // pass, after all allocations and validation have succeeded.
            auto next=result.begin();
            for(auto& g:m_Goals)
                if(next!=result.end()&&g.ID==*next)
                {g.Collected=true;++m_Score;++next;}
            return result;
        }
    private:
        std::vector<Goal> m_Goals;
        std::size_t m_Score=0;
        double m_Elapsed=0;
        bool m_Started=false;
        static void Validate(const Position& p)
        {
            for(auto x:p)if(!std::isfinite(x)||std::abs(x)>1e9)
                throw std::invalid_argument("Invalid collectible position.");
        }
    };
}
