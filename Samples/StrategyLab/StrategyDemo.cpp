#include "RuntimeStrategy.hpp"
#include <iostream>
#include <sstream>
using namespace kairo::player::detail;

// Render workers over the authoritative map. The selected worker ID is a
// host UI concern, not a duplicate copy of the simulation's state.
static void Draw(const RuntimeStrategy& game,std::size_t selected)
{
    auto rows=game.Rows();
    for(std::size_t i=0;i<game.Units().size();++i)
    {
        const auto& u=game.Units()[i];rows[u.Position.Y][u.Position.X]=char('0'+i);
    }
    for(const auto& row:rows)std::cout<<row<<'\n';
    std::cout<<"Selected "<<selected<<" Stored "<<game.Stored()<<'/'<<game.Goal()<<" Cargo "<<game.Units()[selected].Cargo<<'\n';
}
int main()
{
    RuntimeStrategy game(StrategyDemoLevel());std::size_t selected=0;
    std::cout<<"select ID | move X Y | gather X Y | tick N (1..120) | q\n";
    Draw(game,selected);
    for(std::string line;std::getline(std::cin,line);)
    {
        if(line=="q")return 0;
        std::istringstream in(line);std::string action,extra;in>>action;
        try
        {
            if(action=="select")
            {
                std::size_t id;if(!(in>>id)||in>>extra||id>=game.Units().size())throw std::invalid_argument("Invalid selection.");selected=id;
            }
            else if(action=="move"||action=="gather")
            {
                StrategyCell p;if(!(in>>p.X>>p.Y)||in>>extra)throw std::invalid_argument("Invalid coordinates.");
                if(action=="move")game.Move(selected,p);else game.Gather(selected,p);
            }
            else if(action=="tick")
            {
                unsigned n;if(!(in>>n)||in>>extra||!n||n>120)throw std::invalid_argument("Invalid tick count.");
                for(unsigned i=0;i<n&&!game.Won();++i)game.Tick();
            }
            else throw std::invalid_argument("Unknown command.");
            Draw(game,selected);
            if(game.Won()){std::cout<<"Strategy objective complete!\n";return 0;}
        }
        catch(const std::exception& e){std::cout<<e.what()<<'\n';}
    }
    return 1;
}
