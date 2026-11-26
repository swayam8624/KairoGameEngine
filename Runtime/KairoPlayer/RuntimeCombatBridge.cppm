module;
#include "RuntimeCombat.hpp"
#include "RuntimeProjectileFlight.hpp"
#include "CraftSaveFile.hpp"
#include <array>
#include <filesystem>
#include <iostream>
#include <optional>

export module Kairo.Player.RuntimeCombatBridge;

import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimePhysicsBridge;

export namespace kairo::player
{
    // One attack edge per fixed tick; retain the last nonzero movement
    // direction as aim. Physics owns hit ordering and occlusion.
    class RuntimeCombatBridge final
    {
        RuntimeProject& m_Project;
        RuntimePhysicsBridge& m_Physics;
        std::optional<kairo::engine::Entity> m_Player;
        std::optional<detail::RuntimeCombat> m_Combat;
        detail::RuntimeProjectileFlight m_Flights;
        kairo::foundation::math::Vec3f m_Aim{0,0,-1};
    public:
        RuntimeCombatBridge(RuntimeProject& project,RuntimePhysicsBridge& physics,
            std::optional<kairo::engine::Entity> primary)
            :m_Project(project),m_Physics(physics),m_Player(primary)
        {
            if(!std::filesystem::exists(project.Root()/"Config/Combat.kcombat"))return;
            if(!primary||!physics.BodyFor(*primary)||!project.InputMap().FindAction("Attack"))
                throw std::invalid_argument("Combat requires a primary body and Attack action.");
            const auto path=project.RequireProjectFile("Config/Combat.kcombat","combat document");
            const auto bytes=detail::ReadCraftSaveFile(path,detail::MaximumCombatBytes);
            m_Combat.emplace(std::string_view(reinterpret_cast<const char*>(bytes.data()),bytes.size()));
            const auto& scene=project.Scene();
            for(const auto& target:m_Combat->Targets())
            {
                const kairo::engine::Entity e{target.ID};
                if(e==*primary||!scene.Contains(e)||!scene.IsActiveInHierarchy(e)||
                    !scene.HasTag(e,"kairo.damageable")||scene.Parent(e)||!scene.Children(e).empty()||
                    !scene.HasMeshRenderer(e)||!scene.HasCollider(e)||scene.Collider(e).IsTrigger||
                    (scene.HasRigidBody(e)&&scene.RigidBody(e).Motion!=kairo::engine::RigidBodyMotion::Static)||
                    !physics.BodyFor(e)||scene.HasLogic(e)||scene.HasCamera(e)||scene.HasLight(e)||
                    scene.HasEnvironment(e)||scene.HasAudioEmitter(e)||scene.HasAudioListener(e)||
                    scene.HasTag(e,"kairo.collectible")||scene.HasTag(e,"kairo.trigger-visual")||
                    scene.HasTag(e,"kairo.sliding-door")||scene.HasTag(e,"kairo.player-controller"))
                    throw std::invalid_argument("Combat targets must be opted-in static solid visual roots.");
            }
        }
        [[nodiscard]] bool Enabled() const noexcept{return m_Combat.has_value();}
        void Capture(bool pressed,const kairo::foundation::math::Vec3f& planar)
        {
            if(!m_Combat)return;
            if(!std::isfinite(planar.x)||!std::isfinite(planar.z))
                throw std::invalid_argument("Invalid attack direction.");
            if(std::hypot(planar.x,planar.z)>1e-4f)m_Aim={planar.x,0,planar.z};
            m_Combat->CapturePress(pressed);
        }
        void ClearInput() noexcept{if(m_Combat)m_Combat->ClearInput();}
        void CancelFlights() noexcept{m_Flights.Clear();}
        [[nodiscard]] std::size_t ActiveFlights() const noexcept{return m_Flights.Size();}
        void Step(float dt=1.f/60)
        {
            if(!m_Combat)return;
            const auto body=m_Physics.BodyFor(*m_Player);
            if(!body){ClearInput();CancelFlights();return;}
            const auto ray=[&](const detail::FlightVector& p,const detail::FlightVector& d,double range)->std::optional<std::uint32_t>{
                const auto& world=m_Physics.World();
                auto own=kairo::foundation::physics::InvalidColliderID;
                for(const auto& c:world.Colliders())if(c.Active&&c.Body==*body)
                {
                    if(own!=kairo::foundation::physics::InvalidColliderID)
                        throw std::logic_error("Combat player has multiple colliders.");
                    own=c.ID;
                }
                if(own==kairo::foundation::physics::InvalidColliderID)
                    throw std::logic_error("Combat player has no collider.");
                const kairo::foundation::math::Vec3f origin{float(p[0]),float(p[1]),float(p[2])};
                const kairo::foundation::math::Vec3f direction{float(d[0]),float(d[1]),float(d[2])};
                for(const auto& hit:world.RaycastAll(origin,direction,static_cast<float>(range),0xFFFFFFFFu,own))
                {
                    const auto entity=m_Physics.EntityFor(hit.Body);
                    if(!entity)return 0u;
                    if(m_Project.Scene().Collider(*entity).IsTrigger)continue;
                    return entity->Value;
                }
                return {};
            };
            // Resolve existing flights first. Destroyed bodies are removed
            // before another projectile performs its query in the same tick.
            m_Flights.Step(dt,ray,[&](std::uint32_t id){Publish(m_Combat->ApplyHit(id));});
            if(!m_Combat->BeginAttack())return;
            const auto origin=m_Physics.World().Bodies().at(*body).State.Position;
            const detail::FlightVector p{origin.x,origin.y,origin.z},d{m_Aim.x,0,m_Aim.z};
            if(m_Combat->ProjectileSpeed())
            {
                if(m_Flights.Launch(p,d,m_Combat->Range(),m_Combat->ProjectileSpeed()))
                    std::cout<<"Projectile launched.\n";
                else std::cout<<"Projectile budget exhausted.\n";
            }
            else Publish(m_Combat->ApplyHit(ray(p,d,m_Combat->Range()).value_or(0)));
        }
    private:
        void Publish(const detail::AttackResult& event)
        {
            if(!event.Damage){std::cout<<"Attack blocked or missed.\n";return;}
            std::cout<<"Hit "<<event.Target<<" damage "<<event.Damage<<'\n';
            if(event.Destroyed)
            {
                const kairo::engine::Entity target{event.Target};
                const std::array entities{target};
                (void)m_Physics.DeactivateEntities(entities);
                m_Project.Scene().SetEnabled(target,false);
                std::cout<<"Destroyed target "<<event.Target<<'\n';
            }
        }
    };
}
