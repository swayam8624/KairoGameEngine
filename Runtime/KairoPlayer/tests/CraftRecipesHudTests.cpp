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
    const auto empty=BuildCraftHud(inventory,0),half=BuildCraftHud(inventory,10),full=BuildCraftHud(inventory,20);
    Check(half.size()==empty.size()+1&&full.size()==half.size(),"Health bar geometry count");
    Check(empty.back().Background&&half.back().W==122.f&&full.back().W==244.f,"Health bar fill differs");
    bool badHealth=false;
    try{(void)BuildCraftHud(inventory,21);}catch(const std::invalid_argument&){badHealth=true;}
    Check(badHealth,"Invalid health bar accepted");
    const auto mining=BuildCraftHud(inventory,20,10);
    Check(mining.size()==full.size()+2&&mining.back().W==48.f,"Mining progress geometry differs");
    Check(mining[mining.size()-2].Background,"Mining bar backing missing");
    bool badMining=false;
    try{(void)BuildCraftHud(inventory,20,21);}catch(const std::invalid_argument&){badMining=true;}
    Check(badMining,"Invalid mining progress accepted");
    const auto tool=BuildCraftHud(inventory,20,0,32,true);
    Check(tool.size()==full.size()+2&&tool.back().W==122.f&&tool[tool.size()-2].Background,"Tool durability HUD differs");
    bool badTool=false;
    try{(void)BuildCraftHud(inventory,20,0,0,true);}catch(const std::invalid_argument&){badTool=true;}
    Check(badTool,"Broken tool HUD accepted");
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
