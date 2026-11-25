#pragma once

namespace kairo::player::detail
{
    // A discrete action is permitted once per press, after an observed
    // release in the active session. Reset on focus transitions so held
    // keys cannot synthesize a fresh craft, eat, save or jump action.
    class CraftActionLatch
    {
        bool m_Armed=false;
    public:
        void Reset() noexcept{m_Armed=false;}
        bool Press(bool down) noexcept
        {
            if(!down){m_Armed=true;return false;}
            if(!m_Armed)return false;
            m_Armed=false;return true;
        }
    };
    struct CraftActionInputs
    {
        CraftActionLatch Jump,RecipeOne,RecipeTwo,CraftTool,ToggleTool,Eat,Save;
        void Reset() noexcept{*this=CraftActionInputs{};}
    };
}
