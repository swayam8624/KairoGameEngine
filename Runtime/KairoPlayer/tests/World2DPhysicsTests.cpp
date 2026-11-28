#include "../RuntimeWorld2D.hpp"
#include <iostream>
#include <iterator>
using namespace kairo::player::world2d;
void Check(bool ok){if(!ok)throw std::runtime_error("World physics integration regression");}
int main(int argc,char** argv)
{
    Check(argc==2);std::ifstream in(argv[1]);std::string source{std::istreambuf_iterator<char>(in),{}};
    Check(source.starts_with("kairo-world2d 3"));auto world=Parse(source);Check(world.Physics.has_value());
    Game game(world);Check(game.Physics()&&game.Physics()->Moving.size()==2);
    const auto initial=game.Actors();
    for(int i=0;i<300;++i)game.Step({});
    Check(game.Actors()[5].Bounds.Y>initial[5].Bounds.Y+20);
    Check(game.Actors()[5].Bounds.Y+game.Actors()[5].Bounds.H<=world.Height*16-15);
    Check(game.Actors()[7].Bounds!=initial[7].Bounds);
    game.Reset();Check(game.Actors()[5].Bounds==initial[5].Bounds&&game.Actors()[7].Bounds==initial[7].Bounds);
    for(int i=0;i<300;++i)game.Step({});
    const auto state=game.Actors()[5].Bounds;
    game.Reset();for(int i=0;i<300;++i)game.Step({});Check(game.Actors()[5].Bounds==state);
    const auto reject=[&](std::string bad){bool rejected=false;try{(void)Parse(bad);}catch(const std::invalid_argument&){rejected=true;}Check(rejected);};
    auto bad=source;auto at=bad.find("6 dynamic box");Check(at!=std::string::npos);bad.replace(at,13,"1 dynamic box");reject(bad);
    bad=source;at=bad.find("6 dynamic box");bad.replace(at,13,"9 dynamic box");reject(bad);
    bad=source;at=bad.find("6 dynamic box");bad.replace(at,13,"6 dynamic circle");reject(bad);
    reject(source+"unexpected\n");
    std::cout<<"World2D authored physics: moving props, tile collision, reset/replay and invalid references passed\n";
}
