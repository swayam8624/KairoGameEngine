#pragma once
#include "RuntimeWorld2DView.hpp"
#include <span>
#include <optional>
#include <cmath>

namespace kairo::player::world2d
{
    enum class CaptureTransfer {Linear,Gamma22};
    struct CaptureEvidence {unsigned Probes=0,Matches=0;};
    // Compare interior pixels, not the exact edge rasterization convention.
    // Reference rectangles remain independent of the atlas and mesh path.
    inline CaptureEvidence CheckAtlasCapture(const Game& game,unsigned width,unsigned height,
        std::span<const std::uint8_t> rgba,CaptureTransfer transfer=CaptureTransfer::Linear,
        std::span<const DrawRect> overlay={},bool hud=true)
    {
        Require(width&&height&&width<=32768&&height<=32768&&
            rgba.size()==std::uint64_t(width)*height*4,"Invalid atlas capture dimensions.");
        std::vector<DrawRect> rects;
        BuildView(game,width,height,rects,hud);
        rects.insert(rects.end(),overlay.begin(),overlay.end());
        const auto colorAt=[&](int x,int y)->std::optional<unsigned>
        {
            for(auto i=rects.rbegin();i!=rects.rend();++i)
            {
                const auto b=i->Bounds;
                if(x>=b.X&&x<b.X+b.W&&y>=b.Y&&y<b.Y+b.H)return i->RGB;
            }
            return {};
        };
        CaptureEvidence evidence;
        const auto probe=[&](int x,int y)
        {
            if(x<2||y<2||x+2>=int(width)||y+2>=int(height))return;
            const auto expected=colorAt(x,y);
            if(!expected)return;
            for(int dy:{-2,0,2})for(int dx:{-2,0,2})
                if(colorAt(x+dx,y+dy)!=expected)return;
            ++evidence.Probes;
            const auto index=(std::size_t(y)*width+std::size_t(x))*4;
            bool matches=true;
            for(unsigned channel=0;channel<3;++channel)
            {
                const auto linear=double((*expected>>(16-8*channel))&255)/255.;
                const auto encoded=transfer==CaptureTransfer::Gamma22?std::pow(linear,1./2.2):linear;
                matches&=std::abs(int(rgba[index+channel])-int(std::lround(encoded*255.)))<=3;
            }
            if(matches)++evidence.Matches;
        };
        // Uniformly cover the camera plus the centers of every expected
        // authored tile run, sprite run and HUD rectangle.
        for(unsigned y=1;y<13;++y)for(unsigned x=1;x<17;++x)
            probe(int(x*width/17),int(y*height/13));
        for(const auto& rect:rects)
            probe(rect.Bounds.X+rect.Bounds.W/2,rect.Bounds.Y+rect.Bounds.H/2);
        return evidence;
    }
}
