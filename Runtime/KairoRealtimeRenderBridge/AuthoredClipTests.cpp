#include "AuthoredClip.hpp"
#include <iostream>
#include <random>
using namespace kairo::runtime::renderbridge::clip_detail;
struct Key {float TimeSeconds=0;std::array<float,4> Value{},InTangent{},OutTangent{};bool operator==(const Key&)const=default;};
struct Track {std::uint32_t TargetNode=0;unsigned Path=1,Interpolation=1;std::vector<Key> Keyframes;bool operator==(const Track&)const=default;};
struct Clip {std::string Name;std::vector<Track> Channels;bool operator==(const Clip&)const=default;};
void Check(bool ok){if(!ok)throw std::runtime_error("clip regression");}
template<class F> void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Check(rejected);}
int main(){
    Clip clip{"Test \"clip\"",{{0,1,1,{{0,{1,2,3,0},{},{}},{1,{4,5,6,0},{},{}}}}}};
    const auto bytes=Encode("source",clip);
    Check(Decode<Clip>("source",bytes)==clip);
    Reject([&]{(void)Decode<Clip>("other",bytes);});
    Reject([&]{(void)Decode<Clip>("source",bytes+"extra");});
    Reject([&]{(void)Decode<Clip>("source",bytes.substr(0,bytes.size()-5));});
    Reject([&]{(void)Decode<Clip>("source","kairo.clip.v1 source\n\"name\" 257\n");});
    auto invalid=clip;invalid.Channels[0].Keyframes[1].TimeSeconds=0;
    Reject([&]{(void)Encode("source",invalid);});
    invalid=clip;invalid.Channels[0].Path=2;
    Reject([&]{(void)Encode("source",invalid);});
    invalid=clip;invalid.Channels.push_back(invalid.Channels.front());
    Reject([&]{(void)Encode("source",invalid);});
    std::mt19937 rng(2026);std::uniform_real_distribution<float> random(-1000,1000);
    for(unsigned i=0;i<10000;++i){for(auto& key:clip.Channels[0].Keyframes)for(float& v:key.Value)v=random(rng);Check(Decode<Clip>("source",Encode("source",clip))==clip);}
    std::cout<<"10000 authored clip round trips and rejection fixtures passed\n";
}
