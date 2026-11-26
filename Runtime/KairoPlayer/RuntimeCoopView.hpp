#pragma once
#include "RuntimeCoopArena.hpp"
#include "RuntimeGridCanvas.hpp"
#include "RuntimeMenuFocus.hpp"
#include <string>

namespace kairo::player::detail
{
    struct CoopRect{GridPixelRect Box;std::array<float,3> Color;friend bool operator==(const CoopRect&,const CoopRect&)=default;};
    // A pure view of the last authoritative snapshot; no local prediction
    // or private simulation can move a rendered actor. Color is supplemented
    // by a white local-player border and a visible status label.
    [[nodiscard]] inline std::vector<CoopRect> CoopView(CoopState state,unsigned slot,bool busy,unsigned w,unsigned h)
    {
        if(slot>1)throw std::invalid_argument("Invalid co-op seat.");
        (void)DecodeCoopState(EncodeCoopState(state),state.Tick);
        RuntimeGridCanvas canvas(w,h,144,140);std::vector<CoopRect> out;
        using Color=std::array<float,3>;
        constexpr std::array<Color,2> colors{{{.22f,.74f,1.f},{1.f,.65f,.22f}}};
        const auto add=[&](int x,int y,int width,int height,Color color)
        {const auto box=canvas.Rect(x,y,width,height);if(box.W&&box.H)out.push_back({box,color});};
        const auto label=[&](int x,int y,std::string_view text,Color color)
        {
            for(char c:text)
            {
                if(x+5>144)break;
                const auto glyph=MenuGlyph(c);
                for(int row=0;row<7;++row)for(int col=0;col<5;++col)
                    if(glyph[static_cast<unsigned>(row)]&(1u<<(4-col)))add(x+col,y+row,1,1,color);
                x+=6;
            }
        };
        add(0,0,144,140,{.035f,.05f,.08f});
        for(int y=0;y<7;++y)for(int x=0;x<9;++x)
            add(x*16,y*16,15,15,CoopWalkable({x,y})?Color{.12f,.17f,.21f}:Color{.26f,.32f,.4f});
        for(unsigned i=0;i<2;++i)
        {
            const auto goal=CoopGoals[i];add(goal.X*16+1,goal.Y*16+1,13,13,colors[i]);
            add(goal.X*16+3,goal.Y*16+3,9,9,{.08f,.12f,.17f});
        }
        for(unsigned i=0;i<2;++i)
        {
            const auto p=state.Actors[i];
            if(i==slot)add(p.X*16+3,p.Y*16+3,10,10,{1,1,1});
            add(p.X*16+4,p.Y*16+4,8,8,colors[i]);
        }
        label(3,115,"P"+std::to_string(slot+1)+" TURN "+std::to_string(state.Tick),colors[slot]);
        label(3,129,state.Won()?"BOTH AT EXIT":state.Tick==CoopTickLimit?"TURN LIMIT":busy?"WAITING FOR PEER":"ARROWS SPACE WAIT",{.85f,.9f,1});
        return out;
    }
}
