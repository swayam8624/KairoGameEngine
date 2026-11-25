#include "RuntimePlatformer.hpp"
#include <iostream>
#include <sstream>
using namespace kairo::player::detail;

// Render a coarse text view of the authoritative simulation. Each command
// advances a chosen number of fixed ticks, not wall-clock seconds.
static void Draw(const RuntimePlatformer& game)
{
    auto rows=game.Rows();const auto& s=game.Current();
    rows[(s.Y+RuntimePlatformer::Height/2)/16][(s.X+RuntimePlatformer::Width/2)/16]='@';
    for(const auto& row:rows)std::cout<<row<<'\n';
    std::cout<<"Coins "<<s.Coins<<'/'<<game.CoinCount()<<" Deaths "<<s.Deaths<<" X "<<s.X<<" Y "<<s.Y<<'\n';
}
int main()
{
    RuntimePlatformer game(PlatformerDemoLevel());
    std::cout<<"Controls: r/l/w (move right/left/wait), j (jump right), k (jump left), s (jump still), followed by ticks 1..120; reset or q.\n";
    Draw(game);
    for(std::string line;std::getline(std::cin,line);)
    {
        if(line=="q")return 0;
        if(line=="reset"){game.Reset();Draw(game);continue;}
        std::istringstream in(line);char command=0;int ticks=0;std::string extra;
        if(!(in>>command>>ticks)||in>>extra||ticks<1||ticks>120||std::string_view("rlwjks").find(command)==std::string_view::npos)
        {std::cout<<"Invalid command.\n";continue;}
        const int axis=command=='r'||command=='j'?1:command=='l'||command=='k'?-1:0;
        for(int i=0;i<ticks;++i)game.Step({axis,command=='j'||command=='k'||command=='s'});
        Draw(game);
        if(game.Current().Won){std::cout<<"Level complete!\n";return 0;}
    }
    return 1;
}
