#include "../World2DCampaignProject.hpp"
#include <iostream>
import Kairo.EngineCore.Entity;
import Kairo.EngineCore.LogicBytecode;
import Kairo.Foundation.Math.Vector;
#include "../World2DNativeLogic.hpp"
using namespace kairo::player;
void Assert(bool ok){if(!ok)throw std::runtime_error("Native world logic regression");}
int main(int argc,char** argv)
{
    Assert(argc==2);
    const auto campaign=world2d::LoadCampaignProject(argv[1]);
    Assert(campaign.Logic.has_value());
    world2d::Game game(campaign.Worlds.front());
    unsigned audio=0;
    world2d::WorldLogic logic(*campaign.Logic,[&](unsigned event){Assert(event==2);++audio;});
    using logic2d::Event;
    logic.Begin(game,"main");
    for(unsigned i=0;i<120;++i)logic.Tick(game);
    Assert(logic.Value()==2);
    logic.Dispatch(game,{Event::Collect});Assert(logic.Value()==12);
    logic.Dispatch(game,{Event::UI,"pause"});Assert(logic.Value(2)==1);
    logic.Dispatch(game,{Event::Death});Assert(logic.Value(1)==1);
    const auto index=game.Physics()->IDs.at(6);
    const auto before=game.Physics()->Scene.Bodies()[index].Velocity.Y;
    logic.Dispatch(game,{Event::Input,"Jump"});
    Assert(game.Physics()->Scene.Bodies()[index].Velocity.Y<before);
    logic.Dispatch(game,{Event::Complete});Assert(audio==1);
    game.Reset();logic.Begin(game,"main");Assert(logic.Value()==12);
    logic.NewCampaign();Assert(logic.Value()==0&&logic.Value(1)==0);
    // Verify that the emitted commands are actual shared EngineCore opcodes.
    const auto program=world2d::CompileWorldRule(campaign.Logic->Rules.at(4));
    Assert(program.Instructions.size()==4&&program.Instructions[2].Opcode==kairo::engine::LogicOpcode::ApplyEntityImpulse);
    auto bad=*campaign.Logic;bad.Rules[4].Actor=99999;
    bool rejected=false;try{world2d::ValidateLogicBindings(campaign,bad);}catch(const std::invalid_argument&){rejected=true;}Assert(rejected);
    std::cout<<"Shared EngineCore VM impulse, session counters, timers, input/UI/sound dispatch and invalid actor rejection passed\n";
}
