#include "../../Runtime/KairoPlayer/RuntimeWaterGrid.hpp"
#include <iostream>
#include <string_view>
using namespace kairo::player::detail;

// A closed 16x9 basin split by a raised ridge with one gap. Print the actual
// simulated water depth, not a prerecorded animation. Read a horizontal row
// as a cross-section of the top-down grid; the characters encode depth.
int main()
{
    RuntimeWaterGrid grid(16,9);
    for(std::size_t y=0;y<grid.Height();++y)if(y!=4)grid.SetBed(8,y,5.);
    grid.AddWater(2,4,8.);
    constexpr std::string_view palette=" .:-=+*#%@";
    for(unsigned frame=0;frame<=100;++frame)
    {
        if(frame%20==0)
        {
            std::cout<<"Step "<<frame<<" volume "<<grid.TotalVolume()<<'\n';
            for(std::size_t y=0;y<grid.Height();++y)
            {
                for(std::size_t x=0;x<grid.Width();++x)
                    std::cout<<(x==8&&y!=4?'|':palette[static_cast<std::size_t>(std::min(9.,std::ceil(grid.Depth(x,y)*12)))]);
                std::cout<<'\n';
            }
        }
        if(frame!=100)grid.Step(.2);
    }
}
