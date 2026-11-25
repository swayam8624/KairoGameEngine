#include "../CraftCaveSession.hpp"
#include "../CraftCaveSpawn.hpp"
#include "../CraftSurfaceSpawn.hpp"
#include "../CraftEquipmentSave.hpp"
#include "../CraftMining.hpp"
#include "../CraftNutrition.hpp"
#include "../CraftVitals.hpp"
#include "../CraftRecipesHud.hpp"
#include <iostream>

namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}

template<class World,class Spawn,class Encode,class Decode>
void Journey(std::uint64_t seed,World world,Spawn spawn,Encode encode,Decode decode,std::size_t limit)
{
    CraftInventory inventory;
    CraftEquipment equipment;
    CraftMining mining;
    CraftVitals vitals;
    CraftNutrition food;
    const VoxelPlayerConfig config;
    const auto query=[&](VoxelCoordinate c)->std::optional<VoxelMaterial>{return world.Get(c);};
    const auto edit=[&](VoxelCoordinate c,VoxelMaterial m){return world.Set(c,m);};
    const auto initial=spawn(query);
    Check(initial&&initial->Grounded&&!VoxelPlayerBlocked(*initial,config,query),"Initial spawn invalid");
    vitals.Respawn(initial->Feet[1]);
    const auto mine=[&](const VoxelHit& hit,bool accept)
    {
        const CraftMiningTarget target{{hit.Cell.X,hit.Cell.Y,hit.Cell.Z},hit.Material};
        const auto before=inventory.Count(hit.Material),durability=equipment.Durability;
        const auto speed=equipment.Speed(hit.Material);
        const auto frames=static_cast<unsigned>(std::ceil(CraftMiningSeconds(hit.Material)/speed/.05-1e-9));
        for(unsigned i=0;i<frames;++i)
        {
            const bool done=mining.Advance(true,target,.05,[&](const auto& actual){
                Check(actual==target,"Mining target changed");
                if(!accept||!MineCraftBlock(inventory,hit.Cell,query,edit))return false;
                equipment.SuccessfulMine(hit.Material);return true;
            },speed);
            Check(done==(accept&&i+1==frames),"Mining timing or commit differs");
        }
        Check(inventory.Count(hit.Material)==before+(accept?1u:0u),"Mining inventory differs");
        Check(equipment.Durability==durability-(accept&&speed>1?1u:0u),"Mining durability differs");
        Check(world.Get(hit.Cell)==(accept?VoxelAir:hit.Material),"Mining world differs");
    };
    const auto ray=[&](int x,int z){return TraceVoxels({x+.5,15.5,z+.5},{0,-1,0},15,32,query);};
    // Harvest actual generated columns, without granting a starting inventory.
    for(int x=-4;x<=4&&(inventory.Count(CraftGrass)<2||inventory.Count(CraftDirt)<3||inventory.Count(CraftStone)<5);++x)
        for(int depth=0;depth<7;++depth)
        {
            const auto hit=ray(x,0);
            if(!hit)break;
            mine(*hit,true);
            if(inventory.Count(CraftGrass)>=2&&inventory.Count(CraftDirt)>=3&&inventory.Count(CraftStone)>=5)break;
        }
    Check(equipment.Craft(inventory.Counts),"Harvested pick craft failed");
    Check(CraftApplyRecipe(inventory,CraftRecipes[0]),"Harvested building recipe failed");
    // A fresh column contains exposed stone after its grass and dirt are mined.
    std::optional<VoxelHit> stone;
    for(unsigned depth=0;depth<4;++depth)
    {
        const auto hit=ray(12,0);
        if(!hit)break;
        if(hit->Material==CraftStone){stone=hit;break;}
        mine(*hit,true);
    }
    Check(stone.has_value(),"No exposed stone fixture");
    mine(*stone,false);mine(*stone,true);
    Check(equipment.Durability==63&&equipment.Equipped,"Pick use differs");
    for(unsigned i=0;i<600;++i)food.Advance(.05,false,vitals);
    Check(food.Hunger()==19&&food.Eat(inventory.Counts)&&food.Hunger()==20,"Forage failed");
    vitals.Damage(2);
    for(unsigned i=0;i<80;++i)food.Advance(.05,false,vitals);
    Check(vitals.Health()==19&&food.Hunger()==19,"Food-funded healing differs");
    inventory.Selected=CraftPlaced;
    const VoxelCoordinate placed{12,15,12};
    Check(PlaceCraftBlock(inventory,placed,query,edit,[&](auto c){return !PlayerIntersectsBlock(*initial,config,c);}),"Crafted block placement failed");
    const auto hud=BuildCraftHud(inventory,vitals.Health(),mining.Progress(),equipment.Durability,equipment.Equipped,food.Hunger());
    Check(!hud.empty(),"HUD empty");
    for(const auto& r:hud)Check(r.X>=0&&r.Y>=0&&r.W>0&&r.H>0&&r.X+r.W<=960&&r.Y+r.H<=540,"HUD outside canvas");
    const auto payload=encode(seed,world,inventory);
    const auto bytes=EncodeCraftEquipmentEnvelope(payload,equipment,limit);
    const auto outer=DecodeCraftEquipmentEnvelope(bytes,limit);
    const auto saved=decode(outer.Payload);
    Check(saved.Seed==seed&&saved.World.Overrides()==world.Overrides()&&saved.Inventory.Counts==inventory.Counts&&saved.Inventory.Selected==inventory.Selected,"Journey persistence differs");
    Check(outer.Equipment.Durability==63&&outer.Equipment.Equipped,"Journey equipment persistence differs");
    Check(EncodeCraftEquipmentEnvelope(encode(seed,saved.World,saved.Inventory),outer.Equipment,limit)==bytes,"Journey encoding differs");
    const auto reloaded=[&](VoxelCoordinate c)->std::optional<VoxelMaterial>{return saved.World.Get(c);};
    const auto next=spawn(reloaded);
    Check(next&&next->Grounded&&!VoxelPlayerBlocked(*next,config,reloaded),"Edited world respawn invalid");
    auto player=*next;AdvanceVoxelPlayer(player,config,{},.05,reloaded);
    Check(player.Grounded,"Respawn unsupported by collision solver");
}
}
int main()
{
    for(std::uint64_t seed=0;seed<256;++seed)
    {
        Journey(seed,StreamCraftWorldEdits{StreamingCraftBaseline{seed}},
            [](auto&& q){return FindCraftSurfaceSpawn(VoxelPlayerConfig{},CraftExtent,q);},
            EncodeCraftSession,DecodeCraftSession,CraftSessionMaximumBytes);
        Journey(seed,CaveCraftWorldEdits{CaveCraftBaseline{seed}},
            [](auto&& q){return FindCaveSpawn(VoxelPlayerConfig{},q);},
            EncodeCaveCraftSession,DecodeCaveCraftSession,CaveCraftMaximumBytes);
    }
    std::cout<<"512 seeded surface/cave CPU survival journeys passed: harvest, craft, wear, forage, heal, place, save, reload, HUD and respawn\n";
}
