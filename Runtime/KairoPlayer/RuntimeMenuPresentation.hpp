#pragma once
#include "RuntimeMenuFocus.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace kairo::player::detail
{
    // One physical-pixel half-open rectangle is shared by drawing and hit
    // testing. Quantize both endpoints, not the origin and width separately:
    // adjacent fractional layouts therefore never overlap or leave a gap.
    struct MenuPixelBox
    {
        int X=0,Y=0,W=0,H=0;
        [[nodiscard]] bool Contains(double x,double y) const noexcept
        {return W>0&&H>0&&x>=X&&y>=Y&&x<X+W&&y<Y+H;}
        friend bool operator==(const MenuPixelBox&,const MenuPixelBox&)=default;
    };
    [[nodiscard]] inline MenuPixelBox QuantizeMenuBox(double x,double y,double w,double h,unsigned width,unsigned height)
    {
        if(!width||!height||width>32768||height>32768||!std::isfinite(x)||!std::isfinite(y)||
            !std::isfinite(w)||!std::isfinite(h)||w<0||h<0||!std::isfinite(x+w)||!std::isfinite(y+h))
            throw std::invalid_argument("Invalid menu pixel layout.");
        const auto pixel=[](double v,unsigned extent){return static_cast<int>(std::floor(std::clamp(v,0.,double(extent))));};
        const int left=pixel(x,width),top=pixel(y,height),right=pixel(x+w,width),bottom=pixel(y+h,height);
        return {left,top,right-left,bottom-top};
    }
    struct MenuVisualButton{MenuPixelBox Box;std::string Label;};
    struct MenuVisualRect{MenuPixelBox Box;std::array<float,3> Color;};
    inline constexpr std::size_t MenuOverlayLimit=4096;
    // Convert native logical coordinates before this function. Hit and draw
    // consume the same quantized boxes; empty subpixel buttons are inert.
    [[nodiscard]] inline std::optional<std::size_t> HitMenu(std::span<const MenuVisualButton> buttons,double x,double y) noexcept
    {
        for(std::size_t i=0;i<buttons.size();++i)if(buttons[i].Box.Contains(x,y))return i;
        return {};
    }
    // Bounded painter-order overlay. Reserve two background rectangles per
    // button, then share the remaining glyph budget. Merge adjacent lit pixels
    // within each glyph row; choose a prefix that fits at least one physical
    // pixel per glyph unit. Labels remain complete in the accessibility model.
    [[nodiscard]] inline std::vector<MenuVisualRect> BuildMenuOverlay(std::span<const MenuVisualButton> buttons,
        std::size_t focused,bool large)
    {
        if(buttons.empty()||buttons.size()>64||focused>=buttons.size())throw std::invalid_argument("Invalid menu visual buttons.");
        std::vector<MenuVisualRect> rects;
        const auto add=[&](MenuPixelBox box,std::array<float,3> color)
        {if(box.W>0&&box.H>0)rects.push_back({box,color});};
        // A five-bit row has at most three disjoint runs, seven rows per glyph.
        const auto glyphBudget=(MenuOverlayLimit-2*buttons.size())/(21*buttons.size());
        for(std::size_t i=0;i<buttons.size();++i)
        {
            const auto& b=buttons[i];const auto [x,y,w,h]=b.Box;
            if(w<=0||h<=0)continue;
            add(b.Box,i==focused?std::array<float,3>{1,.75f,.15f}:std::array<float,3>{.45f,.5f,.55f});
            add({x+3,y+3,w-6,h-6},{.035f,.045f,.065f});
            if(w<=8||h<=8)continue;
            const auto count=std::min({b.Label.size(),glyphBudget,static_cast<std::size_t>((w-8)/6)});
            if(!count)continue;
            const int scale=std::min({large?6:3,(w-8)/static_cast<int>(count*6),(h-8)/7});
            if(scale<1)continue;
            const int left=x+(w-static_cast<int>(count*6)*scale)/2,top=y+(h-7*scale)/2;
            for(std::size_t c=0;c<count;++c)
            {
                const auto rows=MenuGlyph(b.Label[c]);
                for(unsigned row=0;row<7;++row)
                    for(unsigned col=0;col<5;)
                    {
                        if(!(rows[row]&(1u<<(4-col)))){++col;continue;}
                        const auto start=col++;
                        while(col<5&&(rows[row]&(1u<<(4-col))))++col;
                        add({left+static_cast<int>(c*6+start)*scale,top+static_cast<int>(row)*scale,
                            static_cast<int>(col-start)*scale,scale},{1,1,1});
                    }
            }
        }
        return rects;
    }
}
