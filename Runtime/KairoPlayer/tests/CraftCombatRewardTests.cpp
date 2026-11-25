#include "../CraftCombatReward.hpp"
#include <iostream>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    const auto flat=[](VoxelCoordinate c)->std::optional<VoxelMaterial>{return c.Y<0?1:0;};
    const VoxelPlayer player{{.5,0,.5}};
    std::mt19937 rng(562);
    for(unsigned trial=0;trial<1000;++trial)
    {
        CraftCreature creature;
        std::array<std::uint32_t,4> counts{static_cast<std::uint32_t>(rng()%1000),5,static_cast<std::uint32_t>(trial%4==0?999:trial%4==1?998:rng()%998),3};
        const auto before=counts;
        Check(!CraftStrikeReward(creature,counts).Hit&&counts==before,"Absent creature paid reward");
        for(unsigned i=0;i<60;++i)(void)creature.Advance(.05,player,flat);
        Check(creature.Body().has_value(),"Creature missing");
        auto malformed=counts;malformed[0]=1000;
        bool rejected=false;try{(void)CraftStrikeReward(creature,malformed);}catch(const std::invalid_argument&){rejected=true;}
        Check(rejected&&creature.Health()==12&&malformed[0]==1000,"Invalid inventory mutated creature");
        for(unsigned hit=0;hit<3;++hit)
        {
            const auto result=CraftStrikeReward(creature,counts);
            Check(result.Hit&&result.Defeated==(hit==2),"Hit or defeat result differs");
            Check(result.StoneAwarded==(hit==2?std::min(2u,999u-before[2]):0u),"Reward differs");
            const auto snapshot=counts;
            Check(!CraftStrikeReward(creature,counts).Hit&&counts==snapshot,"Cooldown/death paid extra reward");
            if(hit<2)for(unsigned i=0;i<10;++i)(void)creature.Advance(.05,player,flat);
        }
        Check(!creature.Body()&&counts[0]==before[0]&&counts[1]==before[1]&&counts[2]==std::min(999u,before[2]+2)&&counts[3]==before[3],"Final reward conservation");
        for(unsigned i=0;i<100;++i)Check(!CraftStrikeReward(creature,counts).Hit,"Repeated dead strike accepted");
    }
    std::cout<<"1000 combat reward journeys and 100000 repeated dead strikes passed; stack limits and invalid inventory rollback verified\n";
}
