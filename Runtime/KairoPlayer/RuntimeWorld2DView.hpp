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
    // Two bounded tile layers plus the validated sprite-run budget stay below 4096 draws.
    inline void BuildView(const Game& game,unsigned width,unsigned height,std::vector<DrawRect>& out,bool hud=true)
    {
        out.clear();
        if(!width||!height||width>32768||height>32768)return;
        const auto& w=game.Definition();const Box camera=game.Camera();
        const double scale=std::min(double(width)/camera.W,double(height)/camera.H);
        const int screenW=static_cast<int>(camera.W*scale),screenH=static_cast<int>(camera.H*scale);
        if(!screenW||!screenH)return;
        const int ox=(static_cast<int>(width)-screenW)/2,oy=(static_cast<int>(height)-screenH)/2;
        const auto drawFractional=[&](double bx,double by,double bw,double bh,unsigned color)
        {
            const auto left=std::clamp(bx,0.,double(camera.W)),right=std::clamp(bx+bw,0.,double(camera.W));
            const auto top=std::clamp(by,0.,double(camera.H)),bottom=std::clamp(by+bh,0.,double(camera.H));
            const int x=ox+int(left*screenW/camera.W),y=oy+int(top*screenH/camera.H);
            const int r=ox+int(right*screenW/camera.W),d=oy+int(bottom*screenH/camera.H);
            if(r>x&&d>y)out.push_back({{x,y,r-x,d-y},color});
        };
        const auto draw=[&](Box b,unsigned color){drawFractional(b.X,b.Y,b.W,b.H,color);};
        draw({0,0,camera.W,camera.H},w.Colors[0]);
        const auto layer=[&](const std::vector<std::string>& rows,int cx,int cy)
        {
            const int endX=std::min(w.Width,(cx+camera.W+Tile-1)/Tile);
            const int endY=std::min(w.Height,(cy+camera.H+Tile-1)/Tile);
            for(int y=cy/Tile;y<endY;++y)for(int x=cx/Tile;x<endX;)
            {
                const char c=rows[y][x];
                if(c=='.'){++x;continue;}
                const auto color=w.Tiles.at(c).Color;
                int end=x+1;
                while(end<endX&&rows[y][end]!='.'&&w.Tiles.at(rows[y][end]).Color==color)++end;
                // Adjacent opaque tiles with the same color have identical
                // coverage when rendered as one rectangle.
                draw({x*Tile-cx,y*Tile-cy,(end-x)*Tile,Tile},w.Colors[color]);
                x=end;
            }
        };
        layer(w.Back,camera.X*w.Parallax/100,camera.Y*w.Parallax/100);
        layer(w.Map,camera.X,camera.Y);
        std::array<std::size_t,MaxActors> order{};
        std::size_t count=0;
        for(std::size_t i=0;i<game.Actors().size();++i)
            if(game.Actors()[i].Active&&Overlap(game.Actors()[i].Bounds,camera))order[count++]=i;
        std::sort(order.begin(),order.begin()+count,[&](auto a,auto b)
        {
            const bool da=w.Actors[a].Type==Kind::Decor,db=w.Actors[b].Type==Kind::Decor;
            if(da!=db)return da;
            const auto p=game.Actors()[a].Bounds,q=game.Actors()[b].Bounds;
            const auto depthA=p.Y+p.H,depthB=q.Y+q.H;
            return depthA==depthB?a<b:depthA<depthB;
        });
        for(std::size_t slot=0;slot<count;++slot)
        {
            const auto i=order[slot];
            const auto& actor=w.Actors[i];const auto& sprite=w.Sprites.at(actor.SpriteName);
            const auto& frame=sprite.Frames[(game.Tick()/static_cast<unsigned>(sprite.Period))%sprite.Frames.size()];
            const auto b=game.Actors()[i].Bounds;
            const double px=double(b.W)/sprite.Width,py=double(b.H)/sprite.Height;
            for(int y=0;y<sprite.Height;++y)
                for(int x=0;x<sprite.Width;)
                {
                    const int color=Hex(frame[y*sprite.Width+x]);
                    int end=x+1;
                    while(end<sprite.Width&&Hex(frame[y*sprite.Width+end])==color)++end;
                    if(color)
                        drawFractional(b.X-camera.X+x*px,b.Y-camera.Y+y*py,
                            (end-x)*px,py,w.Colors[static_cast<std::size_t>(color)]);
                    x=end;
                }
        }
        // Small authored-objective HUD, independent of scrolling camera.
        if(hud){
        draw({4,4,48,6},w.Colors[8]);
        const int filled=game.Total()?int(48u*game.Collected()/game.Total()):48;
        if(filled)draw({4,4,filled,6},w.Colors[4]);
        if(game.Won())draw({camera.W/2-24,camera.H/2-5,48,10},w.Colors[3]);
        }
        if(out.size()>MaxDrawRects)throw std::logic_error("2D view exceeded native rectangle budget.");
    }
    inline std::vector<DrawRect> View(const Game& game,unsigned width,unsigned height)
    {
        std::vector<DrawRect> result;
        BuildView(game,width,height,result);
        return result;
    }
}
