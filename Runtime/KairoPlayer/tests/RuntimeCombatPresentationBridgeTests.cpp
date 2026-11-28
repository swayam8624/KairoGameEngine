#include <cmath>
#include <iostream>
#include <stdexcept>
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeCombatBridge;
import Kairo.Player.RuntimeCombatPresentationBridge;
void Check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int main(int argc,char** argv)
{
    if(argc!=2)return 2;
    try
    {
        kairo::player::RuntimeProject project(argv[1]);
        kairo::player::RuntimePhysicsBridge physics(project.Scene());
        const kairo::engine::Entity player{1},marker{60},unused{61};
        kairo::player::RuntimeCombatBridge combat(project,physics,player);
        kairo::player::RuntimeCombatPresentationBridge presentation(project,combat,player);
        Check(presentation.MarkerCount()==8,"Marker pool not loaded.");
        presentation.Step(0);
        Check(!project.Scene().IsActiveInHierarchy(marker)&&!physics.BodyFor(marker),"Initial marker state.");
        combat.Capture(true,{0,0,-1});combat.Step();presentation.Step(0);
        Check(combat.LaunchSerial()==1&&project.Scene().IsActiveInHierarchy(marker)&&!project.Scene().IsActiveInHierarchy(unused),"Launch visibility.");
        const auto start=project.Scene().WorldTransform(marker).Translation;
        combat.Capture(true,{0,0,-1});combat.Step();presentation.Step(1./60);
        const auto advanced=project.Scene().WorldTransform(marker).Translation;
        Check(combat.LaunchSerial()==1&&std::abs(start.z-advanced.z-.2f)<.001f,"Cooldown or position mismatch.");
        for(int i=0;i<12;++i){combat.Step();presentation.Step(1./60);}
        Check(!combat.ActiveFlights()&&!project.Scene().IsActiveInHierarchy(marker),"Impact left visible projectile.");
        combat.Capture(true,{0,0,-1});combat.Step();presentation.Step(0);
        Check(combat.LaunchSerial()==2&&project.Scene().IsActiveInHierarchy(marker),"Slot not reused.");
        combat.CancelFlights();presentation.Step(0);
        Check(!project.Scene().IsActiveInHierarchy(marker)&&!physics.BodyFor(marker),"Cancellation left marker.");
        std::cout<<"Native combat marker allocation, travel, impact and reuse passed\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
