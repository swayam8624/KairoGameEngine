#include "../RuntimeAlsaAudioOutput.hpp"
#include <array>
#include <iostream>

int main()
{
    using namespace kairo::player::detail;
    // ALSA's null PCM exercises real device configuration, nonblocking
    // writes and lifecycle without requiring a physical sound card.
    for(unsigned run=0;run<4;++run)
    {
        RuntimeAlsaAudioOutput output(48000,"null");
        std::array<float,2048> pcm{};
        for(std::size_t i=0;i<pcm.size();++i)pcm[i]=(i%2)?.25f:-.25f;
        if(output.Push(pcm)!=1024)throw std::runtime_error("Initial PCM push failed.");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
        while(!output.Failed()&&output.AcceptedFrames()<1024&&std::chrono::steady_clock::now()<deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if(output.Failed()||output.AcceptedFrames()<1024)throw std::runtime_error("ALSA null device write failed.");
    }
    bool rejected=false;
    try{RuntimeAlsaAudioOutput invalid(0,"null");}catch(const std::invalid_argument&){rejected=true;}
    if(!rejected)throw std::runtime_error("Invalid native rate accepted.");
    std::cout<<"Four ALSA null-device open/write/close cycles passed\n";
}
