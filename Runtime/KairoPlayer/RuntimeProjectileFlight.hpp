#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace kairo::player::detail
{
    using FlightVector=std::array<double,3>;
    struct Flight {FlightVector Position,Direction;double Remaining,Speed;};
    // A point projectile traces the entire distance travelled each fixed tick.
    // The host supplies the nearest solid hit (0 denotes an unmapped blocker).
    class RuntimeProjectileFlight
    {
        std::vector<Flight> m_Flights;
        static bool Valid(const FlightVector& p)
        {return std::all_of(p.begin(),p.end(),[](double v){return std::isfinite(v)&&std::abs(v)<=1e9;});}
    public:
        static constexpr std::size_t Capacity=512;
        [[nodiscard]] std::size_t Size() const noexcept{return m_Flights.size();}
        [[nodiscard]] const std::vector<Flight>& Flights() const noexcept{return m_Flights;}
        void Clear() noexcept{m_Flights.clear();}
        bool Launch(FlightVector origin,FlightVector direction,double range,double speed)
        {
            const double length=std::hypot(direction[0],direction[1],direction[2]);
            if(!Valid(origin)||!Valid(direction)||!std::isfinite(length)||length<=1e-9||
                !std::isfinite(range)||range<=0||range>100||!std::isfinite(speed)||speed<1||speed>500)
                throw std::invalid_argument("Invalid projectile launch.");
            if(Size()>=Capacity)return false;
            for(auto& v:direction)v/=length;
            m_Flights.push_back({origin,direction,range,speed});return true;
        }
        // Resolve in launch order, applying each impact before the next query.
        // The host may remove a collider on impact; later shots see that change.
        template<class Query,class Impact> void Step(double dt,Query&& query,Impact&& impact)
        {
            if(!std::isfinite(dt)||dt<=0||dt>.25)throw std::invalid_argument("Invalid projectile timestep.");
            for(std::size_t i=0;i<m_Flights.size();)
            {
                auto& f=m_Flights[i];
                const double distance=std::min(f.Remaining,f.Speed*dt);
                const std::optional<std::uint32_t> hit=query(f.Position,f.Direction,distance);
                if(hit)
                {
                    impact(*hit);m_Flights.erase(m_Flights.begin()+i);continue;
                }
                for(unsigned k=0;k<3;++k)f.Position[k]+=f.Direction[k]*distance;
                f.Remaining-=distance;
                if(f.Remaining<=0)m_Flights.erase(m_Flights.begin()+i);
                else ++i;
            }
        }
    };
}
