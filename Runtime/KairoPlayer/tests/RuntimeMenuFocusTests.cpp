#include "RuntimeMenuFocus.hpp"
#include <iostream>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()
{
    MenuEdge edge;Check(!edge.Press(true)&&!edge.Press(false)&&edge.Press(true)&&!edge.Press(true),"Edge arming");
    edge.Reset();Check(!edge.Press(true),"Focus reset");
    for(std::size_t count=1;count<=64;++count)
    {
        RuntimeMenuFocus focus(count);
        for(std::size_t i=0;i<count*10;++i){Check(focus.Index()==i%count,"Forward order");focus.Move(false);}
        for(std::size_t i=0;i<count*10;++i){Check(focus.Index()==(count-i%count)%count,"Reverse order");focus.Move(true);}
        focus.PointerDown(0);Check(!focus.PointerUp({}),"Release outside activated");
        focus.PointerDown(0);Check(focus.PointerUp(0)&&!focus.PointerUp(0),"Pointer activation");
        focus.PointerDown(0);focus.CancelPointer();Check(!focus.PointerUp(0),"Cancelled pointer activated");
    }
    for(char c='A';c<='Z';++c)
    {
        const auto glyph=MenuGlyph(c);Check(glyph==MenuGlyph(static_cast<char>(c-'A'+'a')),"Case fold");
        unsigned pixels=0;for(auto row:glyph){Check(row<32,"Glyph exceeds width");pixels|=row;}Check(pixels!=0,"Empty letter");
    }
    std::cout<<"64 focus sizes, 41600 focus movements, pointer and glyph checks passed\n";
}
