#include "RuntimeWeather.hpp"
#include <array>
#include <iostream>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Light{std::array<float,3> Sky{.2f,.5f,.9f};float SunIntensity=3,AmbientIntensity=.3f;};
int main()
{
    const Light daylight;
    for(std::uint64_t seed=0;seed<100;++seed)
    {
        auto previous=RuntimeWeather::At(seed,0);
        for(std::uint64_t second=1;second<=1200;++second)
        {
            const auto w=RuntimeWeather::At(seed,second);
            Check(w==RuntimeWeather::At(seed,second)&&w.Rain<=w.Cloud&&w.Cloud<=16,"Weather bounds/determinism");
            Check(std::abs(int(w.Cloud)-int(previous.Cloud))<=2&&std::abs(int(w.Rain)-int(previous.Rain))<=2,"Discontinuous transition");
            const auto l=RuntimeWeather::Apply(daylight,w);
            Check(l.SunIntensity>=0&&l.SunIntensity<=3&&l.AmbientIntensity>=0&&l.AmbientIntensity<=.3f,"Lighting bounds");
            previous=w;
        }
        Check(RuntimeWeather::At(seed,0)==RuntimeWeatherSample{},"Initial clear");
        Check(RuntimeWeather::At(seed,38).Cloud>=10&&RuntimeWeather::At(seed,68).Rain>=8,"Weather cycle");
        Check(RuntimeWeather::At(seed,98)==RuntimeWeatherSample{},"Clear recovery");
    }
    bool rejected=false;try{(void)RuntimeWeather::Apply(daylight,{0,1});}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Invalid weather accepted");
    std::cout<<"120000 weather transitions and lighting samples passed\n";
}
