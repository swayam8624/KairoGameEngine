#include "../RuntimeProjectileFlight.hpp"
#include <iostream>
#include <limits>
#include <random>
using namespace kairo::player::detail;
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main()
{
    RuntimeProjectileFlight pool;
    Check(pool.Launch({0,0,0},{2,0,0},10,60),"launch");
    unsigned impacts=0;auto impact=[&](std::uint32_t id){Check(id==7,"wrong hit");++impacts;};
    auto plane=[](const FlightVector& p,const FlightVector& d,double dist)->std::optional<std::uint32_t>{
        Check(d[0]==1,"direction not normalized");
        if(p[0]<=3.5&&p[0]+dist>=3.5)return 7;
        return {};
    };
    for(int i=0;i<3;++i)pool.Step(1./60,plane,impact);
    Check(pool.Size()==1&&impacts==0,"premature impact");
    pool.Step(1./60,plane,impact);Check(!pool.Size()&&impacts==1,"segment missed thin obstacle");
    Check(pool.Launch({0,0,0},{1,0,0},3,60),"short launch");
    for(int i=0;i<5;++i)pool.Step(1./60,plane,impact);
    Check(!pool.Size()&&impacts==1,"range exceeded");
    for(std::size_t i=0;i<pool.Capacity;++i)Check(pool.Launch({0,0,0},{1,0,0},1,1),"capacity early");
    Check(!pool.Launch({0,0,0},{1,0,0},1,1),"capacity exceeded");
    bool invalid=false;try{pool.Step(0,plane,impact);}catch(const std::invalid_argument&){invalid=true;}
    Check(invalid&&pool.Size()==pool.Capacity,"bad timestep changed state");pool.Clear();
    invalid=false;try{pool.Launch({0,0,0},{0,0,0},1,1);}catch(const std::invalid_argument&){invalid=true;}
    Check(invalid&&!pool.Size(),"zero direction accepted");
    std::mt19937 rng(93);std::uniform_real_distribution<double> coord(.1,99.9),speed(1,500);
    for(int trial=0;trial<10000;++trial)
    {
        const auto target=coord(rng),range=coord(rng),v=speed(rng);unsigned hits=0;
        Check(pool.Launch({0,0,0},{1,0,0},range,v),"random launch");
        auto query=[&](const FlightVector& p,const FlightVector&,double dist)->std::optional<std::uint32_t>{
            Check(dist>0&&p[0]+dist<=range+1e-9,"range budget");
            if(p[0]<=target&&p[0]+dist>=target)return 1;
            return {};
        };
        while(pool.Size())pool.Step(.25,query,[&](auto id){Check(id==1,"random id");++hits;});
        Check(hits==unsigned(target<=range),"random collision parity");
    }
    // A collider removed by the first impact must not block the next shot.
    Check(pool.Launch({0,0,0},{1,0,0},5,100)&&pool.Launch({0,0,0},{1,0,0},5,100),"batch launch");
    bool wall=true;std::vector<unsigned> order;
    pool.Step(.1,[&](const auto&,const auto&,double)->std::optional<std::uint32_t>{return wall?1:2;},
        [&](unsigned id){order.push_back(id);wall=false;});
    Check(order==std::vector<unsigned>{1,2}&&!pool.Size(),"impact order");
    std::cout<<"10000 seeded flight journeys and boundary tests passed\n";
}
