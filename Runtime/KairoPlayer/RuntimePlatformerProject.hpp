#pragma once
#include "RuntimePlatformer.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace kairo::player::detail
{
    // Versioned authored tile map. Limit to a 48x32 canvas so the existing
    // native overlay can render the whole level at ordinary desktop sizes.
    inline constexpr std::string_view PlatformerProjectHeader="kairo-platformer 1";
    inline constexpr std::size_t PlatformerProjectMaxBytes=4096;

    [[nodiscard]] inline std::vector<std::string> ParsePlatformerProject(std::string_view source)
    {
        if(source.size()>PlatformerProjectMaxBytes)
            throw std::invalid_argument("Platformer document exceeds byte limit.");
        std::vector<std::string> rows;
        bool first=true;
        while(!source.empty())
        {
            const auto end=source.find('\n');
            auto line=source.substr(0,end);
            if(end==std::string_view::npos)source={};
            else source.remove_prefix(end+1);
            if(!line.empty()&&line.back()=='\r')line.remove_suffix(1);
            if(first)
            {
                first=false;
                if(line!=PlatformerProjectHeader)throw std::invalid_argument("Unsupported platformer document version.");
            }
            else
            {
                if(line.size()>48||rows.size()>=32)throw std::invalid_argument("Platformer project exceeds 48x32 tiles.");
                rows.emplace_back(line);
            }
        }
        // The simulation remains the authority for map grammar, dimensions,
        // closed border and unique spawn/goal; no second C++ validator.
        (void)RuntimePlatformer(rows);
        return rows;
    }

    [[nodiscard]] inline std::vector<std::string> LoadPlatformerProject(const std::filesystem::path& path)
    {
        std::ifstream input(path,std::ios::binary);
        if(!input)throw std::runtime_error("Cannot open platformer document.");
        std::string bytes(PlatformerProjectMaxBytes+1,'\0');
        input.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
        if(input.bad())throw std::runtime_error("Cannot read platformer document.");
        bytes.resize(static_cast<std::size_t>(input.gcount()));
        return ParsePlatformerProject(bytes);
    }
}
