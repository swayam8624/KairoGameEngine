#include "../World2DCampaignProject.hpp"
#include "../World2DAtlas.hpp"
#include <chrono>
#include <iostream>
import Kairo.EngineCore.Entity;
import Kairo.EngineCore.LogicBytecode;
import Kairo.Foundation.Math.Vector;
#include "../World2DNativeLogic.hpp"
using namespace kairo::player;
using namespace kairo::player::world2d;
static void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv)
{
    Check(argc==2,"Expected Signal Vault project directory.");
    const auto root=std::filesystem::temp_directory_path()/("kairo-signal-vault-"+std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    Check(std::filesystem::create_directory(root),"Cannot reserve temporary directory.");
    struct Cleanup {std::filesystem::path Path;~Cleanup(){std::error_code e;std::filesystem::remove_all(Path,e);}} cleanup{root};
    std::filesystem::copy(std::filesystem::path(argv[1])/"Config",root/"Config",std::filesystem::copy_options::recursive);
    const auto campaign=LoadCampaignProject(root);
    Check(campaign.Worlds.size()==3&&campaign.UI&&campaign.Audio&&campaign.Logic&&campaign.VFX,"Incomplete authored campaign.");
    auto progress=ReadCheckpoint(root,campaign);
    Check(progress.Level==0&&!progress.Finished,"Fresh progress.");
    WorldLogic logic(*campaign.Logic,[](unsigned){});
    vfx2d::System effects(*campaign.VFX);
    Game death(campaign.Worlds.front());
    for(unsigned i=0;i<16;++i)death.Step({0,-1,false});
    // Deliberately route into the hazard from the starting column.
    for(unsigned i=0;i<12&&death.Deaths()==0;++i)death.Step({1,0,false});
    Check(death.Deaths()==1&&death.Collected()==0,"Hazard did not reset the level.");
    for(unsigned level=0;level<3;++level)
    {
        Game game(campaign.Worlds.at(level));
        const auto& name=campaign.Config.Levels.at(level);
        logic.Restore(progress.ScriptValues);logic.Begin(game,name);effects.Begin(name);
        const auto atlas=PrepareAtlas(game.Definition());
        Check(!atlas.Pixels.RGBA.empty()&&!atlas.Chunks.empty(),"Atlas data missing.");
        const auto origin=[&](unsigned id)->std::optional<std::pair<int,int>>
        {
            const auto& actors=game.Definition().Actors;
            for(std::size_t i=0;i<actors.size();++i)if(actors[i].ID==id)
            {
                const auto b=game.Actors().at(i).Bounds;
                return std::pair{b.X+b.W/2,b.Y+b.H/2};
            }
            return {};
        };
        unsigned ticks=0;
        const auto step=[&](Input input)
        {
            if(game.Won())return;
            Check(++ticks<=1000,"Level route exceeded tick budget.");
            const auto before=game.Collected();logic.Tick(game);game.Step(input);effects.Step(origin);
            Check(game.Deaths()==0,"Reference route hit hazard.");
            if(game.Collected()>before){logic.Dispatch(game,{logic2d::Event::Collect});effects.Burst(vfx2d::Event::Collect,origin);}
            if(game.Won()){logic.Dispatch(game,{logic2d::Event::Complete});effects.Burst(vfx2d::Event::Complete,origin);}
        };
        const auto go=[&](int x,int y)
        {
            for(int i=0;i<200&&!game.Won();++i)
            {
                const auto b=game.Player();
                const int dx=(x>b.X+2)-(x<b.X-2),dy=(y>b.Y+2)-(y<b.Y-2);
                if(!dx&&!dy)return;
                step({dx,dy,false});
            }
        };
        if(level==0){go(90,83);go(250,83);go(330,83);go(330,147);}
        else if(level==1)
        {
            for(int i=0;i<100&&game.Player().X<209;++i)step({1,0,false});
            Check(game.Collected()==2,"Relay bridge pickups.");
            step({1,0,true});
            for(int i=0;i<100&&!game.Won();++i)step({1,0,false});
        }
        else {go(110,35);go(162,35);go(162,130);go(330,145);}
        Check(game.Won()&&game.Collected()==2,"Level route failed.");
        Check(logic.Value()==int((level+1)*250),"Campaign score incorrect.");
        Check(!effects.Particles().empty(),"Completion effect missing.");
        progress=Advance(progress,3);progress.ScriptValues=logic.Values();
        WriteCheckpoint(root,campaign,progress);progress=ReadCheckpoint(root,campaign);
        Check(progress.ScriptValues[0]==int((level+1)*250),"Score was not persisted.");
        std::cout<<name<<": "<<ticks<<" ticks, score "<<logic.Value()<<'\n';
    }
    Check(progress.Finished&&progress.Level==2,"Campaign did not complete.");
    std::cout<<"Signal Vault three stages, hazard reset, VM score, VFX, atlas and checkpoint passed\n";
}
