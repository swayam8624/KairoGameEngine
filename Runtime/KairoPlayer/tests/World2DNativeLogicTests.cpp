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
    // Drive real sensor transitions through the physics scene, not a
    // synthetic event injection. The player is a kinematic participant.
    auto sensorWorld=campaign.Worlds.front();
    sensorWorld.Actors[7].Bounds=sensorWorld.Actors[0].Bounds;
    auto& sensor=sensorWorld.Physics->Bodies[1].Body;
    sensor.Type=kairo::physics2d::Motion::Static;sensor.Sensor=true;sensor.Velocity={};
    world2d::Game sensing(sensorWorld);
    const auto sensorDoc=logic2d::Parse("kairo-logic2d 1\nrules 2\nmain sensor_begin 8 add 0 1\nmain sensor_end 8 add 1 1\n");
    world2d::WorldLogic sensors(sensorDoc,[](unsigned){});
    sensors.Begin(sensing,"main");
    sensing.Step({});sensors.Sensors(sensing);Assert(sensors.Value()==1);
    for(unsigned i=0;i<12;++i){sensing.Step({1,0,false});sensors.Sensors(sensing);}
    Assert(sensors.Value()==1&&sensors.Value(1)==1);
    // VM position writes through the scene authority and rejects physics-
    // owned actors. Detach decor in this fixture before using position.
    auto decorWorld=campaign.Worlds.front();decorWorld.Physics->Bodies.pop_back();
    world2d::Game decor(decorWorld);
    auto positionDoc=logic2d::Parse("kairo-logic2d 1\nrules 1\nmain begin position 8 100 120\n");
    world2d::WorldLogic position(positionDoc,[](unsigned){});
    position.Begin(decor,"main");
    Assert(decor.Actors()[7].Bounds.X==100&&decor.Actors()[7].Bounds.Y==120);
    auto bad=*campaign.Logic;bad.Rules[4].Actor=99999;
    bool rejected=false;try{world2d::ValidateLogicBindings(campaign,bad);}catch(const std::invalid_argument&){rejected=true;}Assert(rejected);
    std::cout<<"Shared EngineCore VM impulse, session counters, timers, input/UI/sound dispatch and invalid actor rejection passed\n";
}
