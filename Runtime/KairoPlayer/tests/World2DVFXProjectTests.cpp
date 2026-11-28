#include "../World2DCampaignProject.hpp"
#include <iostream>
using namespace kairo::player;
void Assert(bool ok){if(!ok)throw std::runtime_error("VFX integration regression");}
int main(int argc,char** argv)
{
    Assert(argc==2);
    const auto root=std::filesystem::canonical(argv[1]);const auto campaign=world2d::LoadCampaignProject(root);
    Assert(campaign.VFX&&campaign.VFX->Materials.size()==3&&campaign.VFX->Emitters.size()==4);
    world2d::Game game(campaign.Worlds.front());
    vfx2d::System effects(*campaign.VFX);effects.Begin("main");
    const auto origin=[&](unsigned id)->std::optional<std::pair<int,int>>
    {
        const auto& actors=game.Definition().Actors;
        for(std::size_t i=0;i<actors.size();++i)if(actors[i].ID==id&&game.Actors()[i].Active)
        {
            const auto b=game.Actors()[i].Bounds;return std::pair{b.X+b.W/2,b.Y+b.H/2};
        }
        return {};
    };
    effects.Burst(vfx2d::Event::Collect,origin);Assert(effects.Particles().size()==24);
    for(unsigned i=0;i<6;++i){game.Step({});effects.Step(origin);}
    Assert(effects.Particles().size()==26);
    const auto pos=origin(8);Assert(pos.has_value());
    Assert(effects.Particles().back().X==pos->first*256&&effects.Particles().back().Y==pos->second*256);
    std::vector<vfx2d::Rect> rects;const auto c=game.Camera();
    effects.Paint(c.X,c.Y,c.W,c.H,1280,720,rects);Assert(!rects.empty());
    auto bad=*campaign.VFX;bad.Emitters.front().Actor=99999;
    bool rejected=false;try{world2d::ValidateVFXBindings(campaign,bad);}catch(const std::invalid_argument&){rejected=true;}Assert(rejected);
    auto candidate=world2d::CampaignRead(root/"Config/VFX.kvfx",vfx2d::MaxBytes);
    const auto at=candidate.find("fff4bd");Assert(at!=std::string::npos);candidate.replace(at,6,"ffffff");
    const auto changed=world2d::LoadCampaignProject(root,std::nullopt,std::nullopt,std::nullopt,candidate);
    Assert(changed.Revision==campaign.Revision&&changed.VFX->Materials.at("spark").Start==0xffffff);
    std::cout<<"Real World2D actor-following VFX, camera projection, invalid binding and cosmetic revision fixtures passed\n";
}
