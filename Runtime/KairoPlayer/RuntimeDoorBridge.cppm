module;
#include "RuntimeSlidingDoor.hpp"
#include "CraftSaveFile.hpp"
#include <cmath>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <vector>

export module Kairo.Player.RuntimeDoorBridge;

import Kairo.EngineCore;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeProject;
import Kairo.Foundation.PhysicsEngine;

export namespace kairo::player
{
    // Door bindings borrow the existing authoritative physics world. Each
    // door is a kinematic solid; no body removal or render-only teleport.
    class RuntimeDoorBridge final
    {
        struct Entry {kairo::engine::Entity Target;detail::RuntimeSlidingDoor Motion;};
        RuntimePhysicsBridge& m_Physics;
        std::optional<kairo::engine::Entity> m_Player;
        std::vector<Entry> m_Doors;
    public:
        RuntimeDoorBridge(RuntimeProject& project,RuntimePhysicsBridge& physics,
            std::optional<kairo::engine::Entity> primary)
            :m_Physics(physics),m_Player(primary)
        {
            if(!std::filesystem::exists(project.Root()/"Config/Doors.kdoor"))return;
            const auto path=project.RequireProjectFile("Config/Doors.kdoor","door document");
            const auto bytes=detail::ReadCraftSaveFile(path,detail::MaximumTriggerBytes);
            const auto rules=detail::ParseDoorRules(std::string_view(reinterpret_cast<const char*>(bytes.data()),bytes.size()));
            if(rules.empty())return;
            const auto& scene=project.Scene();
            if(!primary||!physics.BodyFor(*primary)||!scene.HasCollider(*primary)||
                scene.Collider(*primary).Shape!=kairo::engine::ColliderShape::Capsule)
                throw std::invalid_argument("Doors require a primary capsule player with a physics body.");
            const auto playerScale=scene.WorldTransform(*primary).Scale;
            const auto& capsule=scene.Collider(*primary);
            const double playerGuard=(capsule.Radius+capsule.HalfHeight)*std::max({
                std::abs(playerScale.x),std::abs(playerScale.y),std::abs(playerScale.z)});
            for(const auto& rule:rules)
            {
                const kairo::engine::Entity entity{rule.Target};
                if(entity==*primary||!scene.Contains(entity)||!scene.IsActiveInHierarchy(entity)||
                    !scene.HasTag(entity,"kairo.sliding-door")||scene.Parent(entity)||!scene.Children(entity).empty()||
                    !scene.HasRigidBody(entity)||scene.RigidBody(entity).Motion!=kairo::engine::RigidBodyMotion::Kinematic||
                    !scene.HasCollider(entity)||scene.Collider(entity).Shape!=kairo::engine::ColliderShape::Box||
                    scene.Collider(entity).IsTrigger||!physics.BodyFor(entity)||!scene.HasMeshRenderer(entity)||
                    scene.HasLogic(entity)||scene.HasTag(entity,"kairo.collectible")||scene.HasTag(entity,"kairo.trigger-visual")||
                    scene.HasCamera(entity)||scene.HasLight(entity)||scene.HasEnvironment(entity)||
                    scene.HasAudioEmitter(entity)||scene.HasAudioListener(entity))
                    throw std::invalid_argument("Door requires a tagged, active kinematic solid box mesh root.");
                const auto world=scene.WorldTransform(entity);
                const auto half=scene.Collider(entity).HalfExtents;
                const double guard=std::hypot(double(half.x)*world.Scale.x,double(half.y)*world.Scale.y,
                    double(half.z)*world.Scale.z)+playerGuard+.25;
                const auto p=world.Translation;
                m_Doors.push_back({entity,detail::RuntimeSlidingDoor(rule,{p.x,p.y,p.z},guard)});
            }
        }
        [[nodiscard]] std::size_t Size() const noexcept{return m_Doors.size();}
        void BeforePhysicsStep(float dt)
        {
            if(m_Doors.empty())return;
            const auto player=m_Physics.BodyFor(*m_Player);
            if(!player)return;
            const auto p=m_Physics.World().Bodies().at(*player).State.Position;
            for(auto& door:m_Doors)
            {
                const auto body=m_Physics.BodyFor(door.Target);
                if(!body)continue;
                const auto before=door.Motion.Offset();
                const auto pose=door.Motion.Step({p.x,p.y,p.z},dt);
                if(before!=door.Motion.Offset())
                    m_Physics.SetEntityPosition(door.Target,{pose[0],pose[1],pose[2]});
            }
        }
    };
}
