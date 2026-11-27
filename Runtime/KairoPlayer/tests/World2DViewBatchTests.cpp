#include "../RuntimeWorld2DView.hpp"
#include <iostream>
#include <random>

using namespace kairo::player::world2d;
void Check(bool ok){if(!ok)throw std::runtime_error("View batching regression");}
std::vector<unsigned> Raster(const std::vector<DrawRect>& rects,unsigned w,unsigned h)
{
    std::vector<unsigned> pixels(w*h,0xabcdef);
    for(const auto& item:rects)
    {
        const auto b=item.Bounds;
        Check(b.X>=0&&b.Y>=0&&b.X+b.W<=int(w)&&b.Y+b.H<=int(h));
        for(int y=b.Y;y<b.Y+b.H;++y)
            for(int x=b.X;x<b.X+b.W;++x)pixels[y*w+x]=item.RGB;
    }
    return pixels;
}
// Independent original one-rectangle-per-tile reference; fixture sprites
// are entirely transparent, so only the two layers and HUD are visible.
std::vector<DrawRect> Reference(const Game& game,unsigned width,unsigned height)
{
    std::vector<DrawRect> out;
    const auto& w=game.Definition();const auto c=game.Camera();
    const double scale=std::min(double(width)/c.W,double(height)/c.H);
    const int sw=int(c.W*scale),sh=int(c.H*scale);
    if(!sw||!sh)return out;
    const int ox=(int(width)-sw)/2,oy=(int(height)-sh)/2;
    const auto draw=[&](Box b,unsigned rgb)
    {
        const int x=ox+std::clamp(b.X,0,c.W)*sw/c.W;
        const int y=oy+std::clamp(b.Y,0,c.H)*sh/c.H;
        const int r=ox+std::clamp(b.X+b.W,0,c.W)*sw/c.W;
        const int d=oy+std::clamp(b.Y+b.H,0,c.H)*sh/c.H;
        if(r>x&&d>y)out.push_back({{x,y,r-x,d-y},rgb});
    };
    draw({0,0,c.W,c.H},w.Colors[0]);
    const auto layer=[&](const auto& rows,int cx,int cy)
    {
        for(int y=cy/Tile;y<std::min(w.Height,(cy+c.H+Tile-1)/Tile);++y)
            for(int x=cx/Tile;x<std::min(w.Width,(cx+c.W+Tile-1)/Tile);++x)
                if(rows[y][x]!='.')draw({x*Tile-cx,y*Tile-cy,Tile,Tile},w.Colors[w.Tiles.at(rows[y][x]).Color]);
    };
    layer(w.Back,c.X*w.Parallax/100,c.Y*w.Parallax/100);
    layer(w.Map,c.X,c.Y);
    draw({4,4,48,6},w.Colors[8]);
    const int filled=game.Total()?int(48u*game.Collected()/game.Total()):48;
    if(filled)draw({4,4,filled,6},w.Colors[4]);
    if(game.Won())draw({c.W/2-24,c.H/2-5,48,10},w.Colors[3]);
    return out;
}
int main(int argc,char** argv)
{
    Check(argc==2);
    auto world=Load(argv[1]);
    for(auto& [name,sprite]:world.Sprites)
        for(auto& frame:sprite.Frames)std::fill(frame.begin(),frame.end(),'0');
    for(auto& [name,tile]:world.Tiles)tile.Solid=false;
    for(auto& row:world.Back)std::fill(row.begin(),row.end(),'~');
    for(auto& row:world.Map)std::fill(row.begin(),row.end(),'#');
    Game dense(world);
    std::vector<DrawRect> buffer;buffer.reserve(MaxDrawRects);
    const auto capacity=buffer.capacity();
    BuildView(dense,512,384,buffer);
    const auto reference=Reference(dense,512,384);
    Check(Raster(buffer,512,384)==Raster(reference,512,384));
    Check(buffer.size()*8<reference.size());
    std::cout<<"Dense tile fixture: "<<reference.size()<<" -> "<<buffer.size()<<" rectangles\n";
    std::mt19937 rng(2026);
    std::uint64_t before=0,after=0;
    for(unsigned i=0;i<1000;++i)
    {
        for(auto* rows:{&world.Back,&world.Map})
            for(auto& row:*rows)for(auto& cell:row)cell="..##~~"[rng()%6];
        world.Parallax=int(rng()%101);
        world.Actors[0].Bounds.X=int(rng()%(world.Width*Tile-12));
        world.Actors[0].Bounds.Y=int(rng()%(world.Height*Tile-12));
        Game game(world);
        const unsigned w=32+rng()%256,h=32+rng()%256;
        BuildView(game,w,h,buffer);
        const auto baseline=Reference(game,w,h);
        Check(buffer.capacity()==capacity);
        Check(Raster(buffer,w,h)==Raster(baseline,w,h));
        Check(buffer.size()<=baseline.size());
        before+=baseline.size();after+=buffer.size();
    }
    std::cout<<"1000 randomized pixel-identical tile views: "<<before<<" -> "<<after<<" rectangles\n";
}
