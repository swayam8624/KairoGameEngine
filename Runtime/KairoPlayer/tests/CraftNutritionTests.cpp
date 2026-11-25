#include "../CraftNutrition.hpp"
#include "../CraftVitals.hpp"
#include <iostream>
#include <limits>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void Ticks(CraftNutrition& n,CraftVitals& v,unsigned count,bool sprint=false)
{for(unsigned i=0;i<count;++i)n.Advance(.05,sprint,v);}
}
int main()
{
    CraftNutrition n;CraftVitals v;
    std::array<std::uint32_t,4> items{10,4,3,2};
    Check(!n.Eat(items)&&items[0]==10,"Full hunger spent forage");
    Ticks(n,v,599);Check(n.Hunger()==20,"Early idle drain");
    Ticks(n,v,1);Check(n.Hunger()==19,"Idle drain missing");
    Check(n.Eat(items)&&items==std::array<std::uint32_t,4>{8,4,3,2}&&n.Hunger()==20,"Forage transaction");
    n.Reset();Ticks(n,v,200,true);Check(n.Hunger()==19,"Sprint drain differs");
    n.Reset();v.Damage(6);Ticks(n,v,79);Check(v.Health()==14,"Early healing");
    Ticks(n,v,1);Check(v.Health()==15&&n.Hunger()==19,"Regeneration transaction");
    n.Reset();v.Respawn(0);Ticks(n,v,600*20);Check(!n.CanSprint()&&n.Hunger()==0&&v.Health()==20,"Starvation start");
    Ticks(n,v,79);Check(v.Health()==19,"Starvation damage missing");
    Ticks(n,v,80*19);Check(v.Dead(),"Starvation not lethal");
    v.Heal(20);Check(v.Dead(),"Heal resurrected dead player");
    n.Reset();v.Respawn(0);Check(n.Hunger()==20&&v.Health()==20,"Respawn reset");
    n.Advance(1000,false,v);Check(n.Hunger()==20,"Long stall bypassed cap");
    bool rejected=false;try{n.Advance(std::numeric_limits<double>::infinity(),false,v);}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"Invalid time accepted");
    std::mt19937 rng(791);
    CraftNutrition a,b;CraftVitals av,bv;
    for(unsigned i=0;i<100000;++i)
    {
        if(i%103==0){a.Reset();b.Reset();av.Respawn(0);bv.Respawn(0);}
        if(i%29==0){av.Damage(1);bv.Damage(1);}
        const bool sprint=rng()%2;
        a.Advance(.1,sprint,av);
        const double part=double(rng()%101)/1000.;
        b.Advance(part,sprint,bv);b.Advance(.1-part,sprint,bv);
        Check(a.Hunger()==b.Hunger()&&av.Health()==bv.Health()&&a.CanSprint()==b.CanSprint(),"Frame partition changed nutrition");
    }
    for(unsigned i=0;i<100000;++i)
    {
        std::array<std::uint32_t,4> inventory{static_cast<std::uint32_t>(rng()%10),1,2,3};
        const auto before=inventory;
        if(i%2==0){n.Reset();v.Respawn(0);Ticks(n,v,600);}
        const auto hunger=n.Hunger();const bool possible=hunger<20&&before[0]>=2;
        Check(n.Eat(inventory)==possible,"Forage feasibility");
        Check(inventory[0]==before[0]-(possible?2u:0u)&&inventory[1]==1&&inventory[2]==2&&inventory[3]==3,"Forage conservation");
        Check(n.Hunger()==(possible?std::min(20u,hunger+6):hunger),"Forage hunger mismatch");
    }
    std::cout<<"100000 frame partitions and 100000 forage transactions; sprint, healing, starvation, respawn and stall guards passed\n";
}
