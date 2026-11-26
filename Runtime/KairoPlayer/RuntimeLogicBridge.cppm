module;

#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>
#include "LogicDebugGate.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <span>
#include <variant>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module Kairo.Player.RuntimeLogicBridge;

import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.PhysicsEngine.World;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeProject;

export namespace kairo::player
{
    // Opt-in capture and debugger. Pause blocks the Player thread before
    // an instruction, preserving the VM stack and host side effects.
    class RuntimeLogicTrace final:public kairo::engine::LogicTraceObserver
    {
    public:
        static constexpr std::size_t MaximumRows=4096;
        RuntimeLogicTrace(const std::filesystem::path& root,kairo::assets::AssetID session)
        {
            const auto base=root/".kairo";
            const auto logs=base/"logs";
            const auto path=logs/("logic-"+session.ToString()+".ktrace");
            if(std::filesystem::is_symlink(base)||std::filesystem::is_symlink(logs)||
                std::filesystem::is_symlink(path)||std::filesystem::exists(path))
                throw std::invalid_argument("Logic trace path is linked or already exists.");
            std::filesystem::create_directories(logs);
            m_Out.open(path,std::ios::binary|std::ios::out|std::ios::trunc);
            if(!m_Out)throw std::runtime_error("Cannot create logic trace.");
            m_Out.imbue(std::locale::classic());
            m_Out<<std::setprecision(std::numeric_limits<double>::max_digits10);
            m_Out<<"kairo.logic.trace.v1 "<<session.ToString()<<'\n';
            m_Out.flush();
            m_Base=logs/("logic-"+session.ToString());
            m_Session=session.ToString();
            const auto config=ReadSmall(m_Base.string()+".debug",256);
            if(config)
            {
                std::istringstream in(*config);
                std::string mode,extra;
                if(!(in>>mode))throw std::invalid_argument("Empty debug configuration.");
                if(mode=="first")m_Gate.emplace(true);
                else if(mode=="break")
                {
                    if(!(in>>m_BreakDocument>>m_BreakRevision>>m_BreakPC)||
                        m_BreakDocument.size()!=36||m_BreakRevision.size()!=64)
                        throw std::invalid_argument("Invalid breakpoint configuration.");
                    m_Gate.emplace(false);
                }
                else throw std::invalid_argument("Unknown debug configuration.");
                if(in>>extra)throw std::invalid_argument("Trailing debug configuration.");
            }
        }
        [[nodiscard]] bool Active() const noexcept
        {return (m_Rows<MaximumRows&&!m_Failed)||(m_Gate&&m_Gate->Attached());}
        void SetDocument(std::string_view id,std::string_view revision) noexcept
        {
            m_Document=id;m_Revision=revision;
        }
        void BeforeInstruction(kairo::engine::Entity owner,const kairo::engine::LogicDispatch& dispatch,
            std::uint32_t pc,const kairo::engine::LogicInstruction& instruction,
            std::span<const kairo::engine::LogicRegisterValue> registers) noexcept override
        {
            if(!Active())return;
            try
            {
                std::optional<detail::LogicBreakpoint> breakpoint;
                if(!m_BreakDocument.empty())breakpoint=detail::LogicBreakpoint{m_BreakDocument,m_BreakRevision,m_BreakPC};
                const bool attached=m_Gate&&m_Gate->Attached();
                const bool pause=attached&&m_Gate->Before(m_Document,m_Revision,pc,breakpoint);
                if(attached&&!m_Gate->Attached())PublishPause("detached");
                const bool capture=m_Rows<MaximumRows&&!m_Failed;
                if(!pause&&!capture)return;
                std::ostringstream row;
                row.imbue(std::locale::classic());
                row<<std::setprecision(std::numeric_limits<double>::max_digits10);
                row<<"step "<<0<<' '<<owner.Value<<' '<<m_Document<<' '<<m_Revision
                    <<' '<<EventName(dispatch.Event)<<' '<<pc<<' '<<OpcodeName(instruction.Opcode)
                    <<' '<<instruction.A<<' '<<instruction.B<<' '<<instruction.C<<' '<<instruction.D<<" |";
                // Reserved dispatch registers plus actual register operands.
                // Never dump the whole VM register file at every instruction.
                std::array<std::uint32_t,6> watched{0,1,2,0,0,0};
                std::size_t count=3;
                using enum kairo::engine::LogicOpcode;
                switch(instruction.Opcode)
                {
                    case JumpIfFalse:case LoadBoolean:case LoadFloat:case LoadVector3:case LoadEntity:
                        watched[count++]=instruction.A;break;
                    case AddFloat:watched[count++]=instruction.A;watched[count++]=instruction.B;
                        watched[count++]=instruction.C;break;
                    case SetEntityPosition:case ApplyEntityImpulse:
                        watched[count++]=instruction.A;watched[count++]=instruction.B;break;
                    default:break;
                }
                for(std::size_t i=0;i<count;++i)
                {
                    if(watched[i]>=registers.size())continue;
                    bool repeated=false;
                    for(std::size_t j=0;j<i;++j)repeated|=watched[j]==watched[i];
                    if(repeated)continue;
                    row<<" r"<<watched[i]<<'=';
                    const auto& value=registers[watched[i]];
                    if(const auto* v=std::get_if<bool>(&value))row<<(*v?"true":"false");
                    else if(const auto* v=std::get_if<double>(&value))row<<*v;
                    else if(const auto* v=std::get_if<kairo::foundation::math::Vec3d>(&value))
                        row<<'('<<v->x<<','<<v->y<<','<<v->z<<')';
                    else if(const auto* v=std::get_if<kairo::engine::Entity>(&value))row<<"entity:"<<v->Value;
                    else row<<"unset";
                }
                row<<'\n';
                const auto record=row.str();
                if(capture)
                {
                    m_Out<<"step "<<m_Rows<<record.substr(6);
                    ++m_Rows;m_Dirty=true;
                    if(m_Rows==MaximumRows)m_Out<<"limit 4096 instruction records reached\n";
                    if(!m_Out)m_Failed=true;
                }
                if(pause){Flush();WaitForCommand(record);}
            }
            catch(...)
            {
                m_Failed=true;
                if(m_Gate&&m_Gate->Attached())
                {
                    m_Gate->Detach();
                    try{if(m_Gate->Stop())PublishPause("detached");}catch(...){}
                }
            }
        }
        void RecordFailure(std::string_view message) noexcept
        {
            if(m_Failed||m_ReportedFailure)return;
            m_ReportedFailure=true;
            try
            {
                m_Out<<"error ";
                for(unsigned char c:message.substr(0,512))m_Out<<char(c<32||c==127?' ':c);
                m_Out<<'\n';m_Dirty=true;Flush();
            }
            catch(...){m_Failed=true;}
        }
        void Flush() noexcept
        {
            if(!m_Dirty||m_Failed)return;
            try{m_Out.flush();m_Dirty=false;if(!m_Out)m_Failed=true;}
            catch(...){m_Failed=true;}
        }
    private:
        std::filesystem::path m_Base;
        std::string m_Session,m_BreakDocument,m_BreakRevision;
        std::uint32_t m_BreakPC=0;
        std::optional<detail::LogicDebugGate> m_Gate;
        static std::optional<std::string> ReadSmall(const std::filesystem::path& path,std::size_t limit)
        {
            if(std::filesystem::is_symlink(path))throw std::invalid_argument("Linked debug file rejected.");
            if(!std::filesystem::exists(path))return std::nullopt;
            if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)>limit)
                throw std::invalid_argument("Invalid debug file.");
            std::ifstream in(path,std::ios::binary);
            std::string bytes(limit+1,'\0');
            if(!in)throw std::runtime_error("Cannot read debug file.");
            in.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
            if(in.bad()||in.gcount()>static_cast<std::streamsize>(limit))
                throw std::runtime_error("Debug read exceeded budget.");
            bytes.resize(static_cast<std::size_t>(in.gcount()));
            return bytes;
        }
        void PublishPause(std::string_view status,std::string_view record={})
        {
            const auto path=std::filesystem::path(m_Base.string()+".pause");
            if(std::filesystem::is_symlink(path))throw std::invalid_argument("Linked pause file rejected.");
            kairo::engine::SaveTextFileAtomically(path,
                "kairo.logic.pause.v1 "+m_Session+"\n"+std::string(status)+" "+
                std::to_string(m_Gate->Stop())+"\n"+std::string(record),"logic debug state");
        }
        void WaitForCommand(const std::string& record)
        {
            PublishPause("paused",record);
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(120);
            const auto stop=m_Gate->Stop();
            const auto commandPath=std::filesystem::path(m_Base.string()+".cmd-"+std::to_string(stop));
            while(std::chrono::steady_clock::now()<deadline)
            {
                if(const auto bytes=ReadSmall(commandPath,64))
                    if(const auto action=detail::ParseDebugCommand(*bytes,stop))
                        if(m_Gate->Apply(stop,*action))
                        {
                            PublishPause(m_Gate->Attached()?"running":"detached");
                            return;
                        }
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
            m_Gate->Detach();
            PublishPause("timeout");
        }
        std::ofstream m_Out;
        std::size_t m_Rows=0;
        bool m_Dirty=false,m_Failed=false,m_ReportedFailure=false;
        std::string_view m_Document,m_Revision;
        static const char* EventName(kairo::engine::LogicEventKind value) noexcept
        {
            using enum kairo::engine::LogicEventKind;
            switch(value)
            {
                case BeginPlay:return "BeginPlay";case Tick:return "Tick";
                case InputPressed:return "InputPressed";case InputReleased:return "InputReleased";
                case CollisionBegin:return "CollisionBegin";case CollisionEnd:return "CollisionEnd";
            }
            return "Unknown";
        }
        static const char* OpcodeName(kairo::engine::LogicOpcode value) noexcept
        {
            using enum kairo::engine::LogicOpcode;
            switch(value)
            {
                case Halt:return "Halt";case Print:return "Print";case Jump:return "Jump";
                case JumpIfFalse:return "JumpIfFalse";case LoadBoolean:return "LoadBoolean";
                case LoadFloat:return "LoadFloat";case LoadVector3:return "LoadVector3";
                case LoadEntity:return "LoadEntity";case AddFloat:return "AddFloat";
                case SetEntityPosition:return "SetEntityPosition";case ApplyEntityImpulse:return "ApplyEntityImpulse";
            }
            return "Unknown";
        }
    };

    /// Runtime owner of scene-attached visual logic. It accepts only artifacts
    /// produced for the current source bytes and routes every side effect
    /// through scene/physics host methods; editor graph types never enter the
    /// standalone player.
    class RuntimeLogicBridge final : public RuntimeFixedStepListener,
        private kairo::engine::LogicHost
    {
    public:
        RuntimeLogicBridge(RuntimeProject& project, RuntimePhysicsBridge& physics,
            std::optional<kairo::assets::AssetID> traceSession=std::nullopt)
            : m_Physics(physics)
        {
            if(traceSession)m_Trace.emplace(project.Root(),*traceSession);
            for (const kairo::engine::Entity entity : project.Scene().Entities())
            {
                if (!project.Scene().IsActiveInHierarchy(entity) || !project.Scene().HasLogic(entity) ||
                    !project.Scene().Logic(entity).Enabled) continue;
                const auto document = project.Scene().Logic(entity).Document;
                const auto metadata = project.Assets().Resolve(document);
                const auto sourcePath = project.RequireProjectFile(metadata.Path, "logic document");
                const auto artifactPath = kairo::engine::CompiledLogicPath(project.Root(), document.ID);
                kairo::engine::CompiledLogicArtifact artifact =
                    kairo::engine::LoadCompiledLogicArtifact(artifactPath);
                if (artifact.Source != document.ID)
                    throw std::invalid_argument("Compiled logic artifact belongs to another source: " +
                        artifactPath.string());
                if (artifact.SourceFingerprint != kairo::assets::FingerprintFile(sourcePath))
                    throw std::invalid_argument("Compiled logic artifact is stale; rebuild project logic: " +
                        metadata.Path.generic_string());
                for (const auto& entry : artifact.Program.Entries)
                    if ((entry.Event == kairo::engine::LogicEventKind::InputPressed ||
                        entry.Event == kairo::engine::LogicEventKind::InputReleased) &&
                        project.InputMap().FindAction(entry.Action) == nullptr)
                        throw std::invalid_argument("Compiled logic references an unknown input action: " + entry.Action);
                m_Instances.emplace_back(entity, std::move(artifact.Program),
                    document.ID.ToString(),artifact.SourceFingerprint.ToHex());
            }
        }

        [[nodiscard]] std::size_t InstanceCount() const noexcept { return m_Instances.size(); }

        void BeginPlay()
        {
            DispatchAll({ .Event = kairo::engine::LogicEventKind::BeginPlay,
                .Action = {}, .DeltaSeconds = 0.0, .ActionValue = 0.0, .OtherEntity = {} });
        }

        void DispatchInput(std::string_view action, const kairo::engine::InputActionState& state)
        {
            const double value = static_cast<double>(state.Value.X);
            if (state.Pressed) DispatchAll({ .Event = kairo::engine::LogicEventKind::InputPressed,
                .Action = action, .DeltaSeconds = 0.0, .ActionValue = value, .OtherEntity = {} });
            if (state.Released) DispatchAll({ .Event = kairo::engine::LogicEventKind::InputReleased,
                .Action = action, .DeltaSeconds = 0.0, .ActionValue = value, .OtherEntity = {} });
        }

        void BeforePhysicsStep(float fixedDeltaSeconds) override
        {
            DispatchAll({ .Event = kairo::engine::LogicEventKind::Tick,
                .Action = {}, .DeltaSeconds = fixedDeltaSeconds, .ActionValue = 0.0, .OtherEntity = {} });
        }

        void DispatchContacts()
        {
            for (const RuntimeContactEvent& contact : m_Physics.ContactEvents())
            {
                if (contact.Type == kairo::foundation::physics::PhysicsContactEventType::Stay) continue;
                const auto event = contact.Type == kairo::foundation::physics::PhysicsContactEventType::Begin
                    ? kairo::engine::LogicEventKind::CollisionBegin
                    : kairo::engine::LogicEventKind::CollisionEnd;
                DispatchOne(contact.EntityA, { .Event = event, .Action = {}, .DeltaSeconds = 0.0,
                    .ActionValue = 0.0, .OtherEntity = contact.EntityB });
                DispatchOne(contact.EntityB, { .Event = event, .Action = {}, .DeltaSeconds = 0.0,
                    .ActionValue = 0.0, .OtherEntity = contact.EntityA });
            }
        }

    private:
        struct InstanceRecord final
        {
            kairo::engine::Entity Owner;
            kairo::engine::LogicInstance Instance;
            std::string Document,Revision;
            InstanceRecord(kairo::engine::Entity owner, kairo::engine::LogicProgram program,
                std::string document,std::string revision)
                : Owner(owner), Instance(std::move(program)),Document(std::move(document)),Revision(std::move(revision)) {}
        };

        RuntimePhysicsBridge& m_Physics;
        std::vector<InstanceRecord> m_Instances;
        std::optional<RuntimeLogicTrace> m_Trace;

        void DispatchRecord(InstanceRecord& record,const kairo::engine::LogicDispatch& dispatch)
        {
            auto* trace=m_Trace&&m_Trace->Active()?&*m_Trace:nullptr;
            if(trace)trace->SetDocument(record.Document,record.Revision);
            try
            {
                (void)record.Instance.Dispatch(record.Owner,dispatch,*this,
                    kairo::engine::LogicInstance::DefaultInstructionBudget,trace);
            }
            catch(const std::exception& e)
            {
                if(m_Trace)m_Trace->RecordFailure(e.what());
                throw;
            }
            if(trace)trace->Flush();
        }

        void DispatchAll(const kairo::engine::LogicDispatch& dispatch)
        {
            for (auto& record : m_Instances)
                DispatchRecord(record,dispatch);
        }

        void DispatchOne(kairo::engine::Entity owner, const kairo::engine::LogicDispatch& dispatch)
        {
            for (auto& record : m_Instances)
                if (record.Owner == owner)
                    DispatchRecord(record,dispatch);
        }

        void Print(kairo::engine::Entity owner, std::string_view message) override
        {
            std::cout << "[logic entity=" << owner.Value << "] " << message << '\n';
        }

        void SetEntityPosition(kairo::engine::Entity entity,
            const kairo::foundation::math::Vec3d& position) override
        {
            m_Physics.SetEntityPosition(entity, position);
        }

        void ApplyEntityImpulse(kairo::engine::Entity entity,
            const kairo::foundation::math::Vec3d& impulse) override
        {
            m_Physics.ApplyEntityImpulse(entity, impulse);
        }
    };
}
