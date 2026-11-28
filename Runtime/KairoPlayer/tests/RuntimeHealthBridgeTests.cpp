#include <cmath>
#include <iostream>
#include <optional>
#include <stdexcept>

import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Foundation.PhysicsMath.Types;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeCharacterMotorBridge;
import Kairo.Player.RuntimeHealthBridge;

namespace engine=kairo::engine;
namespace player=kairo::player;
void Check(bool b,const char* message){if(!b)throw std::runtime_error(message);}

int main(int argc,char** argv)
{
    if(argc!=2)return 2;
    try
    {
        player::RuntimeProject project(argv[1]);
        player::RuntimePhysicsBridge physics(project.Scene());
        player::RuntimeCharacterMotorBridge motor(project.Scene(),physics);
        const engine::Entity controlled{1};
        motor.Register(controlled);
        player::RuntimeHealthBridge health(project,physics,motor,controlled);
        Check(health.Enabled()&&health.Health()==100,"Health binding failed.");
        const auto body=physics.BodyFor(controlled);
        // Explicitly compare the contained IDs after checking presence. This
        // preserves the identity invariant without relying on optional's
        // constrained comparison across C++ module boundaries on MSVC.
        const auto sameBody=[&]{
            const auto current=physics.BodyFor(controlled);
            return body.has_value()&&current.has_value()&&*current==*body;
        };
        Check(body.has_value(),"Character body missing.");
        motor.Teleport(controlled,{-3,.92f,3.4f});
        Check(!health.AfterCharacterStep()&&health.Checkpoint()==7,"Checkpoint activation failed.");
        motor.Teleport(controlled,{3,.92f,3.4f});
        Check(!health.AfterCharacterStep()&&health.Health()==60,"Initial damage failed.");
        for(int i=0;i<29;++i)Check(!health.AfterCharacterStep()&&health.Health()==60,"Cooldown failed.");
        Check(!health.AfterCharacterStep()&&health.Health()==20,"Second damage failed.");
        for(int i=0;i<29;++i)(void)health.AfterCharacterStep();
        Check(health.AfterCharacterStep()&&health.Health()==100,"Death and health reset failed.");
        const auto pose=project.Scene().WorldTransform(controlled).Translation;
        Check(std::abs(pose.x+3)<.001f&&std::abs(pose.y-.92f)<.001f&&std::abs(pose.z-3.4f)<.001f,"Checkpoint pose failed.");
        Check(sameBody()&&physics.World().Bodies().at(*body).State.Position==pose,
            "Respawn scene/body disagreement.");
        Check(motor.State(controlled).VerticalVelocity==0,"Motor vertical velocity was not reset.");
        motor.Teleport(controlled,{0,-11,0});
        Check(health.AfterCharacterStep(),"Fall death failed during cooldown.");
        Check(project.Scene().WorldTransform(controlled).Translation==pose,"Fall respawn changed checkpoint.");
        std::cout<<"Health bridge checkpoint, damage and synchronized motor respawn passed\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
