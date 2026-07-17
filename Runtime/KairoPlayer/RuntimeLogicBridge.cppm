module;

#include <iostream>
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
    /// Runtime owner of scene-attached visual logic. It accepts only artifacts
    /// produced for the current source bytes and routes every side effect
    /// through scene/physics host methods; editor graph types never enter the
    /// standalone player.
    class RuntimeLogicBridge final : public RuntimeFixedStepListener,
        private kairo::engine::LogicHost
    {
    public:
        RuntimeLogicBridge(RuntimeProject& project, RuntimePhysicsBridge& physics)
            : m_Physics(physics)
        {
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
                m_Instances.emplace_back(entity, std::move(artifact.Program));
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
            InstanceRecord(kairo::engine::Entity owner, kairo::engine::LogicProgram program)
                : Owner(owner), Instance(std::move(program)) {}
        };

        RuntimePhysicsBridge& m_Physics;
        std::vector<InstanceRecord> m_Instances;

        void DispatchAll(const kairo::engine::LogicDispatch& dispatch)
        {
            for (auto& record : m_Instances)
                (void)record.Instance.Dispatch(record.Owner, dispatch, *this);
        }

        void DispatchOne(kairo::engine::Entity owner, const kairo::engine::LogicDispatch& dispatch)
        {
            for (auto& record : m_Instances)
                if (record.Owner == owner)
                    (void)record.Instance.Dispatch(record.Owner, dispatch, *this);
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
