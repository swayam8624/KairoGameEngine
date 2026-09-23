module;

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

export module Kairo.SceneECSBridge;

import Kairo.ECS;
import Kairo.EngineCore;
import Kairo.Foundation.Math;

export namespace kairo::runtime
{
    /// Stable authored identity retained beside the process-local ECS handle.
    /// Scene serialization never stores the ECS generation/index pair.
    struct AuthoredEntityIdentity final
    {
        kairo::engine::Entity Authored{};
    };

    struct RuntimeName final
    {
        std::string Value;
    };

    struct RuntimeTransform final
    {
        kairo::foundation::math::Transformf Local{};
    };

    struct RuntimeSceneState final
    {
        bool Enabled = true;
        std::uint32_t Layer = 0u;
        std::optional<kairo::ecs::Entity> Parent;
        std::vector<std::string> Tags;
    };

    /// One-way authoring-to-runtime extraction boundary.
    ///
    /// EngineCore Scene remains the durable/editor-facing source of truth.
    /// KairoECS is the process-local execution layout. Stable authored IDs are
    /// retained explicitly so save/load, diagnostics, physics, rendering and
    /// future replication never serialize or infer ECS slot generations.
    class SceneECSMirror final
    {
    public:
        SceneECSMirror() = default;
        SceneECSMirror(const SceneECSMirror&) = delete;
        SceneECSMirror& operator=(const SceneECSMirror&) = delete;
        SceneECSMirror(SceneECSMirror&&) noexcept = default;
        SceneECSMirror& operator=(SceneECSMirror&&) noexcept = default;

        [[nodiscard]] static SceneECSMirror Extract(const kairo::engine::Scene& scene)
        {
            SceneECSMirror result;
            const std::vector<kairo::engine::Entity> authored = scene.Entities();

            result.m_registry.ReserveEntities(authored.size());
            result.m_registry.Reserve<AuthoredEntityIdentity>(authored.size());
            result.m_registry.Reserve<RuntimeName>(authored.size());
            result.m_registry.Reserve<RuntimeTransform>(authored.size());
            result.m_registry.Reserve<RuntimeSceneState>(authored.size());
            result.m_ordered.reserve(authored.size());
            result.m_byAuthored.reserve(authored.size());

            // First pass creates all runtime identities so parent references may
            // point forward without depending on authored file/entity order.
            for (const kairo::engine::Entity entity : authored)
            {
                const kairo::ecs::Entity runtime = result.m_registry.Create();
                result.m_registry.Emplace<AuthoredEntityIdentity>(
                    runtime, AuthoredEntityIdentity{ entity });
                result.m_registry.Emplace<RuntimeName>(
                    runtime, RuntimeName{ scene.Name(entity).Value });
                result.m_registry.Emplace<RuntimeTransform>(
                    runtime, RuntimeTransform{ scene.Transform(entity).Local });
                result.m_registry.Emplace<RuntimeSceneState>(
                    runtime,
                    RuntimeSceneState{
                        .Enabled = scene.IsEnabled(entity),
                        .Layer = scene.Layer(entity),
                        .Parent = std::nullopt,
                        .Tags = scene.Tags(entity)
                    });

                result.m_byAuthored.emplace(entity.Value, runtime);
                result.m_ordered.emplace_back(entity, runtime);
            }

            // Second pass resolves hierarchy to process-local handles only after
            // every authored identity exists.
            for (const auto& [authoredEntity, runtimeEntity] : result.m_ordered)
            {
                const std::optional<kairo::engine::Entity> parent =
                    scene.Parent(authoredEntity);
                if (!parent.has_value()) continue;

                auto found = result.m_byAuthored.find(parent->Value);
                if (found == result.m_byAuthored.end())
                    throw std::logic_error(
                        "SceneECS extraction encountered a parent absent from the authored scene.");

                result.m_registry.Get<RuntimeSceneState>(runtimeEntity).Parent =
                    found->second;
            }

            result.m_registry.ClearStructuralChanges();
            return result;
        }

        [[nodiscard]] kairo::ecs::Registry& Registry() noexcept
        {
            return m_registry;
        }

        [[nodiscard]] const kairo::ecs::Registry& Registry() const noexcept
        {
            return m_registry;
        }

        [[nodiscard]] std::size_t Size() const noexcept
        {
            return m_ordered.size();
        }

        [[nodiscard]] kairo::ecs::Entity RuntimeEntity(
            kairo::engine::Entity authored) const
        {
            const auto found = m_byAuthored.find(authored.Value);
            if (found == m_byAuthored.end())
                throw std::out_of_range(
                    "Authored entity is not present in the extracted ECS world.");
            return found->second;
        }

        [[nodiscard]] kairo::engine::Entity AuthoredEntity(
            kairo::ecs::Entity runtime) const
        {
            return m_registry.Get<AuthoredEntityIdentity>(runtime).Authored;
        }

        /// Synchronizes authoring-owned fields that are safe to update without
        /// rebuilding runtime-only component topology. Runtime systems remain
        /// free to own additional ECS components independently.
        void RefreshAuthoredState(
            const kairo::engine::Scene& scene,
            kairo::engine::Entity authored)
        {
            const kairo::ecs::Entity runtime = RuntimeEntity(authored);
            m_registry.Get<RuntimeName>(runtime).Value = scene.Name(authored).Value;
            m_registry.Get<RuntimeTransform>(runtime).Local =
                scene.Transform(authored).Local;

            RuntimeSceneState& state =
                m_registry.Get<RuntimeSceneState>(runtime);
            state.Enabled = scene.IsEnabled(authored);
            state.Layer = scene.Layer(authored);
            state.Tags = scene.Tags(authored);

            const std::optional<kairo::engine::Entity> parent =
                scene.Parent(authored);
            state.Parent = parent.has_value()
                ? std::optional<kairo::ecs::Entity>(RuntimeEntity(*parent))
                : std::nullopt;
        }

    private:
        kairo::ecs::Registry m_registry;
        std::unordered_map<std::uint32_t, kairo::ecs::Entity> m_byAuthored;
        std::vector<std::pair<kairo::engine::Entity, kairo::ecs::Entity>> m_ordered;
    };
}
