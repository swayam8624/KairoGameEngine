#pragma once
#include "RuntimeWorld2D.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace kairo::player::world2d
{
    struct DrawRect {Box Bounds;unsigned RGB=0;};
    inline constexpr std::size_t MaxDrawRects=4096;
    // Clip each primitive before mapping it to the letterboxed viewport.
    // Two tile layers plus 128 four-by-four sprites stay below 4096 draws.
    inline std::vector<DrawRect> View(const Game& game,unsigned width,unsigned height)
    {
        std::vector<DrawRect> out;
        if(!width||!height||width>32768||height>32768)return out;
        const auto& w=game.Definition();const Box camera=game.Camera();
        const double scale=std::min(double(width)/camera.W,double(height)/camera.H);
        const int screenW=static_cast<int>(camera.W*scale),screenH=static_cast<int>(camera.H*scale);
        if(!screenW||!screenH)return out;
        const int ox=(static_cast<int>(width)-screenW)/2,oy=(static_cast<int>(height)-screenH)/2;
        const auto draw=[&](Box b,unsigned color)
        {
            const int left=std::clamp(b.X,0,camera.W),right=std::clamp(b.X+b.W,0,camera.W);
            const int top=std::clamp(b.Y,0,camera.H),bottom=std::clamp(b.Y+b.H,0,camera.H);
            const int x=ox+left*screenW/camera.W,y=oy+top*screenH/camera.H;
            const int r=ox+right*screenW/camera.W,d=oy+bottom*screenH/camera.H;
            if(r>x&&d>y)out.push_back({{x,y,r-x,d-y},color});
        };
        draw({0,0,camera.W,camera.H},w.Colors[0]);
        const auto layer=[&](const std::vector<std::string>& rows,int cx,int cy)
        {
            const int endX=std::min(w.Width,(cx+camera.W+Tile-1)/Tile);
            const int endY=std::min(w.Height,(cy+camera.H+Tile-1)/Tile);
            for(int y=cy/Tile;y<endY;++y)for(int x=cx/Tile;x<endX;++x)
            {
                const char c=rows[y][x];if(c=='.')continue;
                draw({x*Tile-cx,y*Tile-cy,Tile,Tile},w.Colors[w.Tiles.at(c).Color]);
            }
        };
        layer(w.Back,camera.X*w.Parallax/100,camera.Y*w.Parallax/100);
        layer(w.Map,camera.X,camera.Y);
        std::vector<std::size_t> order;
        for(std::size_t i=0;i<game.Actors().size();++i)
            if(game.Actors()[i].Active&&Overlap(game.Actors()[i].Bounds,camera))order.push_back(i);
        std::stable_sort(order.begin(),order.end(),[&](auto a,auto b)
        {
            const bool da=w.Actors[a].Type==Kind::Decor,db=w.Actors[b].Type==Kind::Decor;
            if(da!=db)return da;
            const auto p=game.Actors()[a].Bounds,q=game.Actors()[b].Bounds;
            return p.Y+p.H<q.Y+q.H;
        });
        for(const auto i:order)
        {
            const auto& actor=w.Actors[i];const auto& sprite=w.Sprites.at(actor.SpriteName);
            const auto& frame=sprite.Frames[(game.Tick()/static_cast<unsigned>(sprite.Period))%sprite.Frames.size()];
            const auto b=game.Actors()[i].Bounds;
            for(int y=0;y<4;++y)
                for(int x=0;x<4;)
                {
                    const int color=Hex(frame[y*4+x]);
                    int end=x+1;
                    while(end<4&&Hex(frame[y*4+end])==color)++end;
                    if(color)
                        draw({b.X-camera.X+x*b.W/4,b.Y-camera.Y+y*b.H/4,
                            end*b.W/4-x*b.W/4,(y+1)*b.H/4-y*b.H/4},w.Colors[static_cast<std::size_t>(color)]);
                    x=end;
                }
        }
        // Small authored-objective HUD, independent of scrolling camera.
        draw({4,4,48,6},w.Colors[8]);
        const int filled=game.Total()?int(48u*game.Collected()/game.Total()):48;
        if(filled)draw({4,4,filled,6},w.Colors[4]);
        if(game.Won())draw({camera.W/2-24,camera.H/2-5,48,10},w.Colors[3]);
        if(out.size()>MaxDrawRects)throw std::logic_error("2D view exceeded native rectangle budget.");
        return out;
    }
}
