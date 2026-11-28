module;
#include "RuntimeHealth.hpp"
#include "CraftSaveFile.hpp"
#include <filesystem>
#include <optional>
#include <iostream>

export module Kairo.Player.RuntimeHealthBridge;

import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeCharacterMotorBridge;

export namespace kairo::player
{
    // Opt-in health adapter. The motor owns teleport and vertical velocity;
    // health never writes a renderer transform or physics pose independently.
    class RuntimeHealthBridge final
    {
        RuntimePhysicsBridge& m_Physics;
        RuntimeCharacterMotorBridge& m_Motor;
        std::optional<kairo::engine::Entity> m_Player;
        std::optional<detail::RuntimeHealth> m_Health;
    public:
        RuntimeHealthBridge(RuntimeProject& project,RuntimePhysicsBridge& physics,
            RuntimeCharacterMotorBridge& motor,std::optional<kairo::engine::Entity> primary)
            :m_Physics(physics),m_Motor(motor),m_Player(primary)
        {
            if(!std::filesystem::exists(project.Root()/"Config/Health.khealth"))return;
            if(!primary||!motor.IsRegistered(*primary)||!physics.BodyFor(*primary))
                throw std::invalid_argument("Health requires a registered primary character.");
            const auto path=project.RequireProjectFile("Config/Health.khealth","health document");
            const auto bytes=detail::ReadCraftSaveFile(path,detail::MaximumHealthBytes);
            const auto p=project.Scene().WorldTransform(*primary).Translation;
            m_Health.emplace(std::string_view(reinterpret_cast<const char*>(bytes.data()),bytes.size()),
                detail::HealthPosition{p.x,p.y,p.z});
        }
        [[nodiscard]] bool Enabled() const noexcept{return m_Health.has_value();}
        [[nodiscard]] unsigned Health() const noexcept{return m_Health?m_Health->Health():0;}
        [[nodiscard]] unsigned Maximum() const noexcept{return m_Health?m_Health->Maximum():0;}
        [[nodiscard]] std::uint32_t Checkpoint() const noexcept{return m_Health?m_Health->Checkpoint():0;}
        // Call after controller movement, once per physics substep, so
        // damage and cooldown do not depend on render frame rate.
        bool AfterCharacterStep(unsigned incomingDamage=0)
        {
            if(!m_Health)return false;
            const auto body=m_Physics.BodyFor(*m_Player);
            if(!body)return false;
            const auto p=m_Physics.World().Bodies().at(*body).State.Position;
            const auto event=m_Health->Step({p.x,p.y,p.z},incomingDamage);
            if(event.Checkpoint)std::cout<<"Checkpoint "<<event.Checkpoint<<" activated.\n";
            if(event.Damage)std::cout<<"Damage "<<event.Damage<<"; health "<<Health()<<"/"<<Maximum()<<'\n';
            if(event.Respawn)
            {
                m_Motor.Teleport(*m_Player,{static_cast<float>(event.Position[0]),
                    static_cast<float>(event.Position[1]),static_cast<float>(event.Position[2])});
                std::cout<<"Respawned at checkpoint "<<Checkpoint()<<'\n';
            }
            return event.Respawn;
        }
    };
}
