#pragma once
#include <charconv>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace kairo::player::detail
{
    enum class DebugAction { Continue, Step, Detach };
    struct LogicBreakpoint
    {
        std::string_view Document,Revision;
        std::uint32_t PC=0;
    };
    // Exactly one stop token is issued per paused instruction. Commands for
    // any other stop must not resume it. Step arms the next instruction,
    // not the next numerical PC (jumps and new dispatches are allowed).
    class LogicDebugGate
    {
        bool m_Attached=true,m_Step=false,m_Paused=false;
        std::uint32_t m_Stop=0;
    public:
        explicit LogicDebugGate(bool breakFirst=false):m_Step(breakFirst){}
        [[nodiscard]] bool Attached() const noexcept{return m_Attached;}
        [[nodiscard]] bool Paused() const noexcept{return m_Paused;}
        [[nodiscard]] std::uint32_t Stop() const noexcept{return m_Stop;}
        bool Before(std::string_view document,std::string_view revision,std::uint32_t pc,
            std::optional<LogicBreakpoint> breakpoint) noexcept
        {
            if(!m_Attached||m_Paused)return false;
            const bool hit=breakpoint&&breakpoint->Document==document&&breakpoint->Revision==revision&&breakpoint->PC==pc;
            if(!m_Step&&!hit)return false;
            if(m_Stop==4096){Detach();return false;}
            ++m_Stop;m_Step=false;m_Paused=true;return true;
        }
        bool Apply(std::uint32_t stop,DebugAction action) noexcept
        {
            if(!m_Attached||!m_Paused||stop!=m_Stop)return false;
            if(action==DebugAction::Detach){Detach();return true;}
            if(action!=DebugAction::Continue&&action!=DebugAction::Step)return false;
            m_Step=action==DebugAction::Step;m_Paused=false;return true;
        }
        void Detach() noexcept{m_Attached=false;m_Paused=false;m_Step=false;}
    };
    inline std::optional<DebugAction> ParseDebugCommand(std::string_view bytes,std::uint32_t stop)
    {
        if(bytes.size()>64||bytes.empty()||bytes.back()!='\n')return std::nullopt;
        bytes.remove_suffix(1);
        const auto space=bytes.find(' ');
        if(space==std::string_view::npos)return std::nullopt;
        std::uint32_t value=0;
        const auto first=bytes.data(),last=first+space;
        const auto result=std::from_chars(first,last,value);
        if(result.ec!=std::errc{}||result.ptr!=last||value!=stop)return std::nullopt;
        const auto action=bytes.substr(space+1);
        if(action=="continue")return DebugAction::Continue;
        if(action=="step")return DebugAction::Step;
        if(action=="detach")return DebugAction::Detach;
        return std::nullopt;
    }
}
