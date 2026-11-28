module;
#include "RuntimeCombatPresentation.hpp"
#include <algorithm>
#include <optional>
#include <vector>
#include <stdexcept>

export module Kairo.Player.RuntimeCombatPresentationBridge;

import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimeCombatBridge;
import Kairo.Player.RuntimeRenderBridge;

export namespace kairo::player
{
    // The authored scene owns reusable visual-only roots. Rendering uses the
    // existing asset/resource bridge; there is no per-shot mesh allocation.
    class RuntimeCombatPresentationBridge final
    {
        RuntimeProject& m_Project;
        RuntimeCombatBridge& m_Combat;
        std::vector<kairo::engine::Entity> m_Markers;
        std::optional<kairo::engine::Entity> m_Animated;
        std::optional<detail::RuntimeCombatPresentation> m_Policy;
        std::optional<std::uint32_t> m_Idle,m_Attack;
        bool m_WasAttacking=false;
    public:
        RuntimeCombatPresentationBridge(RuntimeProject& project,RuntimeCombatBridge& combat,
            std::optional<kairo::engine::Entity> primary)
            :m_Project(project),m_Combat(combat)
        {
            const auto& scene=project.Scene();
            for(const auto e:scene.Entities())
            {
                const bool marker=scene.HasTag(e,"kairo.projectile-marker");
                const bool animated=scene.HasTag(e,"kairo.combat-animated");
                if(!marker&&!animated)continue;
                if(!combat.Enabled()||marker==animated||!scene.Children(e).empty()||
                    scene.HasRigidBody(e)||scene.HasCollider(e)||scene.HasLogic(e)||
                    scene.HasCamera(e)||scene.HasLight(e)||scene.HasEnvironment(e)||
                    scene.HasAudioEmitter(e)||scene.HasAudioListener(e)||
                    scene.HasTag(e,"kairo.event-visual")||scene.HasTag(e,"kairo.trigger-visual")||
                    scene.HasTag(e,"kairo.collectible")||scene.HasTag(e,"kairo.damageable")||
                    scene.HasTag(e,"kairo.sliding-door")||scene.HasTag(e,"kairo.enemy")||
                    scene.HasTag(e,"kairo.player-controller"))
                    throw std::invalid_argument("Combat presentation requires dedicated visual leaves.");
                if(marker)
                {
                    if(scene.Parent(e)||scene.IsActiveInHierarchy(e)||!scene.HasMeshRenderer(e)||
                        scene.HasSceneInstance(e)||m_Markers.size()>=512)
                        throw std::invalid_argument("Projectile markers must be disabled mesh roots, maximum 512.");
                    m_Markers.push_back(e);
                }
                else
                {
                    if(m_Animated||!primary||scene.Parent(e)!=primary||
                        !scene.IsActiveInHierarchy(e)||!scene.HasSceneInstance(e))
                        throw std::invalid_argument("Combat animation requires one active imported visual child of the player.");
                    m_Animated=e;
                }
            }
            std::sort(m_Markers.begin(),m_Markers.end(),[](auto a,auto b){return a.Value<b.Value;});
            if(!m_Markers.empty()||m_Animated)m_Policy.emplace(m_Markers.size());
        }
        [[nodiscard]] std::size_t MarkerCount() const noexcept{return m_Markers.size();}
        // Clip names are checked only once the renderer has imported the asset.
        void BindAnimation(RuntimeRenderBridge& render)
        {
            if(!m_Animated)return;
            m_Idle=render.FindAnimationClip(*m_Animated,"Idle");
            m_Attack=render.FindAnimationClip(*m_Animated,"Attack");
            if(!m_Idle||!m_Attack||m_Idle==m_Attack)
                throw std::invalid_argument("Combat visual requires distinct Idle and Attack clips.");
            render.SetAnimation(*m_Animated,*m_Idle,kairo::engine::AnimationTimeMode::Loop,1.f,true);
        }
        void Step(double dt,RuntimeRenderBridge* render=nullptr)
        {
            if(!m_Policy)return;
            if(m_Animated&&(!render||!m_Idle||!m_Attack))
                throw std::logic_error("Combat animation not bound.");
            const auto frame=m_Policy->Step(m_Combat.LaunchSerial(),m_Combat.ActiveFlights(),dt);
            auto& scene=m_Project.Scene();
            for(std::size_t i=0;i<m_Markers.size();++i)
            {
                const auto e=m_Markers[i];
                if(i<frame.VisibleFlights)
                {
                    scene.Transform(e).Local.Translation=m_Combat.FlightPosition(i);
                    scene.SetEnabled(e,true);
                }
                else scene.SetEnabled(e,false);
            }
            if(m_Animated)
            {
                if(frame.RestartAttack)
                    render->SetAnimation(*m_Animated,*m_Attack,kairo::engine::AnimationTimeMode::Clamp,1.f,true);
                else if(!frame.AttackActive&&m_WasAttacking)
                    render->SetAnimation(*m_Animated,*m_Idle,kairo::engine::AnimationTimeMode::Loop,1.f,true);
                m_WasAttacking=frame.AttackActive;
            }
        }
    };
}
