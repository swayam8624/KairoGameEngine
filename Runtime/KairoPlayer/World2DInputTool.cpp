#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>
import Kairo.EngineCore.InputMap;
int main(int argc,char** argv)
{
    try
    {
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
