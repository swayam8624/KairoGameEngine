#include "../../Runtime/KairoPlayer/RuntimeTwoBoneIK.hpp"
#include <iostream>
using namespace kairo::player::detail;
// Print one pose per requested target. This CPU lab exposes the solved joint
// and tip; the real renderer must still consume local joint rotations.
int main()
{
    std::cout<<"Enter target x y z (Ctrl-D to exit). Root=(0,0,0), bone lengths=2,1.5\n";
    IKPoint target{};
    while(std::cin>>target[0]>>target[1]>>target[2])
    {
        try
        {
            const auto p=SolveTwoBoneIK({0,0,0},target,{0,1,0},2,1.5);
            std::cout<<(p.Reached?"reached":"clamped")<<" joint ";
            for(double x:p.Joint)std::cout<<x<<' ';
            std::cout<<"tip ";for(double x:p.Tip)std::cout<<x<<' ';
            std::cout<<'\n';
        }
        catch(const std::invalid_argument& e){std::cerr<<e.what()<<'\n';}
    }
}
