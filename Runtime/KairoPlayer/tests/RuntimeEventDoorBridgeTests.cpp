#include <cmath>
#include <iostream>
#include <stdexcept>
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeDoorBridge;

void Check(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
int main(int argc,char** argv)
{
    if(argc!=2)return 2;
    try
    {
        kairo::player::RuntimeProject project(argv[1]);
        kairo::player::RuntimePhysicsBridge physics(project.Scene());
        const kairo::engine::Entity player{1},gate{40};
        kairo::player::RuntimeDoorBridge doors(project,physics,player);
        using Command=kairo::player::RuntimeDoorCommand;
        const auto body=physics.BodyFor(gate);
        Check(body&&doors.HasDoor(gate)&&!doors.HasDoor(player),"Door binding.");
        const auto blocked=[&]{const auto hit=physics.Raycast({0,1.25f,-2},{0,0,1},3);return hit&&hit->first==gate;};
        const auto ticks=[&]{for(int i=0;i<180;++i)doors.BeforePhysicsStep(1.f/60);};
        ticks();Check(blocked(),"Locked gate opened near player.");
        doors.Command(gate,Command::Open);
        ticks();Check(!blocked()&&physics.BodyFor(gate)==body,"Open command did not clear collider.");
        doors.Command(gate,Command::Lock);
        ticks();Check(!blocked(),"Lock closed near player.");
        physics.SetEntityPosition(player,{6,1,3});
        ticks();Check(blocked()&&physics.BodyFor(gate)==body,"Lock did not close at clearance.");
        doors.Command(gate,Command::Automatic);
        physics.SetEntityPosition(player,{0,.92,3.4});
        ticks();Check(!blocked(),"Automatic command did not restore proximity opening.");
        const auto p=project.Scene().WorldTransform(gate).Translation;
        Check(std::abs(p.y-4.45f)<.02f,"Scene position differs.");
        bool rejected=false;try{doors.Command(player,Command::Open);}catch(const std::invalid_argument&){rejected=true;}
        Check(rejected,"Invalid door command accepted.");
        std::cout<<"Native door commands, collider and clearance transitions passed\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
