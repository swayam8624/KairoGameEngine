#include "../CraftCombatReward.hpp"
#include <iostream>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    // Exercise the generic pool independently of the renderer: distance,
    // occlusion, partial transfer, repeat collection, expiry and capacity.
    RuntimePickupPool<2> pool;
    Check(pool.TrySpawn({1e9,0,1e9},3,5,2)&&pool.TrySpawn({0,0,0},1,7,5),"Pickup spawn failed");
    Check(!pool.TrySpawn({1,0,0},2,1),"Pickup cap was not enforced");
    unsigned credit=0,room=3;
    auto receive=[&](std::uint32_t kind,std::uint32_t count){
        Check(kind==3,"Collected distant item");auto n=std::min(room,count);room-=n;credit+=n;return n;};
    const auto visible=[](const auto&,const auto&){return true;};
    pool.Collect({1e9,0,1e9},2.,[](const auto&,const auto&){return false;},receive);
    Check(credit==0,"Occluded pickup was collected");
    pool.Collect({1e9,0,1e9},2.,visible,receive);
    Check(credit==3&&pool.Items()[0]->Count==2,"Partial transfer lost items");
    pool.Collect({1e9,0,1e9},2.,visible,receive);
    Check(credit==3,"Full inventory duplicated pickup");
    room=10;pool.Collect({1e9,0,1e9},2.,visible,receive);
    Check(credit==5&&!pool.Items()[0],"Final transfer failed");
    pool.Advance(5.);
    Check(!pool.Items()[1]&&pool.TrySpawn({1,0,0},2,1),"Expiry failed to free slot");
    std::mt19937_64 pickupRng(421);
    for(unsigned trial=0;trial<10000;++trial)
    {
        RuntimePickupPool<2> randomPool;
        const unsigned count=1+static_cast<unsigned>(pickupRng()%1000);
        unsigned space=static_cast<unsigned>(pickupRng()%count),credited=0;
        Check(randomPool.TrySpawn({1e9,0,1e9},3,count,2)&&randomPool.TrySpawn({0,0,0},1,7,5),"Random pool spawn failed");
        Check(!randomPool.TrySpawn({1,0,0},2,1),"Random pool cap failed");
        auto transfer=[&](unsigned kind,unsigned offered){
            Check(kind==3,"Distant item credited");
            const auto n=std::min(space,offered);space-=n;credited+=n;return n;};
        randomPool.Collect({1e9,0,1e9},2,[](const auto&,const auto&){return false;},transfer);
        Check(credited==0,"Occluded random item collected");
        for(unsigned step=0;step<2;++step)
        {
            randomPool.Collect({1e9,0,1e9},2,visible,transfer);
            Check(randomPool.Items()[0]&&randomPool.Items()[0]->Count+credited==count,"Random pickup conservation failed");
        }
        space=count;randomPool.Collect({1e9,0,1e9},2,visible,transfer);
        Check(credited==count&&!randomPool.Items()[0],"Random pickup final transfer failed");
        randomPool.Advance(5);Check(!randomPool.Items()[1],"Random pickup expiration failed");
    }
    std::cout<<"10000 randomized pickup conservation journeys passed\n";
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
    for(unsigned trial=0;trial<128;++trial)
    {
        CraftCreature mob;CraftInventory inventory;CraftPickups loot;
        for(unsigned i=0;i<60;++i)(void)mob.Advance(.05,player,flat);
        for(unsigned hit=0;hit<3;++hit)
        {
            const auto result=CraftStrikeDrop(mob,inventory,loot);
            Check(result.Hit&&result.Defeated==(hit==2)&&result.StoneAwarded==0,"Drop strike differs");
            if(hit<2)for(unsigned i=0;i<10;++i)(void)mob.Advance(.05,player,flat);
        }
        Check(!mob.Body()&&inventory.Count(CraftStone)==0&&loot.Items()[0]&&loot.Items()[0]->Count==2,"Death did not produce exactly two world items");
        const auto center=loot.Items()[0]->Center;
        const auto unknown=[](VoxelCoordinate)->std::optional<VoxelMaterial>{return {};};
        CollectCraftPickups(loot,inventory,center,unknown);
        Check(inventory.Count(CraftStone)==0,"Unknown cell allowed collection");
        inventory.Counts[CraftStone-1]=998;
        CollectCraftPickups(loot,inventory,center,flat);
        Check(inventory.Count(CraftStone)==999&&loot.Items()[0]->Count==1,"Partial loot collect differs");
        --inventory.Counts[CraftStone-1];
        CollectCraftPickups(loot,inventory,center,flat);
        Check(inventory.Count(CraftStone)==999&&!loot.Items()[0],"Loot remainder lost");
        Check(!CraftStrikeDrop(mob,inventory,loot).Hit,"Dead creature paid extra loot");
    }
    std::cout<<"128 drop journeys and bounded pickup pool passed\n";
    std::cout<<"1000 combat reward journeys and 100000 repeated dead strikes passed; stack limits and invalid inventory rollback verified\n";
}
