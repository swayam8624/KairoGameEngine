#include "World2DCampaignProject.hpp"
#include <iostream>
int main(int argc,char** argv)
{
    try
    {
        if((argc==3||argc==4)&&std::string_view(argv[1])=="--campaign")
        {
            const auto root=std::filesystem::canonical(argv[2]);
            std::optional<std::string> candidate;
            if(argc==4)candidate=kairo::player::world2d::CampaignRead(argv[3],8192);
            const auto campaign=kairo::player::world2d::LoadCampaignProject(root,
                candidate?std::optional<std::string_view>(*candidate):std::nullopt);
            std::cout<<"Validated "<<campaign.Worlds.size()<<" campaign levels\n";
            return 0;
        }
        if(argc==3&&std::string_view(argv[1])=="--ui")
        {
            const auto doc=kairo::player::ui2d::Parse(kairo::player::world2d::CampaignRead(argv[2],kairo::player::ui2d::MaxBytes));
            std::cout<<"Validated "<<doc.Widgets.size()<<" UI widgets\n";
            return 0;
        }
        if(argc==7&&std::string_view(argv[1])=="--vfx-preview")
        {
            using namespace kairo::player;
            const auto root=std::filesystem::canonical(argv[2]);
            const auto bytes=world2d::CampaignRead(argv[3],vfx2d::MaxBytes);
            const auto campaign=world2d::LoadCampaignProject(root,std::nullopt,std::nullopt,std::nullopt,bytes);
            const auto level=std::find(campaign.Config.Levels.begin(),campaign.Config.Levels.end(),argv[4]);
            vfx2d::Check(level!=campaign.Config.Levels.end(),"Unknown preview level.");
            const std::string_view event=argv[5];
            vfx2d::Check(event=="ambient"||event=="collect"||event=="death"||event=="complete","Unknown preview event.");
            std::size_t consumed=0;const std::string ticksText=argv[6];
            const auto ticks=std::stoul(ticksText,&consumed);
            vfx2d::Check(consumed==ticksText.size()&&ticks<=600,"Invalid preview tick count.");
            const world2d::Game game(campaign.Worlds.at(static_cast<std::size_t>(level-campaign.Config.Levels.begin())));
            const auto origin=[&](unsigned id)->std::optional<std::pair<int,int>>
            {
                for(const auto& actor:game.Definition().Actors)
                    if(actor.ID==id)return std::pair{actor.Bounds.X+actor.Bounds.W/2,actor.Bounds.Y+actor.Bounds.H/2};
                return {};
            };
            vfx2d::System effects(*campaign.VFX);effects.Begin(*level);
            if(event!="ambient")effects.Burst(event=="collect"?vfx2d::Event::Collect:event=="death"?vfx2d::Event::Death:vfx2d::Event::Complete,origin);
            for(unsigned i=0;i<ticks;++i)effects.Step(origin);
            const auto camera=game.Camera();std::vector<vfx2d::Rect> rects;
            effects.Paint(camera.X,camera.Y,camera.W,camera.H,640,360,rects);
            std::cout<<"vfx-preview "<<effects.Particles().size()<<' '<<effects.Dropped()<<' '<<rects.size()<<'\n';
            for(const auto& r:rects)std::cout<<r.X<<' '<<r.Y<<' '<<r.W<<' '<<r.H<<' '<<r.RGB<<'\n';
            return 0;
        }
        if(argc==4&&std::string_view(argv[1])=="--vfx")
        {
            const auto root=std::filesystem::canonical(argv[2]);
            const auto bytes=kairo::player::world2d::CampaignRead(argv[3],kairo::player::vfx2d::MaxBytes);
            const auto campaign=kairo::player::world2d::LoadCampaignProject(root,std::nullopt,std::nullopt,std::nullopt,bytes);
            std::cout<<"Validated "<<campaign.VFX->Materials.size()<<" particle materials and "<<campaign.VFX->Emitters.size()<<" emitters\n";
            return 0;
        }
        if(argc!=2)throw std::invalid_argument("Usage: KairoWorld2DTool <world.k2d> | --campaign <root> [candidate] | --ui <document.kui> | --vfx <root> <candidate> | --vfx-preview <root> <candidate> <level> <event> <ticks>");
        const auto world=kairo::player::world2d::Load(argv[1]);
        std::cout<<"Validated "<<world.Width<<"x"<<world.Height<<" world with "<<world.Actors.size()<<" actors\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
