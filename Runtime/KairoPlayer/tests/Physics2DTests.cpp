#include "../Physics2D.hpp"
#include <iostream>
#include <random>
using namespace kairo::physics2d;
void Check(bool ok){if(!ok)throw std::runtime_error("Physics2D regression");}
Body Make(Vec p,Vec half,Motion type=Motion::Static,Shape shape=Shape::Box)
{Body b;b.Position=p;b.Half=half;b.Type=type;b.Geometry=shape;return b;}
int main()
{
    auto floor=Make({0,60},{50,4});auto box=Make({0,20},{4,4},Motion::Dynamic);
    Scene drop({floor,box},{},{0,180});for(int i=0;i<360;++i)drop.Step();
    Check(std::abs(drop.Bodies()[1].Position.Y-52)<.01&&std::abs(drop.Bodies()[1].Velocity.Y)<.01);
    auto circle=Make({0,0},{3,3},Motion::Dynamic,Shape::Circle);
    auto wall=Make({8,0},{2,20});circle.Velocity={240,0};circle.Restitution=wall.Restitution=1;
    Scene bounce({circle,wall},{},{});bounce.Step();Check(bounce.Bodies()[0].Velocity.X<-239);
    auto thin=Make({8,0},{1,8});Scene bounded({circle,thin},{},{});for(int i=0;i<10;++i)bounded.Step();
    Check(bounded.Bodies()[0].Position.X<=4.0011);
    auto sensor=Make({8,0},{1,8});sensor.Sensor=true;
    Scene triggers({circle,sensor},{},{});unsigned begins=0,ends=0;
    for(int i=0;i<8;++i){triggers.Step();for(auto e:triggers.Events())(e.Begin?begins:ends)++;}
    Check(begins==1&&ends==1&&triggers.Bodies()[0].Position.X>20);
    sensor.Mask=2;Scene filtered({circle,sensor},{},{});for(int i=0;i<8;++i){filtered.Step();Check(filtered.Events().empty());}
    auto a=Make({0,0},{2,2},Motion::Dynamic),b=Make({8,0},{2,2},Motion::Dynamic);
    a.Velocity={120,0};b.Velocity={-120,0};a.Restitution=b.Restitution=1;
    Scene exchange({a,b},{},{});for(int i=0;i<3;++i)exchange.Step();
    Check(exchange.Bodies()[0].Velocity.X<-119&&exchange.Bodies()[1].Velocity.X>119);
    auto slider=Make({0,52},{4,4},Motion::Dynamic);slider.Velocity={100,0};slider.Friction=floor.Friction=1;
    Scene friction({floor,slider},{},{0,180});for(int i=0;i<60;++i)friction.Step();Check(std::abs(friction.Bodies()[1].Velocity.X)<1);
    auto anchor=Make({0,0},{1,1});auto bob=Make({20,0},{2,2},Motion::Dynamic,Shape::Circle);
    Scene joint({anchor,bob},{{0,1,20}},{0,180});for(int i=0;i<600;++i)joint.Step();
    Check(std::abs(Length(joint.Bodies()[1].Position)-20)<.05);
    auto kin=Make({0,0},{2,2},Motion::Kinematic);kin.Velocity={60,0};
    Scene kinematic({kin,bob},{},{});kinematic.Step();Check(std::abs(kinematic.Bodies()[0].Position.X-1)<1e-9);
    kinematic.SetKinematic(0,{2,0});kinematic.Impulse(1,{3,4});Check(kinematic.Bodies()[1].Velocity.X==3);
    std::mt19937 rng(2026);unsigned overlaps=0;
    for(unsigned i=0;i<10000;++i)
    {
        const Vec px{double(int(rng()%80)-40),double(int(rng()%80)-40)};
        const auto sx=rng()%2?Shape::Box:Shape::Circle;
        const Vec py{double(int(rng()%80)-40),double(int(rng()%80)-40)};
        const auto sy=rng()%2?Shape::Box:Shape::Circle;
        auto x=Make(px,{5,5},Motion::Dynamic,sx);
        auto y=Make(py,{6,6},Motion::Dynamic,sy);
        auto c=Collide(x,y),d=Collide(y,x);
        Check(std::abs(c.Depth-d.Depth)<1e-10);
        if(c.Depth>0){++overlaps;Check(std::abs(Length(c.Normal)-1)<1e-10);y.Position+=c.Normal*(c.Depth+1e-7);Check(Collide(x,y).Depth<1e-6);}
    }
    // Brute force oracle for broadphase completeness, filtering and event order.
    std::size_t expectedPairs=0;
    for(unsigned trial=0;trial<100;++trial)
    {
        std::vector<Body> bodies;
        for(unsigned i=0;i<64;++i)
        {
            const Vec pos{double(rng()%100),double(rng()%100)};
            auto body=Make(pos,{5,5},Motion::Static,rng()%2?Shape::Box:Shape::Circle);
            body.Sensor=i%5==0;body.Layer=1u<<(rng()%3);body.Mask=rng()%8;
            bodies.push_back(body);
        }
        std::vector<std::pair<std::size_t,std::size_t>> expected;
        for(std::size_t i=0;i<bodies.size();++i)for(std::size_t j=i+1;j<bodies.size();++j)
            if((bodies[i].Sensor||bodies[j].Sensor)&&Filter(bodies[i],bodies[j])&&Collide(bodies[i],bodies[j]).Depth>0)
                expected.emplace_back(i,j);
        Scene scene(std::move(bodies),{},{});scene.Step();
        Check(scene.Events().size()==expected.size());
        for(std::size_t i=0;i<expected.size();++i)
        {const auto e=scene.Events()[i];Check(e.Begin&&std::pair{e.A,e.B}==expected[i]);}
        scene.Step();Check(scene.Events().empty());expectedPairs+=expected.size();
    }
    std::cout<<"100 brute-force sensor broadphase comparisons, "<<expectedPairs<<" expected transitions\n";
    bool rejected=false;try{auto bad=box;bad.Mass=0;Scene s({bad},{},{});}catch(const std::invalid_argument&){rejected=true;}Check(rejected);
    std::cout<<"10000 randomized shape contacts, "<<overlaps<<" overlaps; gravity, bounce, bounded sweep, sensors, masks, momentum, friction, joint and kinematic fixtures passed\n";
}
