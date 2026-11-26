#include "RuntimeItemInventory.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <random>
using namespace kairo::player::detail;
static void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main()
{
    using Change=RuntimeItemInventory::Change;
    RuntimeItemInventory bag({{"crystal",9},{"gold",99}});
    const std::array initial{Change{"crystal",3},Change{"gold",5}};
    Check(bag.Apply(initial),"initial grant");
    const auto before=bag.Snapshot();
    Check(!bag.Apply(std::array{Change{"crystal",-4},Change{"gold",1}})&&bag.Snapshot()==before,"partial trade");
    Check(!bag.Apply(std::array{Change{"crystal",-1},Change{"gold",std::numeric_limits<int>::max()}})&&bag.Snapshot()==before,"overflow trade");
    Check(bag.Apply(std::array{Change{"crystal",-5},Change{"crystal",5}})&&bag.Snapshot()==before,"net change order");
    bool bad=false;try{bag.Restore({{"crystal",1},{"other",1}});}catch(const std::invalid_argument&){bad=true;}
    Check(bad&&bag.Snapshot()==before,"bad snapshot changed bag");
    std::mt19937 rng(741);int a=3,b=5;
    for(int i=0;i<50000;++i)
    {
        const int da=int(rng()%21)-10,db=int(rng()%201)-100;
        const bool valid=a+da>=0&&a+da<=9&&b+db>=0&&b+db<=99;
        Check(bag.Apply(std::array{Change{"crystal",da},Change{"gold",db}})==valid,"oracle acceptance");
        if(valid){a+=da;b+=db;}
        Check(bag.Count("crystal")==unsigned(a)&&bag.Count("gold")==unsigned(b),"oracle counts");
    }
    std::cout<<"50000 inventory transactions matched independent integer oracle\n";
}
