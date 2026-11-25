#pragma once
#include "CraftInventory.hpp"

namespace kairo::player::detail
{
    struct CraftRecipe
    {
        std::array<std::uint32_t,4> Consume{},Produce{};
    };
    inline constexpr std::array<CraftRecipe,2> CraftRecipes{{
        CraftRecipe{{0,1,2,0},{0,0,0,2}},
        CraftRecipe{{0,0,4,0},{0,0,0,3}}
    }};
    // Apply all inputs and outputs to a local copy. Reject malformed
    // recipes, insufficient inputs and output overflow before mutation.
    inline bool CraftApplyRecipe(CraftInventory& inventory,const CraftRecipe& recipe)
    {
        if(!inventory.Valid())throw std::invalid_argument("Invalid Craft inventory.");
        bool input=false,output=false;
        auto next=inventory;
        for(unsigned i=0;i<4;++i)
        {
            if(recipe.Consume[i]>CraftInventory::StackLimit||recipe.Produce[i]>CraftInventory::StackLimit)
                throw std::invalid_argument("Recipe exceeds material stack bound.");
            input|=recipe.Consume[i]!=0;output|=recipe.Produce[i]!=0;
        }
        if(!input||!output)throw std::invalid_argument("Recipe needs inputs and outputs.");
        for(unsigned i=0;i<4;++i)
        {
            if(next.Counts[i]<recipe.Consume[i])return false;
            next.Counts[i]-=recipe.Consume[i];
            if(recipe.Produce[i]>CraftInventory::StackLimit-next.Counts[i])return false;
            next.Counts[i]+=recipe.Produce[i];
        }
        inventory=next;return true;
    }

    struct CraftHudRect
    {
        // Pixel coordinates in a fixed logical 960x540 reference canvas.
        // The native adapter maps to the actual perspective camera.
        float X,Y,W,H;
        std::array<float,3> Color;
        bool Background=false;
    };
    inline std::vector<CraftHudRect> BuildCraftHud(const CraftInventory& inventory,
        std::optional<std::uint32_t> health=std::nullopt,unsigned miningProgress=0,unsigned toolDurability=0,bool toolEquipped=false,std::optional<std::uint32_t> hunger=std::nullopt)
    {
        if(!inventory.Valid())throw std::invalid_argument("Invalid HUD inventory.");
        std::vector<CraftHudRect> rects;rects.reserve(128);
        const auto add=[&](float x,float y,float w,float h,std::array<float,3> color)
        {rects.push_back({x,y,w,h,color});};
        constexpr std::array<float,3> white{.9f,.9f,.9f},gray{.04f,.05f,.07f},selected{1.f,.72f,.22f};
        // Four nonoverlapping crosshair arms with an open center.
        add(469,269,8,2,white);add(483,269,8,2,white);
        add(479,259,2,8,white);add(479,273,2,8,white);
        constexpr std::array<unsigned,10> masks{0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
        const auto digit=[&](unsigned number,float x,float y)
        {
            constexpr std::array<std::array<float,4>,7> segments{{
                {{1,0,5,1}},{{6,1,1,5}},{{6,7,1,5}},{{1,12,5,1}},
                {{0,7,1,5}},{{0,1,1,5}},{{1,6,5,1}}}};
            for(unsigned i=0;i<7;++i)if(masks[number]&(1u<<i))
            {
                const auto s=segments[i];add(x+s[0],y+s[1],s[2],s[3],white);
            }
        };
        for(unsigned i=0;i<4;++i)
        {
            const float x=356.f+64.f*i,y=472.f;
            const bool active=inventory.Selected==i+1u;
            const auto border=active?selected:std::array<float,3>{.28f,.32f,.36f};
            // Inner fill is placed behind icons and numerals by the native adapter.
            add(x,y,56,3,border);add(x,y+53,56,3,border);
            add(x,y+3,3,50,border);add(x+53,y+3,3,50,border);
            add(x+3,y+3,50,50,gray);rects.back().Background=true;
            const auto color=CraftColor(static_cast<VoxelMaterial>(i+1u));
            add(x+8,y+8,22,22,color);
            auto count=inventory.Counts[i];
            if(count>=100)digit(count/100,x+30,y+35);
            if(count>=10)digit((count/10)%10,x+38,y+35);
            digit(count%10,x+46,y+35);
            digit(i+1u,x+39,y+9);
        }
        if(health)
        {
            if(*health>20)throw std::invalid_argument("Invalid HUD health.");
            add(356,455,248,10,gray);rects.back().Background=true;
            if(*health)add(358,457,244.f*static_cast<float>(*health)/20.f,6,{.85f,.12f,.15f});
        }
        if(miningProgress>20)throw std::invalid_argument("Invalid HUD mining progress.");
        if(miningProgress)
        {
            add(430,287,100,8,gray);rects.back().Background=true;
            add(432,289,96.f*static_cast<float>(miningProgress)/20.f,4,{.95f,.75f,.2f});
        }
        if(toolDurability>64||(toolEquipped&&!toolDurability))throw std::invalid_argument("Invalid HUD tool.");
        if(toolDurability)
        {
            add(356,438,248,10,gray);rects.back().Background=true;
            add(358,440,244.f*static_cast<float>(toolDurability)/64.f,6,
                toolEquipped?std::array<float,3>{.24f,.72f,.88f}:std::array<float,3>{.46f,.48f,.53f});
        }
        if(hunger)
        {
            if(*hunger>20)throw std::invalid_argument("Invalid HUD hunger.");
            add(356,421,248,10,gray);rects.back().Background=true;
            if(*hunger)add(358,423,244.f*static_cast<float>(*hunger)/20.f,6,{.85f,.57f,.18f});
        }
        return rects;
    }
}
