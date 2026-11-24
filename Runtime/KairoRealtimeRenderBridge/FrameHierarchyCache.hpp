#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

namespace kairo::runtime::renderbridge::detail
{
    // One extraction only: the caller supplies the scene's sorted stable IDs.
    // No persistent dirty flags or second authoritative scene. A cached entry
    // contains the same root-first TRS as Scene::WorldTransform, or nullopt if
    // any ancestor is disabled. Scratch and entries are contiguous, not one
    // heap allocation per entity or ancestry walk.
    template<class Scene, class Entity, class Transform>
    class FrameHierarchyCache final
    {
        struct Entry final
        {
            bool Resolved = false;
            std::optional<Transform> World;
        };

    public:
        FrameHierarchyCache(const Scene& scene, const std::vector<Entity>& entities)
            : m_Scene(scene), m_Entities(entities), m_Entries(entities.size()) {}

        [[nodiscard]] const std::optional<Transform>& Resolve(Entity entity)
        {
            const auto requested = IndexOf(entity);
            if (m_Entries[requested].Resolved) return m_Entries[requested].World;

            m_Pending.clear();
            auto current = requested;
            std::optional<Transform> world;
            for (;;)
            {
                auto& entry = m_Entries[current];
                if (entry.Resolved)
                {
                    world = entry.World;
                    break;
                }
                const auto id = m_Entities[current];
                if (!m_Scene.IsEnabled(id))
                {
                    entry.Resolved = true;
                    break;
                }
                m_Pending.push_back(current);
                const auto parent = m_Scene.Parent(id);
                if (!parent)
                {
                    world = Transform::Identity();
                    break;
                }
                current = IndexOf(*parent);
            }

            // Scene::SetParent enforces acyclicity. Compose root-to-leaf in
            // exactly the original order, including identity * root.
            for (auto it = m_Pending.rbegin(); it != m_Pending.rend(); ++it)
            {
                if (world) *world *= m_Scene.Transform(m_Entities[*it]).Local;
                auto& entry = m_Entries[*it];
                entry.World = world;
                entry.Resolved = true;
            }
            return m_Entries[requested].World;
        }

    private:
        [[nodiscard]] std::size_t IndexOf(Entity entity) const
        {
            const auto found = std::lower_bound(m_Entities.begin(), m_Entities.end(), entity,
                [](Entity a, Entity b) { return a.Value < b.Value; });
            if (found == m_Entities.end() || found->Value != entity.Value)
                throw std::logic_error("Render hierarchy references an unknown entity.");
            return static_cast<std::size_t>(found - m_Entities.begin());
        }

        const Scene& m_Scene;
        const std::vector<Entity>& m_Entities;
        std::vector<Entry> m_Entries;
        std::vector<std::size_t> m_Pending;
    };
}
