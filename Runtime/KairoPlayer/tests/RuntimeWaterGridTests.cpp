#include "RuntimeWaterGrid.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()
{
    RuntimeWaterGrid flat(5,5);flat.AddWater(2,2,4);flat.Step(.25);
    Check(flat.Depth(2,2)==0&&flat.Depth(1,2)==1&&flat.Depth(3,2)==1&&flat.TotalVolume()==4,"Symmetric pulse");
    RuntimeWaterGrid wall(2,1);wall.SetBed(1,0,2);wall.AddWater(0,0,1);wall.Step(.25);
    Check(wall.Depth(0,0)==1&&wall.Depth(1,0)==0,"Flow climbed dry wall");
    RuntimeWaterGrid lake(2,1);lake.SetBed(1,0,1);lake.AddWater(0,0,2);lake.AddWater(1,0,1);lake.Step(.25);
    Check(lake.Depth(0,0)==2&&lake.Depth(1,0)==1,"Lake at rest changed");
    std::mt19937 rng(1207);
    for(unsigned trial=0;trial<100;++trial)
    {
        RuntimeWaterGrid grid(12,9);
        for(unsigned y=0;y<9;++y)for(unsigned x=0;x<12;++x)
        {grid.SetBed(x,y,double(rng()%100)/10.);grid.AddWater(x,y,double(rng()%100)/20.);}
        const double volume=grid.TotalVolume();
        for(unsigned i=0;i<200;++i)
        {
            grid.Step(.25);
            Check(std::abs(grid.TotalVolume()-volume)<1e-8,"Water not conserved");
            for(unsigned y=0;y<9;++y)for(unsigned x=0;x<12;++x)Check(grid.Depth(x,y)>=0,"Negative water");
        }
    }
    bool rejected=false;try{flat.AddWater(5,0,1);}catch(const std::out_of_range&){rejected=true;}
    Check(rejected,"Coordinate alias accepted");
    rejected=false;try{RuntimeWaterGrid invalid(129,1);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Oversized grid accepted");
    std::cout<<"20000 conservative grid steps, dry wall and lake-at-rest tests passed\n";
}
