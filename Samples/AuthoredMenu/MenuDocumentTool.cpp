#include "RuntimeMenuDocument.hpp"
#include <iostream>
#include <string_view>
#ifdef _WIN32
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#endif

// Authoring tools pipe candidate bytes through the exact Player parser.
// Emit only canonical LF bytes on success; diagnostics go to stderr.
int main()
{
    try
    {
#ifdef _WIN32
        // The Python authoring tool transports bytes, not Windows text-mode
        // streams. Preserve CRLF on input and canonical LF on output.
        if(_setmode(_fileno(stdin),_O_BINARY)==-1||_setmode(_fileno(stdout),_O_BINARY)==-1)
            throw std::runtime_error("Cannot set binary menu streams.");
#endif
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
