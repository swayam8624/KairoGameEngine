#include "RuntimeMenuDocument.hpp"
#include <iostream>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()
{
    // Canonical ordering and quoted labels are stable across save/load.
    RuntimeMenuDocument authored{{"02-quit","Quit","quit",100,500,800,200},{"01-play","Play \"game\"","resume",100,100,800,200}};
    const auto text=SerializeRuntimeMenu(authored);
    const auto parsed=ParseRuntimeMenu(text);
    Check(parsed.size()==2&&parsed[0]==authored[1]&&SerializeRuntimeMenu(parsed)==text,"Canonical roundtrip");
    // Reproduce a Windows checkout read in binary mode, including blank lines.
    std::string crlf;
    for(char c:text){if(c=='\n')crlf+='\r';crlf+=c;}
    Check(ParseRuntimeMenu(crlf)==parsed,"CRLF checkout rejected");
    Check(SerializeRuntimeMenu(ParseRuntimeMenu(crlf))==text,"CRLF canonicalization failed");
    Check(ParseRuntimeMenu("kairo-menu 1\r\n\r\nbutton x a resume 0 0 10 10\n")==RuntimeMenuDocument{{"x","a","resume",0,0,10,10}},"Mixed line endings rejected");
    for(const auto* invalid:{"", "kairo-menu 2\n", "kairo-menu 1\n", "kairo-menu 1\nbutton x a shell 0 0 10 10\n", "kairo-menu 1\nbutton x a resume 999 0 2 10\n", "kairo-menu 1\nbutton x a resume 0 0 10 10\nbutton y b quit 5 5 10 10\n", "kairo-menu 1\nbutton x a resume 0 0 10 10\nbutton x b quit 20 20 10 10\n", "kairo-menu 1\r\r\nbutton x a resume 0 0 10 10\n", "kairo-menu 1\nbutton x \"bad\rlabel\" resume 0 0 10 10\n"})
    {
        bool rejected=false;try{(void)ParseRuntimeMenu(invalid);}catch(const std::exception&){rejected=true;}
        Check(rejected,"Invalid document accepted");
    }
    for(unsigned i=1;i<=1000;++i)
    {
        const auto doc=RuntimeMenuDocument{{"play","Play","resume",0,0,i,1}};
        Check(ParseRuntimeMenu(SerializeRuntimeMenu(doc))==doc,"Geometry roundtrip failed");
    }
    std::cout<<"1000 authored menu roundtrips, CRLF and rejection fixtures passed\n";
}
