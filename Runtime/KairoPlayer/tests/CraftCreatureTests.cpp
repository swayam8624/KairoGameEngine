#include "../CraftCreature.hpp"
#include <iostream>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
const auto flat=[](VoxelCoordinate c)->std::optional<VoxelMaterial>{return c.Y<0?1:0;};
}
int main()
{
    CraftCreature creature;VoxelPlayer player{{.5,0,.5}};
    for(unsigned i=0;i<59;++i)Check(creature.Advance(.05,player,flat)==0&&!creature.Body(),"Early spawn");
    Check(creature.Advance(.05,player,flat)==0&&creature.Body()&&creature.Health()==12,"Spawn missing");
    Check(!VoxelPlayerBlocked(*creature.Body(),CraftCreature::Config(),flat),"Spawn blocked");
    unsigned damage=0;
    for(unsigned i=0;i<160;++i){damage+=creature.Advance(.05,player,flat);Check(creature.Body()&&!VoxelPlayerBlocked(*creature.Body(),CraftCreature::Config(),flat),"Pursuit penetration");}
    Check(damage>=2&&damage<=12,"Contact damage schedule");
    const auto feet=creature.Body()->Feet;
    const std::array eye{player.Feet[0],.8,player.Feet[2]},dir{feet[0]-eye[0],feet[1]+.8-eye[1],feet[2]-eye[2]};
    Check(creature.Aimed(eye,dir,flat)&&!creature.Aimed(eye,{-dir[0],-dir[1],-dir[2]},flat),"Aim selection");
    Check(!creature.Aimed(eye,dir,[](VoxelCoordinate)->std::optional<VoxelMaterial>{return {};}),"Unknown cells did not occlude");
    Check(!creature.Aimed(eye,dir,[](VoxelCoordinate)->std::optional<VoxelMaterial>{return 1;}),"Solid cells did not occlude");
    Check(creature.Strike()&&creature.Health()==8&&!creature.Strike(),"Attack cooldown");
    for(unsigned hit=0;hit<2;++hit){for(unsigned i=0;i<10;++i)(void)creature.Advance(.05,player,flat);Check(creature.Strike(),"Cooldown not released");}
    Check(!creature.Body()&&creature.Health()==0&&!creature.Strike(),"Death not removed");
    for(unsigned i=0;i<299;++i)Check(!creature.Advance(.05,player,flat)&&!creature.Body(),"Early respawn");
    (void)creature.Advance(.05,player,flat);Check(creature.Body()&&creature.Health()==12,"Respawn missing");
    // Wall isolates the player even while the creature is pursuing.
    creature.Reset();const auto wall=[](VoxelCoordinate c)->std::optional<VoxelMaterial>{return c.Y<0||(c.X==-2&&c.Y<3)?1:0;};
    for(unsigned i=0;i<400;++i){Check(creature.Advance(.05,player,wall)==0,"Attack through wall");if(creature.Body())Check(!VoxelPlayerBlocked(*creature.Body(),CraftCreature::Config(),wall),"Wall penetration");}
    creature.Reset();const auto gap=[](VoxelCoordinate c)->std::optional<VoxelMaterial>{return c.Y<0||(c.X==-2&&c.Y<3&&std::abs(c.Z)>1)?1:0;};
    unsigned routedDamage=0;
    for(unsigned i=0;i<600;++i){routedDamage+=creature.Advance(.05,player,gap);if(creature.Body())Check(!VoxelPlayerBlocked(*creature.Body(),CraftCreature::Config(),gap),"Navigation penetration");}
    Check(routedDamage>0,"Creature failed to route through wall gap");
    creature.Reset();for(unsigned i=0;i<120;++i)Check(!creature.Advance(.05,player,[](VoxelCoordinate)->std::optional<VoxelMaterial>{return {};})&&!creature.Body(),"Spawned in unknown cells");
    std::mt19937 rng(97);
    for(unsigned i=0;i<1000;++i)
    {
        player.Feet={double(int(rng()%1999999900u)-999999950)+.5,0,double(int(rng()%1999999900u)-999999950)+.5};
        CraftCreature a,b;
        for(unsigned t=0;t<80;++t)
        {
            const auto d=a.Advance(.1,player,flat);const double part=double(rng()%101)/1000.;
            const auto first=b.Advance(part,player,flat),second=b.Advance(.1-part,player,flat);
            Check(d==first+second&&a.Health()==b.Health()&&bool(a.Body())==bool(b.Body()),"Partition state mismatch");
            if(a.Body())Check(a.Body()->Feet==b.Body()->Feet&&!VoxelPlayerBlocked(*a.Body(),CraftCreature::Config(),flat),"Partition movement mismatch");
        }
    }
    std::cout<<"1000 signed spawn/pursuit fixtures, 80000 frame partitions, melee cooldown, occlusion, death and respawn passed\n";
}
