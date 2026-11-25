#pragma once
#include <cstdint>

namespace kairo::player::detail
{
    // A game frame requires focus and a drawable framebuffer. Inactive
    // frames still poll events, but never simulate or submit native work.
    // The renderer and simulation share this predicate; a zero-sized or
    // minimized framebuffer must not receive a native draw.
    [[nodiscard]] inline constexpr bool CraftFrameActive(bool focused,bool minimized,
        std::uint32_t width,std::uint32_t height) noexcept
    {return focused&&!minimized&&width!=0&&height!=0;}

    // A discrete action is permitted once per press, after an observed
    // release in the active session. Reset on focus transitions so held
    // keys cannot synthesize a fresh craft, eat, save or jump action.
    class CraftActionLatch
    {
        bool m_Armed=false;
    public:
        // Reset on activation transition: held keys must be released again.
        void Reset() noexcept{m_Armed=false;}
        // Release arms; first subsequent down consumes the arm. Different
        // from RuntimeControlGate, which returns held state every active frame.
        bool Press(bool down) noexcept
        {
            if(!down){m_Armed=true;return false;}
            if(!m_Armed)return false;
            m_Armed=false;return true;
        }
    };
    // Independent latch for every discrete gameplay command. Continuous
    // movement is handled by the player controller, not these edge latches.
    struct CraftActionInputs
    {
        CraftActionLatch Jump,RecipeOne,RecipeTwo,CraftTool,ToggleTool,Eat,Save;
        // Disarm all discrete actions together, preserving no stale edge.
        void Reset() noexcept{*this=CraftActionInputs{};}
    };
}
