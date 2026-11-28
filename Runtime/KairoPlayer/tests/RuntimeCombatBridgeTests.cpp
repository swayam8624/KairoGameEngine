#include <iostream>
#include <stdexcept>

import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeCombatBridge;

namespace engine=kairo::engine;
namespace player=kairo::player;
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv)
{
    if(argc!=2)return 2;
    try
    {
        player::RuntimeProject project(argv[1]);
        player::RuntimePhysicsBridge physics(project.Scene());
        const engine::Entity controlled{1},crate{40},protectedCrate{41},wall{42};
        player::RuntimeCombatBridge combat(project,physics,controlled);
        Check(combat.Enabled()&&physics.BodyFor(crate)&&physics.BodyFor(protectedCrate),"Combat binding failed.");
        combat.Capture(true,{0,0,-1});combat.Step();
        Check(physics.BodyFor(crate).has_value(),"First hit destroyed crate early.");
        for(int i=0;i<11;++i){combat.Capture(true,{0,0,-1});combat.Step();}
        Check(physics.BodyFor(crate).has_value(),"Cooldown allowed premature hit.");
        combat.Capture(true,{0,0,-1});combat.Step();
        Check(!physics.BodyFor(crate)&&!project.Scene().IsActiveInHierarchy(crate),"Destroyed crate still visible or solid.");
        for(int i=0;i<12;++i)combat.Step();
        combat.Capture(true,{0,0,-1});combat.Step();
        Check(physics.BodyFor(protectedCrate)&&physics.BodyFor(wall),"Occluded crate or wall was destroyed.");
        const auto hit=physics.Raycast({0,.92f,1.5f},{0,0,-1},5);
        Check(hit&&hit->first==wall,"Ray obstruction missing.");
        // Move to the other side of the wall and hit twice, verifying that
        // the previous blocked shot did not damage the protected crate.
        physics.SetEntityPosition(controlled,{0,.92,-1.3});
        for(int i=0;i<12;++i)combat.Step();
        combat.Capture(true,{0,0,-1});combat.Step();
        Check(physics.BodyFor(protectedCrate).has_value(),"Occluded shot damaged the protected crate.");
        for(int i=0;i<12;++i)combat.Step();
        combat.Capture(true,{0,0,-1});combat.Step();
        Check(!physics.BodyFor(protectedCrate)&&!project.Scene().IsActiveInHierarchy(protectedCrate),"Second target not destroyed.");
        std::cout<<"Actual raycast, cooldown, obstruction and collider removal passed\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
