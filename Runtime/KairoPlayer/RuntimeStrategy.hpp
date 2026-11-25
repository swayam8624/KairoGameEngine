#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <queue>
#include <stdexcept>
#include <vector>
#include <string>
#include <utility>

namespace kairo::player::detail
{
    // Discrete strategy map: integer cells, four-neighbor movement, one unit
    // per cell. The host owns presentation and calls Tick at its chosen rate.
    struct StrategyCell{int X=0,Y=0;friend bool operator==(StrategyCell,StrategyCell)=default;};
    enum class StrategyOrder{Idle,Move,Gather};
    struct StrategyUnit
    {
        StrategyCell Position,Target;StrategyOrder Order=StrategyOrder::Idle;
        unsigned Cargo=0;
        friend bool operator==(const StrategyUnit&,const StrategyUnit&)=default;
    };
    class RuntimeStrategy
    {
    public:
        static constexpr unsigned CarryLimit=3;
        // '#' blocks movement, '.' is empty, B is the shared deposit tile,
        // R is a finite resource of six units, U places a worker. Resource
        // and base cells are not walkable: workers interact from adjacency.
        explicit RuntimeStrategy(std::vector<std::string> rows,unsigned goal=6):m_Rows(std::move(rows)),m_Goal(goal)
        {
            if(m_Rows.empty()||m_Rows.size()>32||m_Rows.front().empty()||m_Rows.front().size()>32||!goal||goal>6144)
                throw std::invalid_argument("Invalid strategy map or goal.");
            m_Stock.assign(m_Rows.size()*m_Rows.front().size(),0);unsigned bases=0,total=0;
            for(int y=0;y<int(m_Rows.size());++y)
            {
                if(m_Rows[y].size()!=m_Rows.front().size())throw std::invalid_argument("Ragged map.");
                for(int x=0;x<int(m_Rows[y].size());++x)
                {
                    auto& c=m_Rows[y][x];
                    if(c=='U'){m_Units.push_back({{x,y},{x,y}});c='.';}
                    else if(c=='B'){m_Base={x,y};++bases;}
                    else if(c=='R'){m_Stock[Index({x,y})]=6;total+=6;}
                    else if(c!='.'&&c!='#')throw std::invalid_argument("Unknown strategy tile.");
                }
            }
            if(bases!=1||m_Units.empty()||m_Units.size()>64||goal>total)throw std::invalid_argument("Invalid base, units or resource goal.");
        }
        [[nodiscard]] const std::vector<StrategyUnit>& Units() const noexcept{return m_Units;}
        [[nodiscard]] const std::vector<std::string>& Rows() const noexcept{return m_Rows;}
        [[nodiscard]] unsigned Stored() const noexcept{return m_Stored;}
        [[nodiscard]] unsigned Goal() const noexcept{return m_Goal;}
        [[nodiscard]] bool Won() const noexcept{return m_Stored>=m_Goal;}
        [[nodiscard]] unsigned Stock(StrategyCell cell) const{return m_Stock.at(Index(cell));}
        // An order names a stable worker index. Reject invalid targets before
        // mutation; unreachable destinations wait and replan on future ticks.
        void Move(std::size_t id,StrategyCell target)
        {
            auto& u=m_Units.at(id);if(!Inside(target)||m_Rows[target.Y][target.X]!='.')throw std::invalid_argument("Invalid move target.");
            u.Target=target;u.Order=StrategyOrder::Move;
        }
        void Gather(std::size_t id,StrategyCell target)
        {
            auto& u=m_Units.at(id);if(!Inside(target)||m_Rows[target.Y][target.X]!='R'||!Stock(target))throw std::invalid_argument("Invalid gather target.");
            u.Target=target;u.Order=StrategyOrder::Gather;
        }
        // Stable worker order resolves contested cells. A gather worker moves
        // to resource adjacency, collects one unit/tick up to CarryLimit, then
        // returns to the base to deposit. Depleted resources cause return/idle.
        // No simultaneous swaps; temporary blocking can wait indefinitely.
        void Tick()
        {
            if(Won())return;
            for(std::size_t i=0;i<m_Units.size();++i)
            {
                auto& u=m_Units[i];
                if(u.Order==StrategyOrder::Move)
                {
                    if(u.Position==u.Target){u.Order=StrategyOrder::Idle;continue;}
                    Walk(i,u.Target,false);
                }
                else if(u.Order==StrategyOrder::Gather)
                {
                    auto& stock=m_Stock[Index(u.Target)];
                    if(u.Cargo==CarryLimit||!stock)
                    {
                        if(!u.Cargo){u.Order=StrategyOrder::Idle;continue;}
                        if(Adjacent(u.Position,m_Base)){m_Stored+=u.Cargo;u.Cargo=0;if(!stock)u.Order=StrategyOrder::Idle;}
                        else Walk(i,m_Base,true);
                    }
                    else if(Adjacent(u.Position,u.Target)){--stock;++u.Cargo;}
                    else Walk(i,u.Target,true);
                }
            }
        }
    private:
        std::vector<std::string> m_Rows;
        std::vector<unsigned> m_Stock;
        std::vector<StrategyUnit> m_Units;
        StrategyCell m_Base;unsigned m_Stored=0,m_Goal;
        [[nodiscard]] bool Inside(StrategyCell p) const noexcept
        {return p.X>=0&&p.Y>=0&&p.Y<int(m_Rows.size())&&p.X<int(m_Rows.front().size());}
        [[nodiscard]] std::size_t Index(StrategyCell p) const
        {if(!Inside(p))throw std::out_of_range("Map cell.");return std::size_t(p.Y)*m_Rows.front().size()+std::size_t(p.X);}
        [[nodiscard]] static bool Adjacent(StrategyCell a,StrategyCell b) noexcept
        {return std::abs(a.X-b.X)+std::abs(a.Y-b.Y)==1;}
        // Other workers occupy cells even if they intend to move later in
        // this tick. This guarantees no overlap without a separate reservation map.
        [[nodiscard]] bool Walkable(StrategyCell p,std::size_t worker) const
        {
            if(!Inside(p)||m_Rows[p.Y][p.X]!='.')return false;
            for(std::size_t i=0;i<m_Units.size();++i)if(i!=worker&&m_Units[i].Position==p)return false;
            return true;
        }
        // Breadth-first search on the unit-weight graph. Track the first
        // step along each shortest route instead of reconstructing a path.
        // Neighbor order is fixed, so equal-length routes replay identically.
        void Walk(std::size_t worker,StrategyCell target,bool adjacent)
        {
            struct Node{StrategyCell Cell,First;};
            const auto start=m_Units[worker].Position;
            std::queue<Node> queue;queue.push({start,start});
            std::vector<bool> visited(m_Stock.size());visited[Index(start)]=true;
            constexpr std::array<StrategyCell,4> directions{{{1,0},{0,1},{-1,0},{0,-1}}};
            while(!queue.empty())
            {
                const auto n=queue.front();queue.pop();
                if(adjacent?Adjacent(n.Cell,target):n.Cell==target){m_Units[worker].Position=n.First;return;}
                for(auto d:directions)
                {
                    const StrategyCell next{n.Cell.X+d.X,n.Cell.Y+d.Y};
                    if(!Walkable(next,worker)||visited[Index(next)])continue;
                    visited[Index(next)]=true;queue.push({next,n.Cell==start?next:n.First});
                }
            }
        }
    };
    // A wall forces a detour between workers, deposit and resources.
    [[nodiscard]] inline std::vector<std::string> StrategyDemoLevel()
    {return {"##########","#B..#...R#","#U..#....#","#...#....#","#...U....#","##########"};}
}
