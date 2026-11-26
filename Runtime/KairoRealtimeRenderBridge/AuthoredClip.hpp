#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kairo::runtime::renderbridge::clip_detail
{
    inline constexpr std::size_t MaxBytes=8u*1024u*1024u;
    inline constexpr std::size_t MaxTracks=256,MaxKeys=32768;
    inline bool SafeName(std::string_view name)
    {
        if(name.empty()||name.size()>128)return false;
        for(unsigned char c:name)if(c<32||c==127)return false;
        return true;
    }
    template<class Clip> void Validate(const Clip& clip)
    {
        if(!SafeName(clip.Name)||clip.Channels.empty()||clip.Channels.size()>MaxTracks)
            throw std::invalid_argument("Invalid authored clip name or track count.");
        std::size_t total=0;
        std::set<std::pair<std::uint32_t,unsigned>> tracks;
        for(const auto& track:clip.Channels)
        {
            const auto path=static_cast<unsigned>(track.Path),mode=static_cast<unsigned>(track.Interpolation);
            if(path<1||path>3||mode<1||mode>3||track.Keyframes.empty()||
                track.Keyframes.size()>MaxKeys-total||!tracks.emplace(track.TargetNode,path).second)
                throw std::invalid_argument("Invalid authored animation track.");
            total+=track.Keyframes.size();
            float previous=-1;
            for(const auto& key:track.Keyframes)
            {
                if(!std::isfinite(key.TimeSeconds)||key.TimeSeconds<0||key.TimeSeconds<=previous)
                    throw std::invalid_argument("Key times must be strictly increasing and finite.");
                previous=key.TimeSeconds;
                for(const auto* values:{&key.Value,&key.InTangent,&key.OutTangent})
                    for(float v:*values)if(!std::isfinite(v))throw std::invalid_argument("Nonfinite key value.");
                if(path==2)
                {
                    double norm=0;
                    for(float v:key.Value)norm+=double(v)*v;
                    if(std::abs(norm-1)>1e-3)throw std::invalid_argument("Rotation key must be a unit quaternion.");
                }
            }
        }
    }
    template<class Clip> std::string Encode(std::string_view source,const Clip& clip)
    {
        Validate(clip);
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out<<std::setprecision(std::numeric_limits<float>::max_digits10);
        out<<"kairo.clip.v1 "<<source<<'\n'<<std::quoted(clip.Name)<<' '<<clip.Channels.size()<<'\n';
        for(const auto& track:clip.Channels)
        {
            out<<track.TargetNode<<' '<<static_cast<unsigned>(track.Path)<<' '
                <<static_cast<unsigned>(track.Interpolation)<<' '<<track.Keyframes.size()<<'\n';
            for(const auto& key:track.Keyframes)
            {
                out<<key.TimeSeconds;
                for(const auto* values:{&key.Value,&key.InTangent,&key.OutTangent})
                    for(float v:*values)out<<' '<<v;
                out<<'\n';
            }
        }
        auto bytes=out.str();
        if(bytes.size()>MaxBytes)throw std::length_error("Authored clip exceeds byte budget.");
        return bytes;
    }
    template<class Clip> Clip Decode(std::string_view source,std::string_view bytes)
    {
        if(bytes.size()>MaxBytes)throw std::length_error("Authored clip exceeds byte budget.");
        std::istringstream in{std::string(bytes)};
        in.imbue(std::locale::classic());
        std::string magic,revision;
        Clip clip;
        std::size_t tracks=0,total=0;
        if(!(in>>magic>>revision)||magic!="kairo.clip.v1"||revision!=source)
            throw std::invalid_argument("Authored clip source revision is stale or invalid.");
        in>>std::ws;
        if(in.peek()!='"'||!(in>>std::quoted(clip.Name)>>tracks)||!SafeName(clip.Name)||!tracks||tracks>MaxTracks)
            throw std::invalid_argument("Invalid authored clip header.");
        clip.Channels.resize(tracks);
        for(auto& track:clip.Channels)
        {
            unsigned path=0,mode=0;
            std::size_t keys=0;
            if(!(in>>track.TargetNode>>path>>mode>>keys)||path<1||path>3||mode<1||mode>3||!keys||keys>MaxKeys-total)
                throw std::invalid_argument("Invalid authored track header.");
            total+=keys;
            track.Path=static_cast<decltype(track.Path)>(path);
            track.Interpolation=static_cast<decltype(track.Interpolation)>(mode);
            track.Keyframes.resize(keys);
            for(auto& key:track.Keyframes)
            {
                if(!(in>>key.TimeSeconds))throw std::invalid_argument("Missing key time.");
                for(auto* values:{&key.Value,&key.InTangent,&key.OutTangent})
                    for(float& v:*values)if(!(in>>v))throw std::invalid_argument("Missing key value.");
            }
        }
        in>>std::ws;
        if(!in.eof())throw std::invalid_argument("Trailing authored clip data.");
        Validate(clip);
        return clip;
    }
}
