#pragma once
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace kairo::player::detail
{
    // Project menu document v1: a flat list of buttons in integer thousandths
    // of the viewport. Stable IDs sort the keyboard order; labels are literal
    // printable ASCII for this first portable authoring slice. Actions are
    // host-owned resume/quit commands, never executable source or shell text.
    struct RuntimeMenuButton
    {
        std::string ID,Label,Action;
        unsigned X=0,Y=0,W=0,H=0;
        friend bool operator==(const RuntimeMenuButton&,const RuntimeMenuButton&)=default;
    };
    using RuntimeMenuDocument=std::vector<RuntimeMenuButton>;
    inline constexpr std::size_t MaximumMenuBytes=32768;

    // Validate one canonical identifier and its human-readable accessible label.
    // Keeping control bytes out of labels also protects terminal diagnostics.
    inline void ValidateMenuButton(const RuntimeMenuButton& b)
    {
        if(b.ID.empty()||b.ID.size()>64||b.Label.empty()||b.Label.size()>64||
            !std::all_of(b.ID.begin(),b.ID.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-';})||
            !std::all_of(b.Label.begin(),b.Label.end(),[](unsigned char c){return c>=32&&c<=126;})||
            (b.Action!="resume"&&b.Action!="quit")||!b.W||!b.H||b.X>1000||b.Y>1000||b.W>1000-b.X||b.H>1000-b.Y)
            throw std::invalid_argument("Invalid authored menu button.");
    }
    // Binary reads preserve CRLF on Windows. Normalize only the line-ending
    // CR, never a CR inside a quoted label or token. Output remains LF-only.
    inline bool ReadMenuLine(std::istream& input,std::string& line)
    {
        if(!std::getline(input,line))return false;
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.find('\r')!=std::string::npos)throw std::invalid_argument("Unexpected carriage return in menu statement.");
        return true;
    }
    // Parse into a local candidate. Reject overlap, duplicate identity, empty
    // menus and unknown commands. A failed parse cannot change a live menu.
    [[nodiscard]] inline RuntimeMenuDocument ParseRuntimeMenu(std::string_view source)
    {
        if(source.size()>MaximumMenuBytes)throw std::length_error("Menu document exceeds byte budget.");
        std::istringstream input{std::string(source)};input.imbue(std::locale::classic());
        std::string line;
        if(!ReadMenuLine(input,line)||line!="kairo-menu 1")throw std::invalid_argument("Invalid menu header.");
        RuntimeMenuDocument result;
        while(ReadMenuLine(input,line))
        {
            if(line.empty())continue;
            std::istringstream row(line);row.imbue(std::locale::classic());
            RuntimeMenuButton button;std::string command;
            if(!(row>>command>>std::quoted(button.ID)>>std::quoted(button.Label)>>button.Action>>button.X>>button.Y>>button.W>>button.H)||command!="button")
                throw std::invalid_argument("Malformed menu statement.");
            row>>std::ws;if(!row.eof())throw std::invalid_argument("Trailing menu fields.");
            ValidateMenuButton(button);
            if(result.size()>=64)throw std::length_error("Menu button budget exceeded.");
            for(const auto& other:result)
            {
                if(other.ID==button.ID)throw std::invalid_argument("Duplicate menu ID.");
                if(button.X<other.X+other.W&&other.X<button.X+button.W&&button.Y<other.Y+other.H&&other.Y<button.Y+button.H)
                    throw std::invalid_argument("Overlapping menu buttons.");
            }
            result.push_back(std::move(button));
        }
        if(result.empty())throw std::invalid_argument("Empty authored menu.");
        std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.ID<b.ID;});
        return result;
    }
    // Canonical authoring round-trip: serialize in ID order, then parse the
    // generated document to enforce exactly the same validation as Player.
    [[nodiscard]] inline std::string SerializeRuntimeMenu(RuntimeMenuDocument document)
    {
        std::sort(document.begin(),document.end(),[](const auto& a,const auto& b){return a.ID<b.ID;});
        std::ostringstream out;out.imbue(std::locale::classic());out<<"kairo-menu 1\n";
        for(const auto& b:document)
        {
            ValidateMenuButton(b);
            out<<"button "<<std::quoted(b.ID)<<' '<<std::quoted(b.Label)<<' '<<b.Action<<' '<<b.X<<' '<<b.Y<<' '<<b.W<<' '<<b.H<<'\n';
        }
        auto result=out.str();(void)ParseRuntimeMenu(result);return result;
    }
}
