#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>

namespace kairo::player::detail
{
    // Release-to-arm edge sampling prevents held keys/clicks from firing
    // upon opening a menu or regaining focus. Each control has its own latch.
    class MenuEdge
    {
    public:
        void Reset() noexcept{m_Armed=false;}
        bool Press(bool down) noexcept
        {
            if(!down){m_Armed=true;return false;}
            if(!m_Armed)return false;
            m_Armed=false;return true;
        }
    private:
        bool m_Armed=false;
    };
    // A focus index over the canonical RuntimeUIScene::FocusOrder. The host
    // owns the widgets; this class only advances and validates indices.
    class RuntimeMenuFocus
    {
    public:
        explicit RuntimeMenuFocus(std::size_t count):m_Count(count)
        {if(!count||count>64)throw std::invalid_argument("Invalid menu focus count.");}
        [[nodiscard]] std::size_t Index() const noexcept{return m_Index;}
        void Move(bool reverse) noexcept
        {m_Index=reverse?(m_Index+m_Count-1)%m_Count:(m_Index+1)%m_Count;}
        void Select(std::size_t index)
        {if(index>=m_Count)throw std::out_of_range("Menu focus index.");m_Index=index;}
        // A mouse press arms exactly one target; releasing over another
        // target or outside cancels. Keyboard movement cancels pointer capture.
        void PointerDown(std::optional<std::size_t> target)
        {
            if(target)Select(*target);
            m_Pressed=target;
        }
        [[nodiscard]] bool PointerUp(std::optional<std::size_t> target) noexcept
        {const bool activate=target&&target==m_Pressed;m_Pressed.reset();return activate;}
        void CancelPointer() noexcept{m_Pressed.reset();}
    private:
        std::size_t m_Count,m_Index=0;
        std::optional<std::size_t> m_Pressed;
    };
    // Compact 5x7 uppercase bitmap alphabet. Low five bits form each row,
    // most significant bit at the left. Lowercase folds to uppercase; unsupported
    // punctuation uses a visible question mark, not an invisible label.
    [[nodiscard]] inline std::array<unsigned,7> MenuGlyph(char c) noexcept
    {
        if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');
        switch(c)
        {
            case 'A':return {14,17,17,31,17,17,17}; case 'B':return {30,17,17,30,17,17,30};
            case 'C':return {14,17,16,16,16,17,14}; case 'D':return {30,17,17,17,17,17,30};
            case 'E':return {31,16,16,30,16,16,31}; case 'F':return {31,16,16,30,16,16,16};
            case 'G':return {14,17,16,23,17,17,15}; case 'H':return {17,17,17,31,17,17,17};
            case 'I':return {14,4,4,4,4,4,14}; case 'J':return {7,2,2,2,18,18,12};
            case 'K':return {17,18,20,24,20,18,17}; case 'L':return {16,16,16,16,16,16,31};
            case 'M':return {17,27,21,21,17,17,17}; case 'N':return {17,25,21,19,17,17,17};
            case 'O':return {14,17,17,17,17,17,14}; case 'P':return {30,17,17,30,16,16,16};
            case 'Q':return {14,17,17,17,21,18,13}; case 'R':return {30,17,17,30,20,18,17};
            case 'S':return {15,16,16,14,1,1,30}; case 'T':return {31,4,4,4,4,4,4};
            case 'U':return {17,17,17,17,17,17,14}; case 'V':return {17,17,17,17,17,10,4};
            case 'W':return {17,17,17,21,21,21,10}; case 'X':return {17,17,10,4,10,17,17};
            case 'Y':return {17,17,10,4,4,4,4}; case 'Z':return {31,1,2,4,8,16,31};
            case '0':return {14,17,19,21,25,17,14}; case '1':return {4,12,4,4,4,4,14};
            case '2':return {14,17,1,2,4,8,31}; case '3':return {30,1,1,14,1,1,30};
            case '4':return {2,6,10,18,31,2,2}; case '5':return {31,16,16,30,1,1,30};
            case '6':return {14,16,16,30,17,17,14}; case '7':return {31,1,2,4,8,8,8};
            case '8':return {14,17,17,14,17,17,14}; case '9':return {14,17,17,15,1,1,14};
            case ' ':return {}; case '-':return {0,0,0,31,0,0,0};
            case '.':return {0,0,0,0,0,12,12}; default:return {14,17,1,2,4,0,4};
        }
    }
}
