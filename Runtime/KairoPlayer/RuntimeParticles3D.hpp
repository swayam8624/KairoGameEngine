#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace kairo::player::detail
{
    using ParticlePoint=std::array<float,3>;
    struct Particle3D
    {
        ParticlePoint Position{},Velocity{};
        unsigned Age=0;
        bool Active=false;
        [[nodiscard]] float Scale() const noexcept {return 1.f-.75f*float(Age)/48.f;}
        friend bool operator==(const Particle3D&,const Particle3D&)=default;
    };
    // One fixed-size mesh pool, no GPU calls or allocations during Advance.
    // The caller samples the emitter's world position once per rendered frame.
    class RuntimeParticles3D final
    {
        std::vector<Particle3D> slots_;
        std::uint32_t random_=0x12345678u;
        unsigned phase_=0;
        double remainder_=0;
        static constexpr double Tick=1./60.;
        static std::size_t CheckCapacity(std::size_t capacity)
        {
            if(!capacity||capacity>128)throw std::invalid_argument("3D particle capacity must be 1..128.");
            return capacity;
        }
        float Random()
        {
            random_=1664525u*random_+1013904223u;
            return float(random_>>8)/16777216.f;
        }
        void Step(const std::optional<ParticlePoint>& origin)
        {
            for(auto& p:slots_)if(p.Active)
            {
                if(++p.Age>=48){p.Active=false;continue;}
                p.Velocity[1]-=2.f/60.f;
                for(std::size_t i=0;i<3;++i)p.Position[i]+=p.Velocity[i]/60.f;
            }
            if(++phase_!=3)return;
            phase_=0;
            if(!origin)return;
            for(auto& p:slots_)if(!p.Active)
            {
                p={*origin,{(Random()-.5f)*1.5f,1.8f+Random(),(Random()-.5f)*1.5f},0,true};
                break; // Saturated pools drop the spawn, never evict live particles.
            }
        }
    public:
        explicit RuntimeParticles3D(std::size_t capacity):slots_(CheckCapacity(capacity)){}
        [[nodiscard]] const std::vector<Particle3D>& Slots() const noexcept{return slots_;}
        void Advance(double dt,std::optional<ParticlePoint> origin)
        {
            if(!std::isfinite(dt)||dt<0||dt>.25)throw std::invalid_argument("Invalid particle frame time.");
            if(origin)for(float x:*origin)if(!std::isfinite(x)||std::abs(x)>1.e6f)
                throw std::invalid_argument("Invalid particle emitter position.");
            remainder_+=dt;
            // Small tolerance avoids a lost tick from floating-point partitioning.
            while(remainder_+1.e-12>=Tick)
            {
                remainder_-=Tick;
                if(remainder_<0)remainder_=0;
                Step(origin);
            }
        }
    };
}
