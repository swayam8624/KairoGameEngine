#pragma once
// Include after Kairo.EngineCore.LogicBytecode and Entity imports.
#include "World2DCampaignProject.hpp"
#include <functional>
#include <iostream>
#include <memory>

namespace kairo::player::world2d
{
    // Translate bounded authored world commands to the SAME validated
    // bytecode/host interface used by the 3D visual logic runtime.
    inline kairo::engine::LogicProgram CompileWorldRule(const logic2d::Rule& rule)
    {
        using namespace kairo::engine;
        using logic2d::Op;
        LogicProgram program;
        program.Entries.push_back({LogicEventKind::BeginPlay,{},0});
        if(rule.Command==Op::Print)
        {
            program.Strings.push_back(rule.Message);
            program.Instructions.push_back({LogicOpcode::Print,0});
        }
        else if(rule.Command==Op::Position||rule.Command==Op::Impulse)
        {
            program.RegisterCount=5;
            program.Entities.push_back(Entity{rule.Actor});
            program.Vectors.push_back({rule.X,rule.Y,0});
            program.Instructions.push_back({LogicOpcode::LoadEntity,3,0});
            program.Instructions.push_back({LogicOpcode::LoadVector3,4,0});
            program.Instructions.push_back({rule.Command==Op::Position?LogicOpcode::SetEntityPosition:LogicOpcode::ApplyEntityImpulse,3,4});
        }
        else throw std::invalid_argument("Rule is a World2D host command, not a shared VM command.");
        program.Instructions.push_back({LogicOpcode::Halt});
        program.Validate();return program;
    }
    class WorldLogic final:private kairo::engine::LogicHost
    {
        using Rule=logic2d::Rule;
        using Op=logic2d::Op;
        using Event=logic2d::Event;
        struct Entry {Rule Source;std::unique_ptr<kairo::engine::LogicInstance> VM;};
        std::vector<Entry> m_Entries;
        logic2d::State m_State;
        Game* m_Game=nullptr;
        std::uint64_t m_Tick=0;
        std::string m_Level;
        std::size_t m_TraceRemaining=128;
        std::function<void(unsigned)> m_Sound;
        bool m_Trace=false;
        void Print(kairo::engine::Entity,std::string_view message) override
        {
            if(m_TraceRemaining){--m_TraceRemaining;std::cout<<"[World2D logic] "<<message<<'\n';}
        }
        void SetEntityPosition(kairo::engine::Entity id,const kairo::foundation::math::Vec3d& v) override
        {
            if(!m_Game||!m_Game->ScriptPosition(static_cast<unsigned>(id.Value),v.x,v.y))
                throw std::runtime_error("World2D script position rejected.");
        }
        void ApplyEntityImpulse(kairo::engine::Entity id,const kairo::foundation::math::Vec3d& v) override
        {
            if(!m_Game||!m_Game->ScriptImpulse(static_cast<unsigned>(id.Value),v.x,v.y))
                throw std::runtime_error("World2D script impulse rejected.");
        }
    public:
        explicit WorldLogic(const logic2d::Document& doc,std::function<void(unsigned)> sound,bool trace=false)
            :m_Sound(std::move(sound)),m_Trace(trace)
        {
            for(const auto& r:doc.Rules)
            {
                Entry entry{r,{}};
                if(r.Command==Op::Print||r.Command==Op::Position||r.Command==Op::Impulse)
                    entry.VM=std::make_unique<kairo::engine::LogicInstance>(CompileWorldRule(r));
                m_Entries.push_back(std::move(entry));
            }
        }
        int Value(unsigned slot=0)const{return m_State.Value(slot);}
        void NewCampaign(){m_State.Clear();m_TraceRemaining=128;}
        void Begin(Game& game,std::string_view level)
        {
            m_Level=level;m_Tick=0;Dispatch(game,{Event::Begin});
        }
        void Dispatch(Game& game,logic2d::Signal signal)
        {
            m_Game=&game;
            struct Reset {Game*& Ptr;~Reset(){Ptr=nullptr;}} reset{m_Game};
            for(std::size_t i=0;i<m_Entries.size();++i)
            {
                auto& e=m_Entries[i];const auto& r=e.Source;
                if(!logic2d::Matches(r,m_Level,signal,m_Tick))continue;
                if(e.VM)
                {
                    const auto owner=kairo::engine::Entity{r.Actor?r.Actor:1u};
                    (void)e.VM->Dispatch(owner,{.Event=kairo::engine::LogicEventKind::BeginPlay,
                        .Action={},.DeltaSeconds=0,.ActionValue=0,.OtherEntity={}},*this,8);
                }
                else if(r.Command==Op::Sound)m_Sound(r.SelectorSound);
                else m_State.Apply(r);
                if(m_Trace&&m_TraceRemaining)
                {
                    --m_TraceRemaining;
                    std::cout<<"[World2D trace] "<<m_Level<<" tick="<<m_Tick<<" rule="<<i
                        <<" variable0="<<Value()<<'\n';
                }
            }
        }
        void Tick(Game& game)
        {
            ++m_Tick;Dispatch(game,{Event::Tick});Dispatch(game,{Event::Timer});
        }
        void Sensors(Game& game)
        {
            // Coalesce per-sensor begin/end within one physics tick. Bounded
            // by authored actor count; never forward tile implementation IDs.
            std::set<std::pair<unsigned,bool>> seen;
            for(const auto& [id,begin]:game.SensorEvents())
                if(seen.emplace(id,begin).second)
                    Dispatch(game,{begin?Event::SensorBegin:Event::SensorEnd,{},id});
        }
    };
}
