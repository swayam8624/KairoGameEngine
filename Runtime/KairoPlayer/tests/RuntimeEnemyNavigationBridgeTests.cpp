#include <cmath>
#include <iostream>
#include <stdexcept>

import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeCharacterMotorBridge;
import Kairo.Player.RuntimeEnemyBridge;

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
        kairo::player::RuntimeEnemyBridge enemies(project,physics,motor,player,true);
        Check(enemies.Size()==1&&enemies.NavigationEnabled(),"Authored navigation not installed.");
        bool crossed=false,detoured=false;
        for(int i=0;i<720;++i)
        {
            Check(enemies.Step(1.f/60)==0,"Distant player unexpectedly attacked.");
            const auto p=project.Scene().WorldTransform(enemy).Translation;
            Check(std::abs(p.x)>=.89f||std::abs(p.z)>=.89f,"NPC entered central obstacle clearance.");
            detoured=detoured||std::abs(p.z)>.9f;
            if(p.x>2.25f&&std::abs(p.z)<.3f){crossed=true;break;}
        }
        Check(crossed&&detoured,"Enemy failed to route around central obstacle.");
        std::cout<<"Authored enemy navigation routed around solid blocker\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
