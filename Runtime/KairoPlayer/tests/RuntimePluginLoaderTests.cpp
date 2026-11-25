#include "RuntimePluginLoader.hpp"
#include <array>
#include <chrono>
#include <iostream>
#include <limits>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv)
{
    if(argc!=3)throw std::invalid_argument("Expected good and incompatible native libraries.");
    const auto root=std::filesystem::temp_directory_path()/("kairo-plugin-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if(!std::filesystem::create_directory(root))throw std::runtime_error("Temp directory exists.");
    struct Cleanup{std::filesystem::path Path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(Path,ec);}} cleanup{root};
    const auto good=std::filesystem::path(argv[1]).filename(),bad=std::filesystem::path(argv[2]).filename();
    std::filesystem::copy_file(argv[1],root/good);std::filesystem::copy_file(argv[2],root/bad);
    const auto manifest=root/"plugin.kplugin";
    const auto write=[&](const std::string& id,const std::string& abi,const std::string& library)
    {std::ofstream out(manifest);out<<"kairo-plugin 1\nid "<<id<<"\nabi "<<abi<<"\nlibrary "<<library<<'\n';};
    write("double-value","1",good.string());
    {
        RuntimeNativePlugin plugin(manifest);Check(plugin.ID()=="double-value","Identity mismatch");
        for(int i=-5000;i<=5000;++i)Check(plugin.Invoke(i)==i*2,"Plugin result mismatch");
        bool rejected=false;try{(void)plugin.Invoke(std::numeric_limits<int32_t>::max());}catch(const std::runtime_error&){rejected=true;}
        Check(rejected,"Overflow accepted");
    }
    for(const auto& fixture:std::array<std::array<std::string,3>,4>{{{{"double-value","2",good.string()}},{{"wrong-id","1",good.string()}},{{"double-value","1",bad.string()}},{{"double-value","1","../escape.so"}}}})
    {
        write(fixture[0],fixture[1],fixture[2]);bool rejected=false;
        try{RuntimeNativePlugin plugin(manifest);}catch(const std::exception&){rejected=true;}
        Check(rejected,"Invalid plugin accepted");
    }
    std::cout<<"10001 native plugin calls and four rejection fixtures passed\n";
}
