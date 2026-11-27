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
        if(argc!=2)throw std::invalid_argument("Usage: KairoWorld2DTool <world.k2d> | --campaign <root> [candidate]");
        const auto world=kairo::player::world2d::Load(argv[1]);
        std::cout<<"Validated "<<world.Width<<"x"<<world.Height<<" world with "<<world.Actors.size()<<" actors\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
