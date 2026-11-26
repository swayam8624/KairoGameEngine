#pragma once
#include "RuntimeGridCanvas.hpp"
#include "RuntimePlatformer.hpp"
#include "RuntimeStrategy.hpp"
#include "RuntimeRpgSession.hpp"
#include "RuntimeMenuFocus.hpp"
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace kairo::player::detail
{
    using GameLabColor=std::array<float,3>;
    struct GameLabRect{GridPixelRect Box;GameLabColor Tint;};
    using GameLabRects=std::vector<GameLabRect>;
    // Renderer-neutral painter order: background, map, actors and HUD.
    // A tiny viewport can yield zero scale; do not submit empty rectangles.
    inline void AddGameRect(GameLabRects& rects,GridPixelRect r,GameLabColor color)
    {
        if(r.W>0&&r.H>0)rects.push_back({r,color});
    }
    // Bound text to the authored canvas width, even on narrow levels.
    inline void GameLabText(GameLabRects& rects,const RuntimeGridCanvas& canvas,int x,int y,int available,
        std::string_view value,GameLabColor color)
    {
        for(char c:value)
        {
            if(available<6)break;
            const auto glyph=MenuGlyph(c);
            for(unsigned row=0;row<7;++row)for(unsigned col=0;col<5;++col)
                if(glyph[row]&(1u<<(4-col)))AddGameRect(rects,canvas.Rect(x+int(col),y+int(row)),color);
            x+=6;available-=6;
        }
    }
    [[nodiscard]] inline GameLabRects PlatformerView(const RuntimePlatformer& game,unsigned w,unsigned h)
    {
        const auto& rows=game.Rows();
        const int columns=int(rows.front().size())*16,boardHeight=int(rows.size())*16;
        RuntimeGridCanvas canvas(w,h,columns,boardHeight+12);
        GameLabRects rects;AddGameRect(rects,canvas.Rect(0,0,columns,boardHeight+12),{.035f,.055f,.08f});
        for(int y=0;y<int(rows.size());++y)for(int x=0;x<int(rows[y].size());++x)
        {
            switch(rows[y][x])
            {
                case '#':AddGameRect(rects,canvas.Rect(x*16,y*16,16,16),{.23f,.31f,.42f});break;
                case 'H':AddGameRect(rects,canvas.Rect(x*16+1,y*16+3,14,13),{.95f,.15f,.12f});break;
                case 'C':AddGameRect(rects,canvas.Rect(x*16+5,y*16+5,6,6),{1,.75f,.15f});break;
                case 'G':AddGameRect(rects,canvas.Rect(x*16+2,y*16,12,16),{.12f,.65f,.35f});break;
                default:break;
            }
        }
        const auto& s=game.Current();
        AddGameRect(rects,canvas.Rect(s.X,s.Y,RuntimePlatformer::Width,RuntimePlatformer::Height),{.2f,.75f,1});
        const auto label=s.Won?std::string("LEVEL COMPLETE"):("COINS "+std::to_string(s.Coins)+"/"+std::to_string(game.CoinCount()));
        GameLabText(rects,canvas,4,boardHeight+2,columns-8,label,{1,1,1});return rects;
    }
    // The strategy host owns selection and cursor; the simulation owns worker
    // position, cargo and resources. Draw the exact current simulation values.
    [[nodiscard]] inline GameLabRects StrategyView(const RuntimeStrategy& game,std::size_t selected,StrategyCell cursor,unsigned w,unsigned h)
    {
        const auto& rows=game.Rows();
        const int columns=int(rows.front().size())*16,boardHeight=int(rows.size())*16;
        RuntimeGridCanvas canvas(w,h,columns,boardHeight+12);GameLabRects rects;
        AddGameRect(rects,canvas.Rect(0,0,columns,boardHeight+12),{.035f,.055f,.08f});
        for(int y=0;y<int(rows.size());++y)for(int x=0;x<int(rows[y].size());++x)
        {
            const auto c=rows[y][x];
            const GameLabColor color=c=='#'?GameLabColor{.22f,.27f,.33f}:c=='B'?GameLabColor{.1f,.55f,.8f}:
                c=='R'&&game.Stock({x,y})?GameLabColor{.8f,.55f,.1f}:GameLabColor{.12f,.17f,.15f};
            AddGameRect(rects,canvas.Rect(x*16,y*16,15,15),color);
        }
        for(std::size_t i=0;i<game.Units().size();++i)
        {
            const auto& u=game.Units()[i];const auto p=u.Position;
            AddGameRect(rects,canvas.Rect(p.X*16+2,p.Y*16+2,12,12),i==selected?GameLabColor{.3f,1,.6f}:GameLabColor{.7f,.85f,.9f});
            if(u.Cargo)AddGameRect(rects,canvas.Rect(p.X*16+3,p.Y*16+3,int(u.Cargo)*3,3),{1,.7f,.1f});
        }
        const int x=cursor.X*16,y=cursor.Y*16;
        AddGameRect(rects,canvas.Rect(x,y,16,1),{1,1,1});AddGameRect(rects,canvas.Rect(x,y+15,16,1),{1,1,1});
        AddGameRect(rects,canvas.Rect(x,y,1,16),{1,1,1});AddGameRect(rects,canvas.Rect(x+15,y,1,16),{1,1,1});
        const auto label=game.Won()?std::string("OBJECTIVE COMPLETE"):("STORED "+std::to_string(game.Stored())+"/"+std::to_string(game.Goal()));
        GameLabText(rects,canvas,3,boardHeight+2,columns-6,label,{1,1,1});return rects;
    }
    [[nodiscard]] inline GameLabRects QuestView(const QuestLab& q,unsigned w,unsigned h)
    {
        RuntimeGridCanvas canvas(w,h,180,100);GameLabRects rects;
        AddGameRect(rects,canvas.Rect(0,0,180,100),{.055f,.07f,.13f});
        if(const auto* node=q.Dialogue().Current())
        {
            GameLabText(rects,canvas,4,12,172,node->Text,{1,1,1});
            for(std::size_t i=0;i<node->Choices.size();++i)
            {
                const bool enabled=q.Available(i);
                AddGameRect(rects,canvas.Rect(4,28+int(i)*20,172,16),enabled?GameLabColor{.12f,.35f,.3f}:GameLabColor{.24f,.24f,.24f});
                GameLabText(rects,canvas,7,32+int(i)*20,166,std::to_string(i+1)+" "+node->Choices[i].Label,enabled?GameLabColor{1,1,1}:GameLabColor{.55f,.55f,.55f});
            }
            GameLabText(rects,canvas,4,83,172,"1 2 CHOOSE S SAVE L LOAD",{.75f,.85f,1});
        }
        else GameLabText(rects,canvas,4,40,172,q.Book().Won()?"QUEST COMPLETE":"CONVERSATION ENDED",{.3f,1,.6f});
        // Inventory is real session state, not an inferred objective label.
        GameLabText(rects,canvas,4,70,172,"CRYSTAL "+std::to_string(q.Bag().Count("crystal"))+" GOLD "+std::to_string(q.Bag().Count("gold")),{1,.75f,.2f});
        return rects;
    }
}
