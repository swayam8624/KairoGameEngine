#include "../RuntimePlatformerProject.hpp"
#include <iostream>
#include <random>
#include <stdexcept>
using namespace kairo::player::detail;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void reject(std::string_view data)
{
    bool failed=false;
    try{(void)ParsePlatformerProject(data);}catch(const std::invalid_argument&){failed=true;}
    check(failed,"invalid map accepted");
}
int main()
{
    const std::string base="kairo-platformer 1\n################\n#..............#\n#..............#\n#..............#\n#S.C..H..C...G.#\n################\n";
    const auto rows=ParsePlatformerProject(base);
    check(rows==PlatformerDemoLevel(),"exact sample");
    RuntimePlatformer game(rows);
    for(int i=0;i<31;++i)game.Step({1,false});
    for(int i=0;i<17;++i)game.Step({1,true});
    for(int i=0;i<80;++i)game.Step({1,false});
    check(game.Current().Won&&game.Current().Coins==2,"authored winning route");
    std::string crlf;for(char c:base){if(c=='\n')crlf+='\r';crlf+=c;}
    check(ParsePlatformerProject(crlf)==rows,"CRLF");
    check(ParsePlatformerProject(base.substr(0,base.size()-1))==rows,"no trailing newline");
    for(const auto* bad:{"","kairo-platformer 2\n###\n#S#\n###","kairo-platformer 1\n####\n#SG.\n####",
        "kairo-platformer 1\n#####\n#SSG#\n#####","kairo-platformer 1\n#####\n#S?G#\n#####","kairo-platformer 1\n#####\n#SG#\n#####"})
        reject(bad);
    reject(base+"\n");
    reject(std::string(4097,'#'));
    std::mt19937 rng(2026);
    for(int i=0;i<10000;++i)
    {
        const unsigned w=4+rng()%45,h=3+rng()%30;
        std::vector<std::string> map(h,std::string(w,'#'));
        for(unsigned y=1;y+1<h;++y)for(unsigned x=1;x+1<w;++x)
            map[y][x]=std::string_view(".#CH")[rng()%4];
        map[1][1]='S';map[1][2]='G';
        std::string data="kairo-platformer 1\n";
        for(const auto& row:map)data+=row+'\n';
        check(ParsePlatformerProject(data)==map,"generated map roundtrip");
        data[data.find('\n')+1]='.';
        reject(data);
    }
    std::cout<<"10000 generated valid maps and 10000 invalid borders, plus authored route passed\n";
}
