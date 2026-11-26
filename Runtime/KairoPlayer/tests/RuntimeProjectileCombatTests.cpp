#include "../RuntimeCombat.hpp"
#include "../RuntimeProjectileFlight.hpp"
#include <iostream>
using namespace kairo::player::detail;
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main()
{
    RuntimeCombat combat("kairo-combat 1\nattack 30 2 6\nprojectile 12\ntarget 1 45\n");
    Check(combat.ProjectileSpeed()==12,"speed parse");
    RuntimeProjectileFlight pool;
    unsigned hits=0;
    auto tick=[&](bool press){
        combat.CapturePress(press);
        pool.Step(.1,[](const FlightVector& p,const FlightVector&,double d)->std::optional<std::uint32_t>{
            if(p[0]<=3&&p[0]+d>=3)return 1;
            return {};
        },[&](auto id){const auto event=combat.ApplyHit(id);Check(event.Damage==(hits?15u:30u),"impact damage");++hits;});
        if(combat.BeginAttack())Check(pool.Launch({0,0,0},{1,0,0},combat.Range(),combat.ProjectileSpeed()),"launch");
    };
    tick(true);tick(true);tick(true);
    Check(pool.Size()==2&&combat.Targets()[0].Health==45,"premature impact or cooldown");
    tick(false);Check(hits==1&&combat.Targets()[0].Health==15,"first delayed impact");
    tick(false);tick(false);Check(hits==2&&!pool.Size()&&!combat.Targets()[0].Health,"second delayed impact");
    for(const auto* doc:{"projectile 0\n","projectile 501\n","projectile 12 extra\n","projectile 12\nprojectile 13\n"})
    {
        bool rejected=false;try{RuntimeCombat bad(std::string("kairo-combat 1\nattack 30 2 6\n")+doc);}catch(const std::invalid_argument&){rejected=true;}
        Check(rejected,"bad projectile rule accepted");
    }
    std::cout<<"combined launch, cooldown, delayed damage and parser tests passed\n";
}
