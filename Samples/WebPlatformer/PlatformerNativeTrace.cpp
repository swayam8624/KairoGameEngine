#include "RuntimePlatformer.hpp"
#include <iostream>

// Independent native wrapper trace. Node supplies identical controls to this
// process and the real WASM instance and compares every visible state field.
int main()
{
    using namespace kairo::player::detail;
    RuntimePlatformer game(PlatformerDemoLevel());
    int axis=0,jump=0;
    while(std::cin>>axis>>jump)
    {
        if(axis==9)game.Reset();
        else game.Step({axis,jump!=0});
        const auto& s=game.Current();
        std::cout<<s.X<<' '<<s.Y<<' '<<s.Coins<<' '<<s.Deaths<<' '<<s.Won<<' ';
        for(const auto& row:game.Rows())std::cout<<row;
        std::cout<<'\n';
    }
}
