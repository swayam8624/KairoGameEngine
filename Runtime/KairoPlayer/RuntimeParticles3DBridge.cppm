module;
#include "RuntimeParticles3D.hpp"
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>

export module Kairo.Player.RuntimeParticles3DBridge;

import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Player.RuntimeProject;

export namespace kairo::player
{
    // Opt-in visual-only scene pool: ordinary mesh/material bindings are
    // extracted by RuntimeRenderBridge, just like other authored geometry.
    // This adapter owns transform/visibility, not GPU resources.
    class RuntimeParticles3DBridge final
    {
        struct Marker
        {
            kairo::engine::Entity ID;
            detail::ParticlePoint Scale;
        };
        RuntimeProject& project_;
        std::optional<kairo::engine::Entity> emitter_;
        std::vector<Marker> markers_;
        std::optional<detail::RuntimeParticles3D> system_;
    public:
        explicit RuntimeParticles3DBridge(RuntimeProject& project):project_(project)
        {
            const auto& scene=project.Scene();
            for(const auto entity:scene.Entities())
            {
                const bool emitter=scene.HasTag(entity,"kairo.particle-emitter");
                const bool marker=scene.HasTag(entity,"kairo.particle-marker");
                if(!emitter&&!marker)continue;
                if(emitter&&marker)throw std::invalid_argument("Particle emitter and marker must be distinct.");
                if(emitter)
                {
                    if(emitter_)throw std::invalid_argument("Only one 3D particle emitter per scene is supported.");
                    emitter_=entity;
                    continue;
                }
                if(scene.Parent(entity)||!scene.Children(entity).empty()||scene.IsActiveInHierarchy(entity)||
                    !scene.HasMeshRenderer(entity)||scene.HasSceneInstance(entity)||
                    scene.HasRigidBody(entity)||scene.HasCollider(entity)||scene.HasLogic(entity)||
                    scene.HasCamera(entity)||scene.HasLight(entity)||scene.HasEnvironment(entity)||
                    scene.HasAudioEmitter(entity)||scene.HasAudioListener(entity)||
                    scene.HasTag(entity,"kairo.projectile-marker")||scene.HasTag(entity,"kairo.combat-animated")||
                    scene.HasTag(entity,"kairo.collectible")||scene.HasTag(entity,"kairo.damageable")||
                    scene.HasTag(entity,"kairo.event-visual")||scene.HasTag(entity,"kairo.trigger-visual")||
                    scene.HasTag(entity,"kairo.sliding-door")||scene.HasTag(entity,"kairo.enemy")||
                    scene.HasTag(entity,"kairo.player-controller")||markers_.size()>=128)
                    throw std::invalid_argument("Particle markers must be dedicated disabled mesh roots (maximum 128).");
                const auto scale=scene.WorldTransform(entity).Scale;
                for(float x:{scale.x,scale.y,scale.z})
                    if(!std::isfinite(x)||x<=0||x>1000)
                        throw std::invalid_argument("Invalid particle marker scale.");
                markers_.push_back({entity,{scale.x,scale.y,scale.z}});
            }
            if(bool(emitter_)!=!markers_.empty())
                throw std::invalid_argument("3D particles require an emitter and a marker pool together.");
            std::sort(markers_.begin(),markers_.end(),[](const Marker& a,const Marker& b){return a.ID.Value<b.ID.Value;});
            if(emitter_)
            {
                const auto p=scene.WorldTransform(*emitter_).Translation;
                system_.emplace(markers_.size());
                system_->Advance(0,detail::ParticlePoint{p.x,p.y,p.z});
            }
        }
        [[nodiscard]] std::size_t Capacity() const noexcept{return markers_.size();}
        void Advance(double dt)
        {
            if(!system_)return;
            auto& scene=project_.Scene();
            std::optional<detail::ParticlePoint> origin;
            if(scene.Contains(*emitter_)&&scene.IsActiveInHierarchy(*emitter_))
            {
                const auto p=scene.WorldTransform(*emitter_).Translation;
                origin=detail::ParticlePoint{p.x,p.y,p.z};
            }
            system_->Advance(dt,origin);
            const auto& slots=system_->Slots();
            for(std::size_t i=0;i<markers_.size();++i)
            {
                const auto& marker=markers_[i];
                if(!scene.Contains(marker.ID))continue;
                const auto& p=slots[i];
                if(p.Active)
                {
                    auto& transform=scene.Transform(marker.ID).Local;
                    transform.Translation={p.Position[0],p.Position[1],p.Position[2]};
                    const auto scale=p.Scale();
                    transform.Scale={marker.Scale[0]*scale,marker.Scale[1]*scale,marker.Scale[2]*scale};
                    scene.SetEnabled(marker.ID,true);
                }
                else scene.SetEnabled(marker.ID,false);
            }
        }
    };
}
