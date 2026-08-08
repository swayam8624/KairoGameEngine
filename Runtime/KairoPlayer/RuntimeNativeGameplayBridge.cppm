module;

#include <filesystem>
#include <functional>
#include <stdexcept>
#include <utility>

export module Kairo.Player.RuntimeNativeGameplayBridge;

import Kairo.EngineCore;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeProject;

export namespace kairo::player
{
    using NativeGameplayRegistration =
        std::function<void(kairo::engine::NativeGameplayRegistry&)>;

    /// Process-local registration boundary for game-native C++ modules linked
    /// into a Player executable. A generic Player cannot manufacture arbitrary
    /// project C++ types; linked modules register their reflected factories here
    /// before RuntimeNativeGameplayBridge is constructed.
    [[nodiscard]] inline kairo::engine::NativeGameplayRegistry&
    PlayerNativeGameplayRegistry() noexcept
    {
        static kairo::engine::NativeGameplayRegistry registry;
        return registry;
    }

    inline void RegisterPlayerNativeGameplay(NativeGameplayRegistration registration)
    {
        if (!registration)
            throw std::invalid_argument("Player native gameplay registration callback cannot be empty.");
        registration(PlayerNativeGameplayRegistry());
    }

    class RuntimeNativeGameplayBridge final : public RuntimeFixedStepListener
    {
    public:
        RuntimeNativeGameplayBridge(RuntimeProject& project,
            const kairo::engine::NativeGameplayRegistry& registry)
            : m_Project(project),
              m_World(project.Scene()),
              m_Runtime(m_World, registry)
        {
            const auto manifestPath = project.Root() /
                kairo::engine::DefaultNativeGameplayManifestPath;
            std::error_code error;
            if (!std::filesystem::exists(manifestPath, error))
            {
                if (error) throw std::runtime_error(
                    "Failed while checking native gameplay manifest: " + error.message());
                return;
            }
            if (!std::filesystem::is_regular_file(manifestPath, error) || error)
                throw std::invalid_argument(
                    "Native gameplay manifest is not a readable regular file: " + manifestPath.string());
            const auto manifest = kairo::engine::LoadNativeGameplayManifest(manifestPath);
            kairo::engine::AttachNativeGameplayManifest(
                m_Runtime, manifest, project.Scene(), registry);
        }

        [[nodiscard]] std::size_t InstanceCount() const noexcept
        { return m_Runtime.InstanceCount(); }

        void BeginPlay()
        {
            SynchronizeIn();
            m_Runtime.BeginPlay();
            SynchronizeOut();
        }

        void BeforePhysicsStep(float fixedDeltaSeconds) override
        {
            SynchronizeIn();
            m_Runtime.FixedUpdate(static_cast<double>(fixedDeltaSeconds));
            SynchronizeOut();
        }

        void Update(double deltaSeconds)
        {
            SynchronizeIn();
            m_Runtime.Update(deltaSeconds);
            SynchronizeOut();
        }

        void DispatchEvent(const kairo::engine::Event& event)
        {
            SynchronizeIn();
            m_Runtime.DispatchEvent(event);
            SynchronizeOut();
        }

        void EndPlay()
        {
            SynchronizeIn();
            m_Runtime.EndPlay();
            SynchronizeOut();
        }

        void Reload(const kairo::engine::NativeGameplayRegistry& registry,
            kairo::engine::NativeReloadPolicy policy =
                kairo::engine::NativeReloadPolicy::PreserveCompatibleProperties)
        {
            m_Runtime.Reload(registry, policy);
        }

    private:
        RuntimeProject& m_Project;
        kairo::engine::RuntimeWorld m_World;
        kairo::engine::NativeGameplayRuntime m_Runtime;

        void SynchronizeIn()
        {
            m_World.Replace(m_Project.Scene());
        }

        void SynchronizeOut()
        {
            m_Project.Scene() = m_World.Snapshot();
        }
    };
}
