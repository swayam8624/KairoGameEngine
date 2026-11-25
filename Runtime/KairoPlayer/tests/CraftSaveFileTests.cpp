#include "../CraftSaveFile.hpp"
#include <chrono>
#include <iostream>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F> bool Reject(F&& fn){try{fn();return false;}catch(const std::exception&){return true;}}
struct Scratch
{
    std::filesystem::path Path;
    Scratch()
    {
        const auto base=std::filesystem::temp_directory_path();
        std::mt19937_64 rng(static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
        for(unsigned i=0;i<64;++i)
        {
            Path=base/("kairo-save-test-"+std::to_string(rng()));
            if(std::filesystem::create_directory(Path))return;
        }
        throw std::runtime_error("Could not reserve test directory.");
    }
    ~Scratch(){std::error_code ignored;std::filesystem::remove_all(Path,ignored);}
};
}
int main()
{
    Scratch scratch;
    const auto path=scratch.Path/"world.kcraft";
    auto lock=path;lock+=".write-lock";
    constexpr std::size_t limit=4096;
    std::mt19937 rng(661);
    std::vector<std::uint8_t> previous;
    for(unsigned i=0;i<256;++i)
    {
        std::vector<std::uint8_t> bytes(1+rng()%limit);
        for(auto& b:bytes)b=static_cast<std::uint8_t>(rng());
        WriteCraftSaveFile(path,bytes,limit);
        Check(ReadCraftSaveFile(path,limit)==bytes&&!std::filesystem::exists(lock),"Disk replacement differs");
        previous=std::move(bytes);
    }
    Check(Reject([&]{WriteCraftSaveFile(path,previous,previous.size()-1);}),"Oversize write accepted");
    Check(Reject([&]{(void)ReadCraftSaveFile(path,previous.size()-1);}),"Oversize read accepted");
    Check(Reject([&]{WriteCraftSaveFile(path,{},limit);}),"Empty write accepted");
    Check(std::filesystem::create_directory(lock),"Could not create competing reservation");
    {std::ofstream marker(lock/"payload");marker<<"other writer";}
    Check(Reject([&]{WriteCraftSaveFile(path,previous,limit);}),"Competing reservation ignored");
    Check(std::filesystem::exists(lock/"payload")&&ReadCraftSaveFile(path,limit)==previous,"Competing writer or prior save damaged");
    std::filesystem::remove_all(lock);
    const auto directory=scratch.Path/"directory";
    std::filesystem::create_directory(directory);
    {std::ofstream marker(directory/"keep");marker<<"preserve";}
    Check(Reject([&]{WriteCraftSaveFile(directory,previous,limit);}),"Directory replaced by save");
    auto failedLock=directory;failedLock+=".write-lock";
    Check(std::filesystem::exists(directory/"keep")&&!std::filesystem::exists(failedLock),"Failed replacement leaked reservation or damaged target");
    Check(Reject([&]{(void)ReadCraftSaveFile(directory,limit);}),"Directory read accepted");
    Check(Reject([&]{(void)ReadCraftSaveFile(scratch.Path/"missing",limit);}),"Missing save accepted");
    Check(ReadCraftSaveFile(path,limit)==previous,"Failed writes damaged previous save");
    std::cout<<"256 actual file replacements, bounds, competing reservation, failure cleanup and prior save preservation passed\n";
}
