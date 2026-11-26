module;
#include "RuntimeEnemy.hpp"
#include "RuntimeHealth.hpp"
#include "CraftSaveFile.hpp"
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

export module Kairo.Player.RuntimeEnemyBridge;
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeCharacterMotorBridge;

export namespace kairo::player
{
    class RuntimeEnemyBridge final
    {
        struct Entry{ kairo::engine::Entity Entity; detail::RuntimeEnemy Brain; double Sight; };
        RuntimeProject& m_Project;
        RuntimePhysicsBridge& m_Physics;
        RuntimeCharacterMotorBridge& m_Motor;
        std::optional<kairo::engine::Entity> m_Player;
        std::vector<Entry> m_Enemies;
    public:
        RuntimeEnemyBridge(RuntimeProject& project,RuntimePhysicsBridge& physics,
            RuntimeCharacterMotorBridge& motor,std::optional<kairo::engine::Entity> primary,bool healthEnabled)
            :m_Project(project),m_Physics(physics),m_Motor(motor),m_Player(primary)
        {
            if(!std::filesystem::exists(project.Root()/"Config/Enemies.kenemy"))return;
            if(!primary||!physics.BodyFor(*primary)||!healthEnabled)
                throw std::invalid_argument("Enemies require a primary character and health rules.");
            const auto path=project.RequireProjectFile("Config/Enemies.kenemy","enemy document");
            const auto bytes=detail::ReadCraftSaveFile(path,65536);
            std::istringstream in{std::string(bytes.begin(),bytes.end())};in.imbue(std::locale::classic());
            std::string line;auto read=[&]{if(!std::getline(in,line))return false;
                if(!line.empty()&&line.back()=='\r')line.pop_back();
                if(line.find('\r')!=std::string::npos)throw std::invalid_argument("Invalid enemy line.");
                return true;};
            if(!read()||line!="kairo-enemies 1")throw std::invalid_argument("Invalid enemy header.");
            std::set<std::uint32_t> ids;
            const auto& scene=project.Scene();
            while(read())
            {
                if(line.empty())continue;
                std::istringstream row(line);row.imbue(std::locale::classic());
                std::string command;row>>command;
                if(command!="enemy"||m_Enemies.size()>=64)throw std::invalid_argument("Invalid enemy statement or budget.");
                const kairo::engine::Entity e{detail::HealthUnsigned(row,UINT32_MAX)};
                detail::EnemySettings settings;
                if(!(row>>settings.Speed>>settings.Sight>>settings.Reach))
                    throw std::invalid_argument("Invalid enemy speeds.");
                settings.Memory=detail::HealthUnsigned(row,3600);
                settings.Cooldown=detail::HealthUnsigned(row,600);
                settings.Damage=detail::HealthUnsigned(row,10000);
                detail::EnemyPosition patrol{};
                if(!(row>>patrol[0]>>patrol[1]>>patrol[2]))throw std::invalid_argument("Invalid patrol point.");
                row>>std::ws;
                if(!row.eof()||!ids.insert(e.Value).second||e==*primary||!scene.Contains(e)||
                    !scene.IsActiveInHierarchy(e)||!scene.HasTag(e,"kairo.enemy")||scene.Parent(e)||
                    !scene.Children(e).empty()||!scene.HasMeshRenderer(e)||!scene.HasCollider(e)||
                    scene.Collider(e).IsTrigger||!scene.HasRigidBody(e)||
                    scene.RigidBody(e).Motion!=kairo::engine::RigidBodyMotion::Kinematic||
                    scene.Collider(e).Shape!=kairo::engine::ColliderShape::Capsule||
                    !physics.BodyFor(e)||motor.IsRegistered(e)||scene.HasLogic(e)||
                    scene.HasCamera(e)||scene.HasLight(e)||scene.HasEnvironment(e)||
                    scene.HasAudioEmitter(e)||scene.HasAudioListener(e)||scene.HasTag(e,"kairo.collectible")||
                    scene.HasTag(e,"kairo.trigger-visual")||scene.HasTag(e,"kairo.sliding-door"))
                    throw std::invalid_argument("Enemy must be an opted-in kinematic capsule mesh root.");
                const auto p=scene.WorldTransform(e).Translation;
                m_Enemies.push_back({e,detail::RuntimeEnemy(settings,{{p.x,p.y,p.z},patrol}),settings.Sight});
            }
            std::sort(m_Enemies.begin(),m_Enemies.end(),[](const auto& a,const auto& b){return a.Entity.Value<b.Entity.Value;});
            for(const auto& enemy:m_Enemies)motor.Register(enemy.Entity);
        }
        [[nodiscard]] std::size_t Size() const noexcept{return m_Enemies.size();}
        // Stable authored ID order. Direct steering uses the existing
        // collision-resolving motor; navmesh routing is a separate pass.
        unsigned Step(float dt)
        {
            if(m_Enemies.empty())return 0;
            const auto playerBody=m_Physics.BodyFor(*m_Player);
            if(!playerBody)return 0;
            const auto p=m_Physics.World().Bodies().at(*playerBody).State.Position;
            unsigned damage=0;
            for(auto& enemy:m_Enemies)
            {
                const auto body=m_Physics.BodyFor(enemy.Entity);
                if(!body){m_Motor.Unregister(enemy.Entity);continue;}
                const auto q=m_Physics.World().Bodies().at(*body).State.Position;
                const auto delta=p-q;const float distance=delta.Length();
                bool visible=false;
                if(distance>1e-5f&&distance<=enemy.Sight)
                {
                    const auto& world=m_Physics.World();
                    auto own=kairo::foundation::physics::InvalidColliderID;
                    for(const auto& c:world.Colliders())if(c.Active&&c.Body==*body){own=c.ID;break;}
                    for(const auto& hit:world.RaycastAll(q,delta,distance,0xFFFFFFFFu,own))
                    {
                        const auto entity=m_Physics.EntityFor(hit.Body);
                        if(!entity)break;
                        if(m_Project.Scene().Collider(*entity).IsTrigger)continue;
                        visible=*entity==*m_Player;break;
                    }
                }
                const auto decision=enemy.Brain.Step({q.x,q.y,q.z},{p.x,p.y,p.z},visible,dt);
                const auto v=decision.Velocity;
                (void)m_Motor.Step(enemy.Entity,{float(v[0]),0,float(v[2])},false,dt);
                damage=std::min(10000u,damage+decision.Damage);
            }
            return damage;
        }
    };
}
