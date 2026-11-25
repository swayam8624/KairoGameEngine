#include "../CraftRecipesHud.hpp"
#include <iostream>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
}
int main()
{
    CraftInventory inventory;
    Check(!CraftApplyRecipe(inventory,CraftRecipes[0]),"Crafted without inputs");
    inventory.Counts={0,1,2,998};
    const auto before=inventory.Counts;
    Check(!CraftApplyRecipe(inventory,CraftRecipes[0])&&inventory.Counts==before,"Overflow lost inputs");
    inventory.Counts={0,1,2,0};
    Check(CraftApplyRecipe(inventory,CraftRecipes[0])&&inventory.Counts==std::array<std::uint32_t,4>{0,0,0,2},"Recipe output mismatch");
    bool rejected=false;
    try{(void)CraftApplyRecipe(inventory,{});}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Empty recipe accepted");
    std::mt19937 rng(456);
    std::size_t rects=0;
    for(unsigned i=0;i<10000;++i)
    {
        for(auto& count:inventory.Counts)count=rng()%1000;
        inventory.Selected=static_cast<VoxelMaterial>(1+rng()%4);
        const auto& recipe=CraftRecipes[i%2];
        auto expected=inventory.Counts;
        bool possible=true;
        for(unsigned k=0;k<4;++k)
        {
            if(expected[k]<recipe.Consume[k]){possible=false;break;}
            expected[k]-=recipe.Consume[k];
            if(expected[k]+recipe.Produce[k]>999){possible=false;break;}
            expected[k]+=recipe.Produce[k];
        }
        const auto initial=inventory.Counts;
        Check(CraftApplyRecipe(inventory,recipe)==possible,"Recipe feasibility differs");
        Check(inventory.Counts==(possible?expected:initial),"Recipe rollback differs");
        const auto hud=BuildCraftHud(inventory);
        Check(hud.size()<=144&&hud.size()>=40,"HUD rectangle count");
        std::size_t backing=0,gold=0;
        for(const auto& r:hud)
        {
            Check(r.X>=0&&r.Y>=0&&r.W>0&&r.H>0&&r.X+r.W<=960&&r.Y+r.H<=540,"HUD geometry outside screen");
            backing+=r.Background;
            gold+=r.Color==std::array<float,3>{1.f,.72f,.22f};
        }
        Check(backing==4&&gold==4,"HUD selected slot or backgrounds incorrect");
        rects+=hud.size();
    }
    std::cout<<"10000 recipe transactions and "<<rects<<" HUD rectangles validated; overflow rollback and selected hotbar passed\n";
}
