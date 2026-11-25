#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace kairo::player::detail
{
    // Conservative depth-only flow on a unit-area terrain grid. Each cell
    // stores bed elevation and nonnegative depth (meters). No momentum,
    // waves, viscosity, evaporation or open boundaries are modeled.
    class RuntimeWaterGrid
    {
    public:
        RuntimeWaterGrid(std::size_t width,std::size_t height)
            :m_Width(width),m_Height(height),m_Bed(CheckedSize(width,height)),m_Depth(m_Bed.size()){}
        [[nodiscard]] std::size_t Width() const noexcept{return m_Width;}
        [[nodiscard]] std::size_t Height() const noexcept{return m_Height;}
        // Bed edits displace the surface, not volume: depth stays constant.
        void SetBed(std::size_t x,std::size_t y,double elevation)
        {
            if(!std::isfinite(elevation)||std::abs(elevation)>10000)throw std::invalid_argument("Invalid bed height.");
            m_Bed[Index(x,y)]=elevation;
        }
        // External source or drain; reject negative depth or excess volume
        // before publishing the new cell value. Unit area means depth=volume.
        void AddWater(std::size_t x,std::size_t y,double volume)
        {
            const auto i=Index(x,y);const double next=m_Depth[i]+volume;
            if(!std::isfinite(next)||next<0||next>10000)throw std::invalid_argument("Invalid water volume.");
            m_Depth[i]=next;
        }
        [[nodiscard]] double Depth(std::size_t x,std::size_t y) const{return m_Depth[Index(x,y)];}
        [[nodiscard]] double Surface(std::size_t x,std::size_t y) const
        {const auto i=Index(x,y);return m_Bed[i]+m_Depth[i];}
        [[nodiscard]] double TotalVolume() const noexcept
        {double total=0;for(double d:m_Depth)total+=d;return total;}
        // Propose one directed flux per horizontal/vertical edge using the
        // free-surface height difference. Then scale all outgoing fluxes
        // from any donor so their sum cannot exceed its old water volume.
        // Apply equal/opposite transfers to a second buffer, never depend
        // on cell update order. Closed boundaries have no external flux.
        // factor is dimensionless relaxation [0,.25], not physical dt.
        void Step(double factor)
        {
            if(!std::isfinite(factor)||factor<0||factor>.25)throw std::invalid_argument("Invalid flow factor.");
            struct Flux{std::size_t From,To;double Amount;};
            std::vector<Flux> flux;flux.reserve(m_Depth.size()*2);
            std::vector<double> outgoing(m_Depth.size());
            const auto edge=[&](std::size_t a,std::size_t b)
            {
                const double delta=(m_Bed[a]+m_Depth[a])-(m_Bed[b]+m_Depth[b]);
                if(delta==0)return;
                const auto from=delta>0?a:b,to=delta>0?b:a;
                const double amount=std::min(m_Depth[from],std::abs(delta)*factor);
                if(amount>0){flux.push_back({from,to,amount});outgoing[from]+=amount;}
            };
            for(std::size_t y=0;y<m_Height;++y)for(std::size_t x=0;x<m_Width;++x)
            {
                const auto i=y*m_Width+x;
                if(x+1<m_Width)edge(i,i+1);
                if(y+1<m_Height)edge(i,i+m_Width);
            }
            auto next=m_Depth;
            for(const auto& f:flux)
            {
                const double scale=outgoing[f.From]>m_Depth[f.From]?m_Depth[f.From]/outgoing[f.From]:1.;
                const double amount=f.Amount*scale;
                next[f.From]-=amount;next[f.To]+=amount;
            }
            // Round-off may leave a tiny negative residual after transfers.
            // Do not silently clamp substantial invalid values or overflow.
            for(auto& d:next)
            {
                if(!std::isfinite(d)||d< -1e-10||d>10000)throw std::runtime_error("Water step exceeds bounds.");
                d=std::max(0.,d);
            }
            m_Depth.swap(next);
        }
    private:
        std::size_t m_Width,m_Height;
        std::vector<double> m_Bed,m_Depth;
        // Validate before multiplying dimensions or allocating any grid.
        static std::size_t CheckedSize(std::size_t w,std::size_t h)
        {
            if(!w||!h||w>128||h>128)throw std::invalid_argument("Water grid dimensions must be 1..128.");
            return w*h;
        }
        // Check each axis individually: x==width must not alias next row.
        [[nodiscard]] std::size_t Index(std::size_t x,std::size_t y) const
        {
            if(x>=m_Width||y>=m_Height)throw std::out_of_range("Water grid coordinate.");
            return y*m_Width+x;
        }
    };
}
