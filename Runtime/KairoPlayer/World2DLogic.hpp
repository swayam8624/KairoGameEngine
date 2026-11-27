#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace kairo::player::logic2d
{
    inline constexpr std::size_t MaxBytes=16384,MaxRules=64,Slots=16;
    enum class Event {Begin,Tick,Collect,Death,Complete,Input,SensorBegin,SensorEnd,UI,Timer};
    enum class Op {Print,Position,Impulse,Add,Set,Sound};
    struct Rule
    {
        std::string Level,Argument,Message;
        Event When=Event::Begin;
        Op Command=Op::Print;
        unsigned Selector=0,Actor=0,Slot=0,SelectorSound=0;
        double X=0,Y=0;
    };
    struct Document {std::vector<Rule> Rules;};
    inline void Check(bool ok,const char* why){if(!ok)throw std::invalid_argument(why);}
    inline bool Token(std::string_view s)
    {
        if(s.empty()||s.size()>64)return false;
        for(char c:s)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return false;
        return true;
    }
    inline Document Parse(std::string_view bytes)
    {
        Check(!bytes.empty()&&bytes.size()<=MaxBytes,"Invalid logic document size.");
        std::istringstream in{std::string(bytes)};in.imbue(std::locale::classic());
        std::string tag;int version=0,count=0;
        Check(bool(in>>tag>>version)&&tag=="kairo-logic2d"&&version==1,"Invalid logic document.");
        Check(bool(in>>tag>>count)&&tag=="rules"&&count>=1&&count<=int(MaxRules),"Invalid logic rule count.");
        Document doc;
        for(int i=0;i<count;++i)
        {
            Rule r;std::string event,op;
            Check(bool(in>>r.Level>>event)&&Token(r.Level),"Invalid logic level/event.");
            constexpr std::array<std::string_view,10> events{"begin","tick","collect","death","complete","input","sensor_begin","sensor_end","ui","timer"};
            unsigned e=0;while(e<events.size()&&events[e]!=event)++e;
            Check(e<events.size(),"Unknown logic event.");r.When=static_cast<Event>(e);
            if(r.When==Event::Input||r.When==Event::UI)
                Check(bool(in>>r.Argument)&&Token(r.Argument),"Invalid logic event selector.");
            if(r.When==Event::SensorBegin||r.When==Event::SensorEnd||r.When==Event::Timer)
                Check(bool(in>>r.Selector)&&r.Selector>0&&(r.When!=Event::Timer||r.Selector<=36000),"Invalid sensor/timer selector.");
            Check(bool(in>>op),"Missing logic operation.");
            constexpr std::array<std::string_view,6> ops{"print","position","impulse","add","set","sound"};
            unsigned o=0;while(o<ops.size()&&ops[o]!=op)++o;
            Check(o<ops.size(),"Unknown logic operation.");r.Command=static_cast<Op>(o);
            if(r.Command==Op::Print)
            {
                Check(bool(in>>std::quoted(r.Message))&&r.Message.size()<=128,"Invalid logic print.");
                for(char c:r.Message)Check(c>=32&&c<=126,"Invalid print character.");
            }
            else if(r.Command==Op::Position||r.Command==Op::Impulse)
                Check(bool(in>>r.Actor>>r.X>>r.Y)&&r.Actor>0&&std::isfinite(r.X)&&std::isfinite(r.Y)&&
                    std::abs(r.X)<=4096&&std::abs(r.Y)<=4096,"Invalid logic vector.");
            else if(r.Command==Op::Sound)
                Check(bool(in>>r.SelectorSound)&&r.SelectorSound<=2,"Invalid sound event.");
            else
                Check(bool(in>>r.Slot>>r.X)&&r.Slot<Slots&&std::isfinite(r.X)&&std::abs(r.X)<=1000000&&
                    std::trunc(r.X)==r.X,"Invalid logic variable.");
            doc.Rules.push_back(std::move(r));
        }
        Check(!(in>>tag),"Trailing logic data.");return doc;
    }
    struct Signal {Event Kind;std::string_view Argument{};unsigned Selector=0;};
    inline bool Matches(const Rule& r,std::string_view level,Signal s,std::uint64_t tick)
    {
        if(r.Level!=level||r.When!=s.Kind)return false;
        if(s.Kind==Event::Input||s.Kind==Event::UI)return r.Argument==s.Argument;
        if(s.Kind==Event::SensorBegin||s.Kind==Event::SensorEnd)return r.Selector==s.Selector;
        if(s.Kind==Event::Timer)return tick!=0&&tick%r.Selector==0;
        return true;
    }
    class State
    {
        std::array<int,Slots> m_Values{};
    public:
        int Value(unsigned slot)const{return m_Values.at(slot);}
        void Clear(){m_Values={};}
        void Apply(const Rule& r)
        {
            auto& v=m_Values.at(r.Slot);
            if(r.Command==Op::Set)v=static_cast<int>(r.X);
            else if(r.Command==Op::Add)
            {
                const auto n=static_cast<long long>(v)+static_cast<long long>(r.X);
                v=static_cast<int>(n<-1000000?-1000000:n>1000000?1000000:n);
            }
            else throw std::logic_error("Not a variable operation.");
        }
    };
}
