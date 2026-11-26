#include "../RuntimeCombatPresentation.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
void Check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int main()
{
    RuntimeCombatPresentation view(8,.25);
    auto f=view.Step(0,0,0);Check(!f.AttackActive&&!f.RestartAttack&&!f.VisibleFlights,"initial state");
    f=view.Step(1,12,.1);Check(f.AttackActive&&f.RestartAttack&&f.VisibleFlights==8,"launch and pool cap");
    f=view.Step(1,1,0);Check(f.AttackActive&&!f.RestartAttack&&f.VisibleFlights==1,"paused frame");
    f=view.Step(1,0,.25);Check(!f.AttackActive&&!f.VisibleFlights,"expiry");
    f=view.Step(3,2,.25);Check(f.AttackActive&&f.RestartAttack,"coalesced launches");
    bool rejected=false;try{(void)view.Step(2,0,.1);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected&&view.Step(3,2,0).AttackActive,"invalid sample changed state");
    std::mt19937 rng(47);std::uint64_t serial=0;double remaining=0;RuntimeCombatPresentation random(16,.25);
    for(int i=0;i<50000;++i)
    {
        const bool restart=rng()%8==0;serial+=restart?1:0;
        const auto flights=rng()%513;const double dt=double(rng()%5)/16;
        remaining=restart?.25:std::max(0.,remaining-dt);
        f=random.Step(serial,flights,dt);
        Check(f.AttackActive==(remaining>0)&&f.RestartAttack==restart&&f.VisibleFlights==std::min<std::size_t>(flights,16),"random presentation parity");
    }
    std::cout<<"50000 randomized combat presentation samples and boundary tests passed\n";
}
