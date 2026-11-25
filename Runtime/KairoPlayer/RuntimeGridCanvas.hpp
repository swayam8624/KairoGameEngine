#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace kairo::player::detail
{
    // Integer physical-pixel canvas. Fit an authored subcell grid while
    // preserving square units and centering the result. No GPU dependency.
    struct GridPixelRect{int X,Y,W,H;friend bool operator==(GridPixelRect,GridPixelRect)=default;};
    class RuntimeGridCanvas
    {
    public:
        RuntimeGridCanvas(unsigned width,unsigned height,int columns,int rows)
            :m_Columns(columns),m_Rows(rows)
        {
            if(!width||!height||width>32768||height>32768||columns<1||rows<1||columns>1024||rows>1024)
                throw std::invalid_argument("Invalid grid canvas.");
            m_Scale=std::min(int(width)/columns,int(height)/rows);
            m_X=(int(width)-columns*m_Scale)/2;m_Y=(int(height)-rows*m_Scale)/2;
        }
        [[nodiscard]] int Scale() const noexcept{return m_Scale;}
        // A zero scale means the viewport cannot show one pixel per unit.
        // Return an empty rectangle rather than a singular or negative size.
        [[nodiscard]] GridPixelRect Rect(int x,int y,int w=1,int h=1) const
        {
            if(x<0||y<0||w<1||h<1||x>=m_Columns||y>=m_Rows||w>m_Columns-x||h>m_Rows-y)
                throw std::out_of_range("Grid rectangle outside canvas.");
            return {m_X+x*m_Scale,m_Y+y*m_Scale,w*m_Scale,h*m_Scale};
        }
        // Inverse layout for pointer picking; reject letterbox pixels.
        [[nodiscard]] std::optional<GridPixelRect> Hit(int x,int y) const noexcept
        {
            if(!m_Scale||x<m_X||y<m_Y||x>=m_X+m_Columns*m_Scale||y>=m_Y+m_Rows*m_Scale)return {};
            return GridPixelRect{(x-m_X)/m_Scale,(y-m_Y)/m_Scale,1,1};
        }
    private:
        int m_Columns,m_Rows,m_Scale=0,m_X=0,m_Y=0;
    };
}
