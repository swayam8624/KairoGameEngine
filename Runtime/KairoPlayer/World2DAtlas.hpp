#pragma once
#include "RuntimeWorld2D.hpp"
#include <cmath>
#include <cstddef>

namespace kairo::player::world2d
{
    // One texel gutter per frame, one palette row, at most 1024 frames.
    // Fixed cells make packing independent of import order and animation time.
    struct AtlasRegion {unsigned X=0,Y=0,W=0,H=0;friend bool operator==(AtlasRegion,AtlasRegion)=default;};
    struct Atlas
    {
        unsigned Width=0,Height=0;
        std::vector<std::uint8_t> RGBA;
        std::array<AtlasRegion,16> Palette{};
        std::map<std::string,std::vector<AtlasRegion>> Frames;
    };
    inline constexpr unsigned AtlasCell=34,AtlasColumns=32,ChunkTiles=16;
    inline Atlas BuildAtlas(const World& world)
    {
        Require(!world.Sprites.empty()&&world.Sprites.size()<=MaxSprites,"Invalid atlas sprite count.");
        std::size_t frames=0;
        for(const auto& [name,sprite]:world.Sprites)
        {
            Require(Name(name)&&sprite.Width>=1&&sprite.Width<=32&&sprite.Height>=1&&sprite.Height<=32&&
                !sprite.Frames.empty()&&sprite.Frames.size()<=16,"Invalid atlas sprite.");
            frames+=sprite.Frames.size();
        }
        Atlas atlas;
        atlas.Width=AtlasCell*static_cast<unsigned>(std::min<std::size_t>(AtlasColumns,frames));
        atlas.Height=1+AtlasCell*static_cast<unsigned>((frames+AtlasColumns-1)/AtlasColumns);
        atlas.RGBA.resize(static_cast<std::size_t>(atlas.Width)*atlas.Height*4);
        const auto put=[&](unsigned x,unsigned y,unsigned color,bool opaque)
        {
            const auto i=(static_cast<std::size_t>(y)*atlas.Width+x)*4;
            atlas.RGBA[i]=static_cast<std::uint8_t>(color>>16);
            atlas.RGBA[i+1]=static_cast<std::uint8_t>(color>>8);
            atlas.RGBA[i+2]=static_cast<std::uint8_t>(color);
            atlas.RGBA[i+3]=opaque?255:0;
        };
        for(unsigned i=0;i<16;++i)
        {
            atlas.Palette[i]={i,0,1,1};
            put(i,0,world.Colors[i],true);
        }
        unsigned slot=0;
        for(const auto& [name,sprite]:world.Sprites)
        {
            auto& regions=atlas.Frames[name];
            for(const auto& frame:sprite.Frames)
            {
                Require(frame.size()==static_cast<std::size_t>(sprite.Width*sprite.Height),"Invalid atlas frame size.");
                const AtlasRegion region{(slot%AtlasColumns)*AtlasCell+1,
                    2+(slot/AtlasColumns)*AtlasCell,static_cast<unsigned>(sprite.Width),static_cast<unsigned>(sprite.Height)};
                ++slot;
                regions.push_back(region);
                // Replicate border texels, including transparent pixels. The
                // padded region is never sampled as an authored sprite.
                for(int y=-1;y<=sprite.Height;++y)for(int x=-1;x<=sprite.Width;++x)
                {
                    const int index=Hex(frame[std::clamp(y,0,sprite.Height-1)*sprite.Width+
                        std::clamp(x,0,sprite.Width-1)]);
                    Require(index>=0,"Invalid atlas frame pixel.");
                    put(static_cast<unsigned>(int(region.X)+x),static_cast<unsigned>(int(region.Y)+y),
                        world.Colors[static_cast<unsigned>(index)],index!=0);
                }
            }
        }
        return atlas;
    }

    struct AtlasQuad {Box Bounds;AtlasRegion Region;};
    struct TileChunk {Box Bounds;bool Background=false;std::vector<AtlasQuad> Quads;};
    inline std::vector<TileChunk> BuildTileChunks(const World& w,const Atlas& atlas)
    {
        std::vector<TileChunk> chunks;
        for(bool background:{true,false})
        {
            const auto& rows=background?w.Back:w.Map;
            for(int y=0;y<w.Height;y+=ChunkTiles)for(int x=0;x<w.Width;x+=ChunkTiles)
            {
                const int endX=std::min(w.Width,x+int(ChunkTiles)),endY=std::min(w.Height,y+int(ChunkTiles));
                TileChunk chunk{{x*Tile,y*Tile,(endX-x)*Tile,(endY-y)*Tile},background,{}};
                for(int row=y;row<endY;++row)for(int col=x;col<endX;)
                {
                    if(rows[row][col]=='.'){++col;continue;}
                    const auto color=w.Tiles.at(rows[row][col]).Color;
                    int end=col+1;
                    while(end<endX&&rows[row][end]!='.'&&w.Tiles.at(rows[row][end]).Color==color)++end;
                    chunk.Quads.push_back({{col*Tile,row*Tile,(end-col)*Tile,Tile},atlas.Palette.at(color)});
                    col=end;
                }
                if(!chunk.Quads.empty())chunks.push_back(std::move(chunk));
            }
        }
        return chunks;
    }

    struct SpriteDraw {std::size_t ActorIndex=0,Frame=0;Box Bounds;};
    struct AtlasFrame
    {
        Box Camera,BackgroundCamera;
        std::vector<std::size_t> Chunks;
        std::vector<SpriteDraw> Sprites;
    };
    inline void BuildAtlasFrame(const Game& game,const std::vector<TileChunk>& chunks,AtlasFrame& frame)
    {
        frame.Camera=game.Camera();frame.BackgroundCamera=frame.Camera;
        const auto& w=game.Definition();
        frame.BackgroundCamera.X=frame.Camera.X*w.Parallax/100;
        frame.BackgroundCamera.Y=frame.Camera.Y*w.Parallax/100;
        frame.Chunks.clear();frame.Sprites.clear();
        for(std::size_t i=0;i<chunks.size();++i)
            if(Overlap(chunks[i].Bounds,chunks[i].Background?frame.BackgroundCamera:frame.Camera))
                frame.Chunks.push_back(i);
        for(std::size_t i=0;i<game.Actors().size();++i)
        {
            const auto& actor=game.Actors()[i];
            if(!actor.Active||!Overlap(actor.Bounds,frame.Camera))continue;
            const auto& sprite=w.Sprites.at(w.Actors[i].SpriteName);
            frame.Sprites.push_back({i,(game.Tick()/static_cast<unsigned>(sprite.Period))%sprite.Frames.size(),actor.Bounds});
        }
        std::sort(frame.Sprites.begin(),frame.Sprites.end(),[&](const SpriteDraw& a,const SpriteDraw& b)
        {
            const bool da=w.Actors[a.ActorIndex].Type==Kind::Decor,db=w.Actors[b.ActorIndex].Type==Kind::Decor;
            if(da!=db)return da;
            const int za=a.Bounds.Y+a.Bounds.H,zb=b.Bounds.Y+b.Bounds.H;
            return za==zb?a.ActorIndex<b.ActorIndex:za<zb;
        });
    }
}
