#include <cmath>
#include <iostream>
#include <stdexcept>

import Kairo.EngineCore;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeDoorBridge;

namespace engine=kairo::engine;
namespace player=kairo::player;

void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}

class DoorStep final:public player::RuntimeFixedStepListener
{
    player::RuntimeDoorBridge& m_Doors;
public:
    explicit DoorStep(player::RuntimeDoorBridge& doors):m_Doors(doors){}
    void BeforePhysicsStep(float dt) override{m_Doors.BeforePhysicsStep(dt);}
};

int main(int argc,char** argv)
{
    if(argc!=2)return 2;
    try
    {
        player::RuntimeProject project(argv[1]);
        player::RuntimePhysicsBridge physics(project.Scene());
        const engine::Entity controlled{1},gate{40};
        player::RuntimeDoorBridge doors(project,physics,controlled);
        Check(doors.Size()==1,"Expected exactly one authored gate.");
        const auto body=physics.BodyFor(gate);
        Check(body.has_value(),"Gate body absent.");
        const auto blocked=[&]{
            const auto hit=physics.Raycast({0,1.25f,-2},{0,0,1},3);
            return hit&&hit->first==gate;
        };
        Check(blocked(),"Closed gate collider did not block ray.");
        DoorStep listener(doors);
        for(int i=0;i<180;++i)(void)physics.Advance(1.f/60,&listener);
        Check(physics.BodyFor(gate)==body,"Opening changed body identity.");
        Check(!blocked(),"Open gate collider still blocks passage.");
        const auto opened=project.Scene().WorldTransform(gate).Translation;
        Check(std::abs(opened.y-4.45f)<.02f,"Open rendered pose incorrect.");
        Check(std::abs(physics.World().Bodies().at(*body).State.Position.y-opened.y)<.02f,
            "Open scene and collider pose disagree.");
        physics.SetEntityPosition(controlled,{6,1,3});
        for(int i=0;i<180;++i)(void)physics.Advance(1.f/60,&listener);
        Check(physics.BodyFor(gate)==body&&blocked(),"Closing did not restore original collider.");
        Check(std::abs(project.Scene().WorldTransform(gate).Translation.y-1.25f)<.02f,
            "Closed scene pose incorrect.");
        std::cout<<"Door fixed-step scene/physics/raycast round trip passed\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
