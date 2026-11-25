#pragma once
#include "VoxelPlayer.hpp"
#include <vector>

namespace kairo::player::detail
{
    // Bounded four-neighbor navigation on the creature's current grounded
    // voxel layer. No jumping, climbing, or unloaded-cell speculation.
    // The returned path includes the start-cell center and each successive
    // center. The physics solver still owns actual motion and collision.
    template<class Query>
    std::vector<std::array<double,2>> FindCraftCreaturePath(const VoxelPlayer& body,
        const VoxelPlayer& target,const VoxelPlayerConfig& config,Query&& query)
    {
        ValidateVoxelPlayer(body,config);
        ValidateVoxelPlayer(target,VoxelPlayerConfig{});
        constexpr int Radius=12,Width=2*Radius+1,Count=Width*Width;
        const double height=std::round(body.Feet[1]);
        if(!body.Grounded||std::abs(body.Feet[1]-height)>1e-4||std::abs(target.Feet[1]-height)>1.0)return {};
        const int x=static_cast<int>(std::floor(body.Feet[0])),z=static_cast<int>(std::floor(body.Feet[2]));
        const int gx=static_cast<int>(std::floor(target.Feet[0]))-x,gz=static_cast<int>(std::floor(target.Feet[2]))-z;
        if(std::abs(gx)>Radius||std::abs(gz)>Radius)return {};
        const auto index=[](int dx,int dz){return (dz+Radius)*Width+dx+Radius;};
        const auto center=[&](int id){return std::array<double,2>{x+(id%Width-Radius)+.5,z+(id/Width-Radius)+.5};};
        const auto valid=[&](int id){
            const auto p=center(id);
            VoxelPlayer candidate{{p[0],height,p[1]}};
            if(std::abs(p[0])>1e9||std::abs(p[1])>1e9)return false;
            const auto support=query(VoxelCoordinate{static_cast<int>(std::floor(p[0])),static_cast<int>(height)-1,static_cast<int>(std::floor(p[1]))});
            return support&&*support!=VoxelAir&&!VoxelPlayerBlocked(candidate,config,query);
        };
        const int start=index(0,0),goal=index(gx,gz);
        if(!valid(start)||!valid(goal))return {};
        std::array<int,Count> parent{},queue{};parent.fill(-1);
        parent[start]=start;queue[0]=start;int head=0,tail=1;
        constexpr std::array<std::array<int,2>,4> offsets{{{{1,0}},{{0,1}},{{-1,0}},{{0,-1}}}};
        while(head<tail&&parent[goal]==-1)
        {
            const int id=queue[head++],dx=id%Width-Radius,dz=id/Width-Radius;
            for(const auto& offset:offsets)
            {
                const int nx=dx+offset[0],nz=dz+offset[1];
                if(std::abs(nx)>Radius||std::abs(nz)>Radius)continue;
                const int next=index(nx,nz);
                if(parent[next]!=-1||!valid(next))continue;
                parent[next]=id;queue[tail++]=next;
            }
        }
        if(parent[goal]==-1)return {};
        std::vector<std::array<double,2>> path;
        for(int id=goal;;id=parent[id]){path.push_back(center(id));if(id==start)break;}
        std::reverse(path.begin(),path.end());return path;
    }
}
