#include "../CraftCreatureNavigation.hpp"
#include <iostream>
#include <random>
#include <queue>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    VoxelPlayer body{{.5,0,.5},0,true},target{{5.5,0,.5},0,true};
    const auto config=VoxelPlayerConfig{};
    const auto flat=[](VoxelCoordinate c)->std::optional<VoxelMaterial>{return c.Y<0?1:0;};
    Check(FindCraftCreaturePath(body,target,config,flat).size()==6,"Flat route length");
    const auto wall=[](VoxelCoordinate c)->std::optional<VoxelMaterial>{return c.Y<0||(c.X==2&&std::abs(c.Z)<3&&c.Y<2)?1:0;};
    const auto route=FindCraftCreaturePath(body,target,config,wall);
    Check(route.size()==12,"Wall detour length");
    body.Grounded=false;Check(FindCraftCreaturePath(body,target,config,flat).empty(),"Airborne navigation");body.Grounded=true;
    Check(FindCraftCreaturePath(body,target,config,[](VoxelCoordinate)->std::optional<VoxelMaterial>{return {};}).empty(),"Unknown route");
    std::mt19937 rng(1701);
    constexpr int width=25,count=width*width,start=12*width+12;
    unsigned reached=0;
    for(unsigned trial=0;trial<500;++trial)
    {
        std::array<bool,count> blocked{};for(auto& b:blocked)b=rng()%4==0;
        const int goal=static_cast<int>(rng()%count);blocked[start]=blocked[goal]=false;
        const int x=static_cast<int>(rng()%1900000000u)-950000000,z=static_cast<int>(rng()%1900000000u)-950000000;
        body.Feet={x+.5,0,z+.5};target.Feet={x+goal%width-12+.5,0,z+goal/width-12+.5};
        const auto query=[&](VoxelCoordinate c)->std::optional<VoxelMaterial>{
            const int dx=c.X-x+12,dz=c.Z-z+12;
            if(dx<0||dx>=width||dz<0||dz>=width)return {};
            return c.Y<0||(blocked[dz*width+dx]&&c.Y<2)?1:0;
        };
        std::array<int,count> distance{};distance.fill(-1);std::queue<int> q;q.push(start);distance[start]=0;
        while(!q.empty())
        {
            const int current=q.front();q.pop();
            for(int candidate=0;candidate<count;++candidate)
            {
                if(blocked[candidate]||distance[candidate]!=-1||std::abs(candidate%width-current%width)+std::abs(candidate/width-current/width)!=1)continue;
                distance[candidate]=distance[current]+1;q.push(candidate);
            }
        }
        const auto path=FindCraftCreaturePath(body,target,config,query);
        Check(path.size()==static_cast<std::size_t>(distance[goal]+1),"Independent shortest path differs");
        if(path.empty())continue;
        ++reached;
        Check(path.front()==std::array{body.Feet[0],body.Feet[2]}&&path.back()==std::array{target.Feet[0],target.Feet[2]},"Path endpoints");
        for(std::size_t i=0;i<path.size();++i)
        {
            VoxelPlayer p{{path[i][0],0,path[i][1]}};Check(!VoxelPlayerBlocked(p,config,query),"Path crosses solid");
            if(i)Check(std::abs(path[i][0]-path[i-1][0])+std::abs(path[i][1]-path[i-1][1])==1,"Nonadjacent path");
        }
    }
    std::cout<<"500 signed randomized navigation grids matched independent BFS; "<<reached<<" reachable paths\n";
}
