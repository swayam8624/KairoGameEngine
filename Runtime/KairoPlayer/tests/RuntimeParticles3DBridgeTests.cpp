#include <cmath>
#include <iostream>
#include <stdexcept>
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimeParticles3DBridge;

void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv)
{
    if(argc!=2)return 2;
    try
    {
        kairo::player::RuntimeProject project(argv[1]);
        kairo::player::RuntimeParticles3DBridge particles(project);
        const kairo::engine::Entity emitter{50},marker{100},next{101};
        auto& scene=project.Scene();
        Check(particles.Capacity()==24,"Particle pool size.");
        Check(!scene.IsActiveInHierarchy(marker),"Marker active before simulation.");
        particles.Advance(.05);
        Check(scene.IsActiveInHierarchy(marker)&&!scene.IsActiveInHierarchy(next),"Initial emission.");
        const auto start=scene.WorldTransform(marker);
        Check(std::abs(start.Translation.y-1.f)<.0001f,"Emitter position not used.");
        particles.Advance(.05);
        const auto advanced=scene.WorldTransform(marker);
        Check(advanced.Translation.y>start.Translation.y&&advanced.Scale.x<start.Scale.x,"Motion or size decay.");
        Check(scene.IsActiveInHierarchy(next),"Second particle absent.");
        scene.SetEnabled(emitter,false);
        for(unsigned i=0;i<48;++i)particles.Advance(1./60.);
        Check(!scene.IsActiveInHierarchy(marker)&&!scene.IsActiveInHierarchy(next),"Disabled emitter did not drain.");
        scene.SetEnabled(emitter,true);
        particles.Advance(.05);
        Check(scene.IsActiveInHierarchy(marker)&&std::abs(scene.WorldTransform(marker).Scale.x-.2f)<.0001f,"Pool not reused at authored scale.");
        std::cout<<"Native mesh particle scene emission, motion, scale, drain and reuse passed\n";
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
