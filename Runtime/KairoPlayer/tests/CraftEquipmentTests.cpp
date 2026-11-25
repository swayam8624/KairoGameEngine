#include "../CraftEquipment.hpp"
#include "../CraftMining.hpp"
#include <iostream>
#include <random>

namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
int main()
{
    CraftEquipment tool;
    std::array<std::uint32_t,4> counts{0,2,3,0};
    Check(tool.Craft(counts)&&counts==std::array<std::uint32_t,4>{}&&tool.Equipped&&tool.Durability==64,"Craft transaction");
    Check(!tool.Craft(counts)&&tool.Speed(3)==2&&tool.Speed(1)==1,"Equipment speed or duplicate crafting");
    CraftMining mining;
    const CraftMiningTarget stone{{1,2,3},3};
    for(unsigned i=0;i<7;++i)Check(!mining.Advance(true,stone,.1,[](const auto&){return true;},tool.Speed(3)),"Early tool completion");
    Check(!mining.Advance(true,stone,.05,[](const auto&){return false;},tool.Speed(3))&&tool.Durability==64,"Failed mining wore tool");
    for(unsigned i=0;i<64;++i)
    {
        bool done=false;
        for(unsigned j=0;j<8;++j)
            done=mining.Advance(true,stone,.1,[&](const auto&){tool.SuccessfulMine(3);return true;},tool.Speed(3));
        Check(done&&tool.Durability==63-i&&tool.Equipped==(i!=63),"Tool wear or break");
    }
    Check(tool.Speed(3)==1&&!tool.Toggle(),"Broken tool active");
    counts={0,2,3,0};Check(tool.Craft(counts),"Recraft failed");
    tool.SuccessfulMine(1);Check(tool.Durability==64,"Grass wore pick");
    for(unsigned i=0;i<7;++i)(void)mining.Advance(true,stone,.1,[](const auto&){return true;},2);
    Check(tool.Toggle()&&!mining.Advance(true,stone,.1,[](const auto&){return true;},tool.Speed(3))&&mining.Progress()==1,"Speed change carried progress");
    std::mt19937 rng(920);
    for(unsigned i=0;i<100000;++i)
    {
        CraftEquipment e;
        std::array<std::uint32_t,4> inventory{};
        for(auto& n:inventory)n=rng()%10;
        const auto before=inventory;
        const bool possible=before[1]>=2&&before[2]>=3;
        Check(e.Craft(inventory)==possible,"Craft feasibility");
        auto expected=before;
        if(possible){expected[1]-=2;expected[2]-=3;}
        Check(inventory==expected&&e.Valid(),"Craft conservation");
        const auto mat=static_cast<std::uint16_t>(1+rng()%4);
        const bool use=e.Speed(mat)>1;
        const auto durability=e.Durability;
        e.SuccessfulMine(mat);
        Check(e.Durability==durability-(use?1u:0u)&&e.Valid(),"Wear reference mismatch");
    }
    std::cout<<"100000 equipment transactions, 64-use breakage, speed change and failed edit passed\n";
}
