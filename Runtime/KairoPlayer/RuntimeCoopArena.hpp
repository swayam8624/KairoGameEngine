#pragma once
#include "RuntimeNetworkProtocol.hpp"
#include <cstdlib>

namespace kairo::player::detail
{
    struct CoopCell{int X,Y;friend bool operator==(CoopCell,CoopCell)=default;};
    inline constexpr std::array<const char*,7> CoopMap{
        "#########","#.......#","#...#...#","#.......#","#...#...#","#.......#","#########"};
    inline constexpr std::array<CoopCell,2> CoopStarts{{{1,1},{1,5}}},CoopGoals{{{7,1},{7,5}}};
    inline constexpr std::uint32_t CoopTickLimit=256;
    struct CoopState
    {
        std::uint32_t Tick=0;
        std::array<CoopCell,2> Actors=CoopStarts;
        [[nodiscard]] bool Won() const noexcept{return Actors==CoopGoals;}
        friend bool operator==(const CoopState&,const CoopState&)=default;
    };
    inline bool CoopWalkable(CoopCell p) noexcept
    {return p.X>=0&&p.X<9&&p.Y>=0&&p.Y<7&&CoopMap[static_cast<unsigned>(p.Y)][p.X]!='#';}
    // An authoritative turn consumes exactly one input from each assigned
    // connection. Validate both before mutating. Players cannot submit
    // coordinates, diagonals, repeated ticks or turns after completion.
    // Collision policy is symmetric: occupied-at-start cells cannot be
    // entered even if vacated, and two moves to the same cell both fail.
    class CoopAuthority
    {
    public:
        [[nodiscard]] CoopState State() const noexcept{return m_State;}
        [[nodiscard]] CoopState Apply(const std::array<NetworkFrame,2>& inputs)
        {
            if(m_State.Won()||m_State.Tick>=CoopTickLimit)throw std::invalid_argument("Co-op match ended.");
            auto next=m_State;next.Tick++;
            for(unsigned i=0;i<2;++i)
            {
                const auto in=inputs[i];ValidateNetworkFrame(in);
                if(in.Kind!=NetworkKind::Input||in.Tick!=next.Tick||std::abs(in.X)+std::abs(in.Y)>1)
                    throw std::invalid_argument("Invalid co-op turn input.");
                const CoopCell p{m_State.Actors[i].X+in.X,m_State.Actors[i].Y+in.Y};
                if(CoopWalkable(p)&&p!=m_State.Actors[1-i])next.Actors[i]=p;
            }
            if(next.Actors[0]==next.Actors[1])next.Actors=m_State.Actors;
            m_State=next;return m_State;
        }
    private:
        CoopState m_State;
    };
    // Snapshot pair is ordered by actor slot, not by arrival time. A pair
    // must carry the same tick and valid distinct cells. Publish only after
    // both records are received; never show a half-updated remote world.
    inline std::array<NetworkFrame,2> EncodeCoopState(CoopState state)
    {
        return {{{NetworkKind::Snapshot,state.Tick,state.Actors[0].X,state.Actors[0].Y},
                 {NetworkKind::Snapshot,state.Tick,state.Actors[1].X,state.Actors[1].Y}}};
    }
    inline CoopState DecodeCoopState(const std::array<NetworkFrame,2>& frames,std::uint32_t expected)
    {
        if(expected>CoopTickLimit)throw std::invalid_argument("Co-op tick outside limit.");
        CoopState state;state.Tick=expected;
        for(unsigned i=0;i<2;++i)
        {
            const auto f=frames[i];ValidateNetworkFrame(f);
            if(f.Kind!=NetworkKind::Snapshot||f.Tick!=expected||!CoopWalkable({f.X,f.Y}))
                throw std::invalid_argument("Invalid co-op snapshot.");
            state.Actors[i]={f.X,f.Y};
        }
        if(state.Actors[0]==state.Actors[1]||(expected==0&&state.Actors!=CoopStarts))
            throw std::invalid_argument("Invalid co-op actor positions.");
        return state;
    }
}
