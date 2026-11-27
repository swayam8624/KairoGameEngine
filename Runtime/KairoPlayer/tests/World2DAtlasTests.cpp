#include "../World2DAtlas.hpp"
#include "../World2DAtlasCapture.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::world2d;
void Check(bool ok){if(!ok)throw std::runtime_error("Atlas regression");}
unsigned Pixel(const Atlas& a,unsigned x,unsigned y)
{
    Check(x<a.Width&&y<a.Height);
    const auto i=(std::size_t(y)*a.Width+x)*4;
    return unsigned(a.RGBA[i])<<24|unsigned(a.RGBA[i+1])<<16|unsigned(a.RGBA[i+2])<<8|a.RGBA[i+3];
}
void Verify(const World& world,const Atlas& a)
{
    Check(a.Width<=1088&&a.Height<=1089&&a.RGBA.size()==std::size_t(a.Width)*a.Height*4);
    for(unsigned i=0;i<16;++i)Check(Pixel(a,a.Palette[i].X,a.Palette[i].Y)==(world.Colors[i]<<8|255));
    std::vector<bool> occupied(std::size_t(a.Width)*a.Height);
    for(const auto& [name,sprite]:world.Sprites)
    {
        const auto& frames=a.Frames.at(name);Check(frames.size()==sprite.Frames.size());
        for(std::size_t i=0;i<frames.size();++i)
        {
            const auto r=frames[i];Check(r.W==unsigned(sprite.Width)&&r.H==unsigned(sprite.Height)&&r.X&&r.Y>1);
            for(int y=-1;y<=sprite.Height;++y)for(int x=-1;x<=sprite.Width;++x)
            {
                const auto ax=unsigned(int(r.X)+x),ay=unsigned(int(r.Y)+y);
                const auto at=std::size_t(ay)*a.Width+ax;
                Check(at<occupied.size()&&!occupied[at]);occupied[at]=true;
                const int c=Hex(sprite.Frames[i][std::clamp(y,0,sprite.Height-1)*sprite.Width+std::clamp(x,0,sprite.Width-1)]);
                Check(Pixel(a,ax,ay)==(world.Colors.at(static_cast<std::size_t>(c))<<8|(c?255:0)));
            }
        }
    }
}
void VerifyTiles(const World& world,const Atlas& atlas,const std::vector<TileChunk>& chunks)
{
    for(bool back:{true,false})
    {
        std::vector<int> pixels(world.Width*world.Height,-1);
        for(const auto& chunk:chunks)if(chunk.Background==back)
        {
            Check(chunk.Quads.size()<=256);
            for(const auto& q:chunk.Quads)
            {
                Check(q.Bounds.X>=chunk.Bounds.X&&q.Bounds.Y>=chunk.Bounds.Y&&
                    q.Bounds.X+q.Bounds.W<=chunk.Bounds.X+chunk.Bounds.W&&q.Bounds.Y+q.Bounds.H<=chunk.Bounds.Y+chunk.Bounds.H);
                for(int x=q.Bounds.X/Tile;x<(q.Bounds.X+q.Bounds.W)/Tile;++x)
                {
                    auto& p=pixels[q.Bounds.Y/Tile*world.Width+x];Check(p==-1);p=int(q.Region.X);
                }
                Check(q.Region==atlas.Palette[q.Region.X]);
            }
        }
        const auto& rows=back?world.Back:world.Map;
        for(int y=0;y<world.Height;++y)for(int x=0;x<world.Width;++x)
            Check(pixels[y*world.Width+x]==(rows[y][x]=='.'?-1:int(world.Tiles.at(rows[y][x]).Color)));
    }
}
int main(int argc,char** argv)
{
    Check(argc==2);
    auto world=Load(argv[1]);auto atlas=BuildAtlas(world);Verify(world,atlas);
    auto chunks=BuildTileChunks(world,atlas);VerifyTiles(world,atlas,chunks);
    Game game(world);AtlasFrame frame;std::mt19937 rng(2026);
    unsigned maxDraws=0;std::uint64_t rectangles=0,draws=0;
    for(unsigned i=0;i<10000;++i)
    {
        game.Step({int(rng()%3)-1,int(rng()%3)-1,false});
        BuildAtlasFrame(game,chunks,frame);
        Check(frame.Sprites.size()<=MaxActors&&frame.Camera==game.Camera());
        for(auto c:frame.Chunks)Check(Overlap(chunks[c].Bounds,chunks[c].Background?frame.BackgroundCamera:frame.Camera));
        for(std::size_t j=0;j<frame.Sprites.size();++j)
        {
            const auto& s=frame.Sprites[j];const auto& definition=world.Sprites.at(world.Actors[s.ActorIndex].SpriteName);
            Check(game.Actors()[s.ActorIndex].Active&&Overlap(s.Bounds,frame.Camera));
            Check(s.Frame==(game.Tick()/unsigned(definition.Period))%definition.Frames.size());
            if(j){const auto& p=frame.Sprites[j-1];if(world.Actors[p.ActorIndex].Type!=Kind::Decor)Check(world.Actors[s.ActorIndex].Type!=Kind::Decor);}
        }
        const unsigned n=unsigned(frame.Chunks.size()+frame.Sprites.size());maxDraws=std::max(maxDraws,n);
        draws+=n;rectangles+=View(game,1280,720).size();
    }
    std::cout<<"10000 frame plans: "<<rectangles<<" old rectangles vs "<<draws<<" textured scene records; peak "<<maxDraws<<" records\n";
    // Independent reference image, then intentional color corruption.
    Game start(world);constexpr unsigned w=1280,h=720;
    std::vector<std::uint8_t> pixels(std::size_t(w)*h*4);
    for(const auto& r:View(start,w,h))for(int y=r.Bounds.Y;y<r.Bounds.Y+r.Bounds.H;++y)for(int x=r.Bounds.X;x<r.Bounds.X+r.Bounds.W;++x)
    {
        const auto i=(std::size_t(y)*w+x)*4;
        pixels[i]=std::uint8_t(r.RGB>>16);pixels[i+1]=std::uint8_t(r.RGB>>8);pixels[i+2]=std::uint8_t(r.RGB);pixels[i+3]=255;
    }
    const auto good=CheckAtlasCapture(start,w,h,pixels);
    Check(good.Probes>=8&&good.Probes==good.Matches);
    for(std::size_t i=0;i<pixels.size();++i)if(i%4!=3)
        pixels[i]=static_cast<std::uint8_t>(std::lround(std::pow(double(pixels[i])/255.,1./2.2)*255.));
    const auto gamma=CheckAtlasCapture(start,w,h,pixels,CaptureTransfer::Gamma22);
    Check(gamma.Matches==gamma.Probes&&gamma.Probes==good.Probes);
    std::fill(pixels.begin(),pixels.end(),0);const auto bad=CheckAtlasCapture(start,w,h,pixels);
    Check(bad.Matches<bad.Probes/2);
    std::cout<<"Capture reference: "<<good.Matches<<"/"<<good.Probes<<"; corrupted "<<bad.Matches<<"/"<<bad.Probes<<"\n";
    // Maximum atlas capacity and non-overlapping padded frame regions.
    world.Sprites.clear();
    for(unsigned s=0;s<64;++s)
    {
        Sprite sprite; sprite.Width=sprite.Height=32;
        for(unsigned f=0;f<16;++f)
        {
            std::string image(1024,'0');for(auto& p:image)p="0123456789abcdef"[rng()%16];
            sprite.Frames.push_back(std::move(image));
        }
        world.Sprites.emplace("sprite_"+std::to_string(s),std::move(sprite));
    }
    atlas=BuildAtlas(world);Verify(world,atlas);
    Check(atlas.Width==1088&&atlas.Height==1089);
    std::cout<<"Maximum atlas: "<<atlas.RGBA.size()<<" bytes, 1024 padded frames checked\n";
}
