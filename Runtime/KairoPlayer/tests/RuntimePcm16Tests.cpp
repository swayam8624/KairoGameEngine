#include "../RuntimePcm16.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <random>
namespace {
using namespace kairo::player::detail;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
int main()
{
    for(int code=-32768;code<=32767;++code)
    {
        const float normalized=static_cast<float>(double(code)/(code<0?32768.:32767.));
        Check(RuntimeFloatToPcm16(normalized)==code,"PCM16 level did not roundtrip");
    }
    Check(RuntimeFloatToPcm16(-2)==-32768&&RuntimeFloatToPcm16(2)==32767,"Clipping endpoints differ");
    Check(RuntimeFloatToPcm16(std::numeric_limits<float>::infinity())==0&&RuntimeFloatToPcm16(std::numeric_limits<float>::quiet_NaN())==0,"Nonfinite sample was not silent");
    std::mt19937 rng(419);std::array<float,1024> input{};std::array<std::int16_t,1024> output{};
    for(unsigned pass=0;pass<100;++pass)
    {
        for(auto& v:input)v=static_cast<float>(int(rng()%400001)-200000)/100000.f;
        ConvertRuntimePcm16(input,output);
        for(std::size_t i=0;i<input.size();++i)
        {
            const double v=std::max(-1.,std::min(1.,double(input[i])));
            const double target=v*(v<0?32768.:32767.);
            Check(std::abs(double(output[i])-target)<=.50000001,"Conversion exceeded half-LSB error");
        }
    }
    bool rejected=false;try{ConvertRuntimePcm16(input,std::span(output).first(1));}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Mismatched conversion size accepted");
    std::cout<<"65536 PCM16 levels and 102400 random sample conversions passed\n";
}
