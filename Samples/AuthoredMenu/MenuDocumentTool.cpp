#include "RuntimeMenuDocument.hpp"
#include <iostream>
#include <string_view>

// Authoring tools pipe candidate bytes through the exact Player parser.
// Emit only canonical LF bytes on success; diagnostics go to stderr.
int main()
{
    try
    {
        using namespace kairo::player::detail;
        char buffer[MaximumMenuBytes+1];
        std::cin.read(buffer,sizeof(buffer));
        if(std::cin.bad()||std::cin.gcount()>static_cast<std::streamsize>(MaximumMenuBytes))
            throw std::runtime_error("Menu input exceeds byte budget or read failed.");
        const auto menu=ParseRuntimeMenu(std::string_view(buffer,static_cast<std::size_t>(std::cin.gcount())));
        std::cout<<SerializeRuntimeMenu(menu);
        return std::cout?0:1;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
