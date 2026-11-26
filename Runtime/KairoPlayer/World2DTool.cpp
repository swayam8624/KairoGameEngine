#include "RuntimeWorld2D.hpp"
#include <iostream>
int main(int argc,char** argv)
{
    try
    {
        if(argc!=2)throw std::invalid_argument("Usage: KairoWorld2DTool <world.k2d>");
        const auto world=kairo::player::world2d::Load(argv[1]);
        std::cout<<"Validated "<<world.Width<<"x"<<world.Height<<" world with "<<world.Actors.size()<<" actors\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
