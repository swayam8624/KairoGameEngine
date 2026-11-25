#pragma once
#include <array>
#include <cmath>
#include <stdexcept>

namespace kairo::player::detail
{
    using FragmentPoint=std::array<double,3>;
    struct RuntimeFragment
    {
        FragmentPoint Center{},Velocity{};double Mass=0,HalfExtent=0;
    };
    // One authored cube fractures into eight equal-volume rigid subcubes.
    // This is predetermined topology, not arbitrary mesh cutting. Fragments
    // translate and collide with a ground plane, but do not rotate or collide
    // with one another. The original cube is stationary before fracture.
    class RuntimeBreakableCube
    {
    public:
        static constexpr double FixedStep=1./120.;
        RuntimeBreakableCube(FragmentPoint center,double halfExtent,double mass,double threshold)
            :m_Center(center),m_HalfExtent(halfExtent),m_Mass(mass),m_Threshold(threshold)
        {
            Validate(center);
            if(!std::isfinite(halfExtent)||halfExtent<.01||halfExtent>100||
                !std::isfinite(mass)||mass<.01||mass>10000||!std::isfinite(threshold)||threshold<=0||threshold>10000)
                throw std::invalid_argument("Invalid breakable cube.");
        }
        [[nodiscard]] bool Fractured() const noexcept{return m_Fractured;}
        [[nodiscard]] FragmentPoint Center() const noexcept{return m_Center;}
        [[nodiscard]] double HalfExtent() const noexcept{return m_HalfExtent;}
        [[nodiscard]] const std::array<RuntimeFragment,8>& Fragments() const noexcept{return m_Fragments;}
        // Impulse units kg*m/s. A single threshold-crossing impact creates
        // the pieces once. Equal radial separation velocities sum to zero,
        // while impulse/mass contributes the exact center-of-mass velocity.
        // Additional fracture energy is deliberately authored, not derived
        // from impact energy; this is a gameplay policy, not a material law.
        bool Strike(FragmentPoint impulse,double separationSpeed=2.)
        {
            Validate(impulse);
            if(!std::isfinite(separationSpeed)||separationSpeed<0||separationSpeed>100)
                throw std::invalid_argument("Invalid fracture separation speed.");
            if(m_Fractured||std::hypot(impulse[0],impulse[1],impulse[2])<m_Threshold)return false;
            auto candidate=m_Fragments;
            const double h=m_HalfExtent*.5,radial=separationSpeed/std::sqrt(3.);
            for(unsigned i=0;i<candidate.size();++i)
            {
                auto& f=candidate[i];f.Mass=m_Mass/8.;f.HalfExtent=h;
                for(unsigned axis=0;axis<3;++axis)
                {
                    const double sign=(i&(1u<<axis))?1.:-1.;
                    f.Center[axis]=m_Center[axis]+sign*h;
                    f.Velocity[axis]=impulse[axis]/m_Mass+sign*radial;
                }
                Validate(f.Center);Validate(f.Velocity);
            }
            m_Fragments=candidate;m_Fractured=true;return true;
        }
        // Semi-implicit Euler at 120 Hz. Project penetrating subcubes onto
        // the plane, reflect downward velocity with restitution 0.25 and
        // damp tangential velocity. Publish only fully validated positions.
        void Step(double floor=0)
        {
            if(!std::isfinite(floor)||std::abs(floor)>1e6)throw std::invalid_argument("Invalid fragment floor.");
            if(!m_Fractured)return;
            auto next=m_Fragments;
            for(auto& f:next)
            {
                f.Velocity[1]-=9.81*FixedStep;
                for(unsigned axis=0;axis<3;++axis)f.Center[axis]+=f.Velocity[axis]*FixedStep;
                if(f.Center[1]<floor+f.HalfExtent)
                {
                    f.Center[1]=floor+f.HalfExtent;
                    if(f.Velocity[1]<0)f.Velocity[1]=-f.Velocity[1]*.25;
                    f.Velocity[0]*=.8;f.Velocity[2]*=.8;
                }
                Validate(f.Center);Validate(f.Velocity);
            }
            m_Fragments=next;
        }
        // Restore intact state, keeping the authored geometry and mass.
        void Reset() noexcept{m_Fragments={};m_Fractured=false;}
    private:
        FragmentPoint m_Center;
        double m_HalfExtent,m_Mass,m_Threshold;
        bool m_Fractured=false;
        std::array<RuntimeFragment,8> m_Fragments{};
        static void Validate(FragmentPoint p)
        {for(double x:p)if(!std::isfinite(x)||std::abs(x)>1e6)throw std::invalid_argument("Invalid fragment vector.");}
    };
}
