#pragma once
#include "RuntimeMenuFocus.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace kairo::player::ui2d
{
    inline constexpr std::size_t MaxBytes=16384,MaxWidgets=24,MaxRects=2048;
    inline void Check(bool ok,const char* message){if(!ok)throw std::invalid_argument(message);}
    enum class Screen {Play,Pause,Complete};
    enum class Kind {Panel,Label,Bar,Button,Counter};
    enum class Anchor {TopLeft,Top,TopRight,Left,Center,Right,BottomLeft,Bottom,BottomRight};
    enum class Binding {None,Coins,Deaths,Level,Title,Script};
    enum class Action {None,Pause,Resume,Restart,NewGame,Quit};
    struct Box {int X=0,Y=0,W=0,H=0;friend bool operator==(Box,Box)=default;};
    struct Rect {Box Bounds;unsigned RGB=0;};
    struct Widget
    {
        std::string ID;
        Screen Page=Screen::Play;
        Kind Type=Kind::Panel;
        Anchor Pin=Anchor::TopLeft;
        Box Bounds;
        unsigned Background=0x202832,Foreground=0xffffff;
        Binding Value=Binding::None;
        Action Command=Action::None;
        std::string Text;
    };
    struct Document {std::vector<Widget> Widgets;};
    struct State {unsigned Coins=0,Total=0,Deaths=0,Level=1,Levels=1;std::string Title;int ScriptValue=0;};
    template<class E,std::size_t N> E Decode(std::string_view s,const std::array<std::string_view,N>& names)
    {
        const auto i=std::find(names.begin(),names.end(),s);
        Check(i!=names.end(),"Invalid UI enum value.");
        return static_cast<E>(i-names.begin());
    }
    inline unsigned Color(std::string_view s)
    {
        Check(s.size()==6,"Expected six digit UI color.");
        unsigned out=0;
        for(const auto c:s)
        {
            const int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
            Check(n>=0,"Invalid UI color.");out=16*out+unsigned(n);
        }
        return out;
    }
    inline bool ID(std::string_view s)
    {
        return !s.empty()&&s.size()<=24&&std::all_of(s.begin(),s.end(),[](char c)
        {return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_';});
    }
    inline Document Parse(std::string_view bytes)
    {
        Check(bytes.size()<=MaxBytes,"UI document too large.");
        std::istringstream in{std::string(bytes)};
        std::string tag;int version=0,count=0;
        Check(bool(in>>tag>>version)&&tag=="kairo-ui2d"&&version==1,"Unsupported UI document.");
        Check(bool(in>>tag>>count)&&tag=="widgets"&&count>=1&&count<=int(MaxWidgets),"Invalid UI widget count.");
        Document doc;std::set<std::string> ids;
        std::array<unsigned,3> budget{},escape{};
        for(int i=0;i<count;++i)
        {
            Widget w;std::string page,type,anchor,bg,fg,binding,action;
            Check(bool(in>>w.ID>>page>>type>>anchor>>w.Bounds.X>>w.Bounds.Y>>w.Bounds.W>>w.Bounds.H>>
                bg>>fg>>binding>>action>>std::quoted(w.Text)),"Invalid UI widget record.");
            Check(ID(w.ID)&&ids.insert(w.ID).second,"Invalid or duplicate UI widget ID.");
            w.Page=Decode<Screen>(page,std::array<std::string_view,3>{"play","pause","complete"});
            w.Type=Decode<Kind>(type,std::array<std::string_view,5>{"panel","label","bar","button","counter"});
            w.Pin=Decode<Anchor>(anchor,std::array<std::string_view,9>{"tl","t","tr","l","c","r","bl","b","br"});
            w.Value=Decode<Binding>(binding,std::array<std::string_view,6>{"none","coins","deaths","level","title","script"});
            w.Command=Decode<Action>(action,std::array<std::string_view,6>{"none","pause","resume","restart","newgame","quit"});
            w.Background=Color(bg);w.Foreground=Color(fg);
            const auto b=w.Bounds;
            Check(b.X>=-1000&&b.X<=1000&&b.Y>=-1000&&b.Y<=1000&&b.W>=1&&b.W<=1000&&b.H>=1&&b.H<=1000,
                "UI bounds outside limits.");
            Check(w.Text.size()<=32&&std::all_of(w.Text.begin(),w.Text.end(),[](char c){return c>=32&&c<=126;}),"Invalid UI text.");
            Check((w.Type==Kind::Button)==(w.Command!=Action::None),"Only buttons may have a command.");
            Check(w.Type==Kind::Bar?w.Value==Binding::Coins:
                w.Type==Kind::Counter?w.Value!=Binding::None&&w.Value!=Binding::Title:
                w.Type==Kind::Label?w.Value==Binding::None||w.Value==Binding::Title:w.Value==Binding::None,"Invalid UI binding.");
            if(w.Command==Action::Resume)Check(w.Page==Screen::Pause,"Resume requires pause page.");
            if(w.Command==Action::Pause)Check(w.Page==Screen::Play,"Pause requires play page.");
            if(w.Command==Action::Restart)Check(w.Page!=Screen::Complete,"Restart unavailable after completion.");
            const auto index=static_cast<unsigned>(w.Page);
            // Worst case horizontal glyph runs: at most 3 runs per row,
            // 21 per glyph. Limit authored pages before runtime.
            const unsigned letters=w.Type==Kind::Counter?unsigned(w.Text.size())+21:
                w.Value==Binding::Title?40:unsigned(w.Text.size());
            budget[index]+=w.Type==Kind::Panel?1:w.Type==Kind::Bar?2:2+21*letters;
            Check(budget[index]<=MaxRects,"UI page exceeds rectangle budget.");
            if(w.Command==Action::Resume||w.Command==Action::NewGame||w.Command==Action::Quit)++escape[index];
            doc.Widgets.push_back(std::move(w));
        }
        Check(!(in>>tag),"Trailing UI document data.");
        Check(escape[1]&&escape[2],"Pause and complete pages require an exit button.");
        return doc;
    }
    inline Box Layout(const Widget& w,unsigned width,unsigned height)
    {
        Check(width<=32768&&height<=32768,"UI viewport too large.");
        // Responsive canvas units: 1000 units along the shorter axis.
        const auto unit=double(std::min(width,height))/1000.;
        const int bw=int(std::lround(w.Bounds.W*unit)),bh=int(std::lround(w.Bounds.H*unit));
        const int ax=int(w.Pin)%3,ay=int(w.Pin)/3;
        return {(int(width)-bw)*ax/2+int(std::lround(w.Bounds.X*unit)),
            (int(height)-bh)*ay/2+int(std::lround(w.Bounds.Y*unit)),bw,bh};
    }
    inline Box Clip(Box b,unsigned w,unsigned h)
    {
        const int x=std::clamp(b.X,0,int(w)),y=std::clamp(b.Y,0,int(h));
        const int r=std::clamp(b.X+b.W,0,int(w)),d=std::clamp(b.Y+b.H,0,int(h));
        return {x,y,std::max(0,r-x),std::max(0,d-y)};
    }
    inline bool Contains(Box b,int x,int y){return x>=b.X&&y>=b.Y&&x<b.X+b.W&&y<b.Y+b.H;}
    inline std::optional<std::size_t> Hit(const Document& doc,Screen screen,unsigned w,unsigned h,int x,int y)
    {
        for(std::size_t i=doc.Widgets.size();i>0;--i)
        {
            const auto& widget=doc.Widgets[i-1];
            if(widget.Page==screen&&widget.Type==Kind::Button&&Contains(Clip(Layout(widget,w,h),w,h),x,y))return i-1;
        }
        return {};
    }
    class Pointer
    {
        std::optional<std::size_t> m_Hover,m_Pressed;
        std::optional<Screen> m_Page;
        bool m_Down=false,m_Armed=false;
    public:
        std::optional<std::size_t> Hover() const{return m_Hover;}
        std::optional<std::size_t> Pressed() const{return m_Pressed;}
        Action Update(const Document& doc,Screen screen,unsigned w,unsigned h,int x,int y,bool down,bool active)
        {
            if(!active||m_Page!=screen)
            {
                m_Page=screen;m_Hover.reset();m_Pressed.reset();m_Down=down;m_Armed=active&&!down;
                return Action::None;
            }
            m_Hover=Hit(doc,screen,w,h,x,y);
            Action action=Action::None;
            if(!down&&!m_Armed)m_Armed=true;
            if(m_Armed&&down&&!m_Down)m_Pressed=m_Hover;
            if(!down&&m_Down)
            {
                if(m_Pressed&&m_Pressed==m_Hover)action=doc.Widgets[*m_Pressed].Command;
                m_Pressed.reset();
            }
            m_Down=down;return action;
        }
    };
    // Modal keyboard focus follows document paint order. Tab/Shift+Tab
    // traverse visible buttons; Enter activates once after release-to-arm.
    class Keyboard
    {
        std::optional<Screen> m_Page;
        std::optional<std::size_t> m_Focus;
        kairo::player::detail::MenuEdge m_Tab,m_Enter;
    public:
        std::optional<std::size_t> Focus() const noexcept{return m_Focus;}
        Action Update(const Document& doc,Screen page,unsigned w,unsigned h,bool tab,bool reverse,bool enter,bool active)
        {
            if(!active||page==Screen::Play||m_Page!=page)
            {
                m_Page=page;m_Focus.reset();m_Tab.Reset();m_Enter.Reset();
            }
            if(!active||page==Screen::Play)return Action::None;
            std::array<std::size_t,MaxWidgets> visible{};std::size_t count=0;
            for(std::size_t i=0;i<doc.Widgets.size();++i)
            {
                const auto& widget=doc.Widgets[i];
                if(widget.Page!=page||widget.Type!=Kind::Button)continue;
                const auto b=Clip(Layout(widget,w,h),w,h);
                if(b.W&&b.H)visible.at(count++)=i;
            }
            const bool move=m_Tab.Press(tab),activate=m_Enter.Press(enter);
            if(!count){m_Focus.reset();return Action::None;}
            const auto end=visible.begin()+count;
            auto it=m_Focus?std::find(visible.begin(),end,*m_Focus):end;
            std::size_t index=it==end?0:static_cast<std::size_t>(it-visible.begin());
            if(move)index=reverse?(index+count-1)%count:(index+1)%count;
            m_Focus=visible[index];
            return activate?doc.Widgets[*m_Focus].Command:Action::None;
        }
    };
    inline std::string Text(const Widget& w,const State& s)
    {
        switch(w.Value)
        {
            case Binding::Script:return w.Text+std::to_string(s.ScriptValue);
            case Binding::Title:return s.Title;
            case Binding::Coins:return w.Text+std::to_string(s.Coins)+"/"+std::to_string(s.Total);
            case Binding::Deaths:return w.Text+std::to_string(s.Deaths);
            case Binding::Level:return w.Text+std::to_string(s.Level)+"/"+std::to_string(s.Levels);
            default:return w.Text;
        }
    }
    inline void Paint(const Document& doc,Screen screen,const State& state,const Pointer& pointer,
        unsigned width,unsigned height,std::vector<Rect>& out,std::optional<std::size_t> focus={})
    {
        out.clear();
        const auto draw=[&](Box b,unsigned color)
        {
            b=Clip(b,width,height);
            if(b.W&&b.H)out.push_back({b,color});
        };
        for(std::size_t i=0;i<doc.Widgets.size();++i)
        {
            const auto& w=doc.Widgets[i];if(w.Page!=screen)continue;
            const auto b=Layout(w,width,height);if(b.W<=0||b.H<=0)continue;
            if(w.Type!=Kind::Label&&w.Type!=Kind::Counter)
            {
                if(w.Type==Kind::Button&&(pointer.Hover()==i||pointer.Pressed()==i||focus==i))draw(b,w.Foreground);
                const int inset=w.Type==Kind::Button&&(pointer.Hover()==i||pointer.Pressed()==i||focus==i)?2:0;
                draw({b.X+inset,b.Y+inset,b.W-2*inset,b.H-2*inset},w.Background);
            }
            if(w.Type==Kind::Bar)
            {
                const double fraction=state.Total?std::min(1.,double(state.Coins)/state.Total):1.;
                draw({b.X,b.Y,int(b.W*fraction),b.H},w.Foreground);
            }
            if(w.Type==Kind::Label||w.Type==Kind::Counter||w.Type==Kind::Button)
            {
                const auto text=Text(w,state);
                const int scale=std::min(b.H/9,b.W/std::max(1,int(text.size())*6+2));
                if(scale<=0)continue;
                int x=b.X+(b.W-int(text.size())*6*scale)/2;
                const int y=b.Y+(b.H-7*scale)/2;
                for(char c:text)
                {
                    const auto glyph=c=='/'?std::array<unsigned,7>{1,1,2,4,8,16,16}:kairo::player::detail::MenuGlyph(c);
                    for(int row=0;row<7;++row)for(int col=0;col<5;)
                    {
                        if(!(glyph[row]&(1u<<(4-col)))){++col;continue;}
                        int end=col+1;while(end<5&&(glyph[row]&(1u<<(4-end))))++end;
                        draw({x+col*scale,y+row*scale,(end-col)*scale,scale},w.Foreground);col=end;
                    }
                    x+=6*scale;
                }
            }
        }
        if(out.size()>MaxRects)throw std::logic_error("UI exceeded validated paint budget.");
    }
}
