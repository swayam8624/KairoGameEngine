#include "../World2DCampaignProject.hpp"
#include <chrono>
#include <iostream>
#include <system_error>
using namespace kairo::player::world2d;
void Check(bool ok){if(!ok)throw std::runtime_error("Campaign project regression");}
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Check(rejected);}
int main(int argc,char** argv)
{
    if(argc!=2)throw std::invalid_argument("Expected authored campaign sample");
    const auto source=std::filesystem::canonical(argv[1]);
    const auto root=std::filesystem::temp_directory_path()/("kairo-campaign-test-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if(!std::filesystem::create_directory(root))throw std::runtime_error("Cannot reserve temporary project");
    struct Cleanup{std::filesystem::path Path;~Cleanup(){std::error_code e;std::filesystem::remove_all(Path,e);}} cleanup{root};
    std::filesystem::copy(source/"Config",root/"Config",std::filesystem::copy_options::recursive);
    const auto campaign=LoadCampaignProject(root);
    Check(campaign.Worlds.size()==2&&campaign.Config.Levels[1]=="climb");
    auto progress=ReadCheckpoint(root,campaign);
    Check(progress.Level==0&&!progress.Finished);
    Game first(campaign.Worlds[0]);
    const auto go=[&](int x,int y)
    {
        for(int i=0;i<200;++i)
        {
            const auto b=first.Player();
            const int dx=(x>b.X+2)-(x<b.X-2),dy=(y>b.Y+2)-(y<b.Y-2);
            if(!dx&&!dy)break;
            first.Step({dx,dy,false});
        }
    };
    go(116,99);go(116,78);go(203,78);go(203,99);go(290,99);go(290,50);go(460,50);go(460,194);
    Check(first.Won()&&first.Collected()==2&&first.Deaths()==0);
    progress=Advance(progress,2);WriteCheckpoint(root,campaign,progress);
    Check(ReadCheckpoint(root,campaign).Level==1);
    Game second(campaign.Worlds[1]);
    for(int i=0;i<100&&second.Player().X<209;++i)second.Step({1,0,false});
    Check(second.Collected()==2);
    second.Step({1,0,true});
    for(int i=0;i<100&&!second.Won();++i)second.Step({1,0,false});
    Check(second.Won()&&second.Deaths()==0);
    progress=Advance(progress,2);WriteCheckpoint(root,campaign,progress);
    Check(ReadCheckpoint(root,campaign).Finished);
    // A different valid world revision may not resume the old progress.
    {std::ofstream out(root/"Config/Worlds/climb.k2d",std::ios::app);out<<"\n";}
    const auto revised=LoadCampaignProject(root);
    Check(revised.Revision!=campaign.Revision);
    Reject([&]{(void)ReadCheckpoint(root,revised);});
    const auto checkpoint=CheckpointPath(root);
    const auto previous=CampaignRead(checkpoint,256);
    auto lock=checkpoint;lock+=".write-lock";
    std::filesystem::create_directory(lock);
    Reject([&]{WriteCheckpoint(root,revised,{});});
    Check(CampaignRead(checkpoint,256)==previous);
    std::filesystem::remove(lock);
    WriteCheckpoint(root,revised,{});
    Check(ReadCheckpoint(root,revised).Level==0);
    std::filesystem::remove(root/"Config/Worlds/climb.k2d");
    Reject([&]{(void)LoadCampaignProject(root);});
    std::cout<<"Both authored levels completed; checkpoint round trip, stale revision, writer reservation and missing scene tests passed\n";
}
