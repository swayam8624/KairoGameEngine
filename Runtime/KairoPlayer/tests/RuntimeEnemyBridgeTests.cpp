#include <iostream>
#include <stdexcept>
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeCharacterMotorBridge;
import Kairo.Player.RuntimeHealthBridge;
import Kairo.Player.RuntimeEnemyBridge;
import Kairo.Player.RuntimeCombatBridge;

void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv)
{
    if(argc!=2)return 2;
    try
    {
        kairo::player::RuntimeProject project(argv[1]);
        kairo::player::RuntimePhysicsBridge physics(project.Scene());
        kairo::player::RuntimeCharacterMotorBridge motor(project.Scene(),physics);
        const kairo::engine::Entity player{1},enemy{40};
        motor.Register(player);
        kairo::player::RuntimeHealthBridge health(project,physics,motor,player);
        kairo::player::RuntimeEnemyBridge enemies(project,physics,motor,player,health.Enabled());
        kairo::player::RuntimeCombatBridge combat(project,physics,player);
        Check(enemies.Size()==1&&motor.IsRegistered(enemy),"Enemy binding failed.");
        // The authored pillar is between two capsule centers within reach.
        motor.Teleport(enemy,{-3.5f,.92f,-2.5f});
        motor.Teleport(player,{-3.5f,.92f,-1.1f});
        Check(enemies.Step(1.f/60)==0,"Enemy attacked through solid pillar.");
        motor.Teleport(enemy,{0,.92f,1.6f});
        motor.Teleport(player,{0,.92f,2.6f});
        const auto damage=enemies.Step(1.f/60);
        Check(damage==10,"Visible enemy did not attack.");
        Check(!health.AfterCharacterStep(damage)&&health.Health()==90,"Enemy damage not applied.");
        Check(enemies.Step(1.f/60)==0,"Enemy ignored cooldown.");
        combat.Capture(true,{0,0,-1});combat.Step();
        Check(physics.BodyFor(enemy).has_value(),"First attack killed enemy early.");
        for(int i=0;i<11;++i)combat.Step();
        combat.Capture(true,{0,0,-1});combat.Step();
        Check(!physics.BodyFor(enemy)&&!project.Scene().IsActiveInHierarchy(enemy),"Enemy defeat did not remove body and visibility.");
        Check(enemies.Step(1.f/60)==0&&!motor.IsRegistered(enemy),"Destroyed enemy retained motor or attacked.");
        std::cout<<"Native enemy occlusion, attack, damage and defeat passed\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
