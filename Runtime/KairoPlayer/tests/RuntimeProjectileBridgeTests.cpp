#include <iostream>
#include <stdexcept>
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeCombatBridge;
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv)
{
    if(argc!=2)return 2;
    try
    {
        kairo::player::RuntimeProject project(argv[1]);
        kairo::player::RuntimePhysicsBridge physics(project.Scene());
        const kairo::engine::Entity player{1},crate{40},protectedCrate{41},wall{42};
        kairo::player::RuntimeCombatBridge combat(project,physics,player);
        const auto attack=[&]{combat.Capture(true,{0,0,-1});combat.Step();};
        const auto advance=[&](int ticks){for(int i=0;i<ticks;++i)combat.Step();};
        attack();
        Check(combat.ActiveFlights()==1&&physics.BodyFor(crate).has_value(),"First launch was immediate.");
        advance(12);
        Check(!combat.ActiveFlights()&&physics.BodyFor(crate).has_value(),"First impact did not leave crate alive.");
        attack();
        Check(combat.ActiveFlights()==1&&physics.BodyFor(crate).has_value(),"Second launch damaged immediately.");
        advance(12);
        Check(!combat.ActiveFlights()&&!physics.BodyFor(crate)&&!project.Scene().IsActiveInHierarchy(crate),"Delayed destruction failed.");
        attack();advance(40);
        Check(!combat.ActiveFlights()&&physics.BodyFor(protectedCrate)&&physics.BodyFor(wall),"Wall failed to intercept projectile.");
        attack();combat.CancelFlights();
        Check(!combat.ActiveFlights(),"Cancel did not discard in-flight attacks.");
        std::cout<<"Native projectile flight, delayed destruction and occlusion passed\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
