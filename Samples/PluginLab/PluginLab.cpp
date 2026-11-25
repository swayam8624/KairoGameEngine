#include "RuntimePluginLoader.hpp"
#include <charconv>
#include <iostream>
#include <string_view>

// Explicit local trust: the host never discovers or executes plugins merely
// because they appear inside a project. The operator must choose the manifest.
int main(int argc,char** argv)
{
    try
    {
        if(argc!=4||std::string_view(argv[1])!="--trusted-plugin")
            throw std::invalid_argument("Usage: KairoPluginLab --trusted-plugin <manifest> <integer>");
        const std::string_view input=argv[3];std::int32_t value=0;
        const auto [end,error]=std::from_chars(input.data(),input.data()+input.size(),value);
        if(error!=std::errc{}||end!=input.data()+input.size())throw std::invalid_argument("Invalid integer input.");
        const kairo::player::detail::RuntimeNativePlugin plugin(argv[2]);
        std::cout<<plugin.ID()<<": "<<plugin.Invoke(value)<<'\n';
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
