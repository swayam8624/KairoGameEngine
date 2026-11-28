#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "World2DCampaignProject.hpp"
import Kairo.EngineCore.InputMap;
import Kairo.EngineCore.Entity;
import Kairo.EngineCore.LogicBytecode;
import Kairo.Foundation.Math.Vector;
import Kairo.Player.RuntimeProject;
#include "World2DNativeLogic.hpp"
int main(int argc,char** argv)
{
    try
    {
        if(argc==4&&std::string_view(argv[1])=="--logic")
        {
            const auto root=std::filesystem::canonical(argv[2]);
            const kairo::player::RuntimeProject project(root/"Project.kproject");
            const auto candidate=kairo::player::world2d::CampaignRead(argv[3],kairo::player::logic2d::MaxBytes);
            const auto campaign=kairo::player::world2d::LoadCampaignProject(root,std::nullopt,std::nullopt,candidate);
            kairo::player::world2d::ValidateWorldLogicInputs(*campaign.Logic,project.InputMap());
            kairo::player::world2d::WorldLogic compiled(*campaign.Logic,[](unsigned){});
            std::cout<<"Validated "<<campaign.Logic->Rules.size()<<" logic rules\n";return 0;
        }
        if(argc!=2)throw std::invalid_argument("Usage: KairoWorld2DInputTool <Input.kinput>");
        const auto map=kairo::engine::LoadInputActionMap(std::filesystem::path(argv[1]));
        const auto* move=map.FindAction("Move");
        if(!move||move->Type!=kairo::engine::InputActionValueType::Axis2D)
            throw std::invalid_argument("World2D requires a Move axis2d action.");
        for(const auto* name:{"Jump","Quit","Pause","Restart","NewGame"})
            if(const auto* action=map.FindAction(name);
                action&&action->Type!=kairo::engine::InputActionValueType::Button)
                throw std::invalid_argument("World2D optional controls must be button actions.");
        std::cout<<"Validated World2D input actions\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
