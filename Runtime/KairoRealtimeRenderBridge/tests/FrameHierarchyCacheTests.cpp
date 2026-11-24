// Host-neutral test for the exact hierarchy cache used by both Editor and Player.
// Affine transforms use unsigned modulo arithmetic so composition is exact,
// noncommutative, and overflow is well-defined under sanitizers.
#include "../FrameHierarchyCache.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace
{
    struct Entity { std::uint32_t Value; };
    struct Transform
    {
        std::uint64_t Scale = 1u, Translation = 0u;
        static Transform Identity() { return {}; }
        Transform& operator*=(const Transform& rhs)
        {
            Translation += Scale * rhs.Translation;
            Scale *= rhs.Scale;
            return *this;
        }
        bool operator==(const Transform&) const = default;
    };
    struct Node
    {
        std::optional<Entity> Parent;
        bool Enabled = true;
        Transform Local;
    };
    struct Scene
    {
        std::vector<Node> Nodes;
        mutable std::size_t EnabledQueries = 0u, TransformQueries = 0u;
        bool IsEnabled(Entity e) const { ++EnabledQueries; return Nodes.at(e.Value - 1u).Enabled; }
        std::optional<Entity> Parent(Entity e) const { return Nodes.at(e.Value - 1u).Parent; }
        const Node& Transform(Entity e) const { ++TransformQueries; return Nodes.at(e.Value - 1u); }
    };
    using Cache = kairo::runtime::renderbridge::detail::FrameHierarchyCache<Scene, Entity, Transform>;

    void Check(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    std::optional<Transform> Reference(const Scene& scene, Entity entity)
    {
        std::vector<Entity> path;
        for (std::optional<Entity> p = entity; p; p = scene.Nodes[p->Value - 1u].Parent)
        {
            if (!scene.Nodes[p->Value - 1u].Enabled) return {};
            path.push_back(*p);
        }
        auto result = Transform::Identity();
        for (auto it = path.rbegin(); it != path.rend(); ++it)
            result *= scene.Nodes[it->Value - 1u].Local;
        return result;
    }
}

int main()
{
    std::mt19937 random(82501u);
    std::size_t comparisons = 0u;
    for (int trial = 0; trial < 500; ++trial)
    {
        Scene scene;
        const auto count = 1u + random() % 200u;
        scene.Nodes.resize(count);
        std::vector<Entity> ids(count);
        for (std::size_t i = 0u; i < count; ++i)
            ids[i] = { static_cast<std::uint32_t>(i + 1u) };
        auto order = ids;
        std::shuffle(order.begin(), order.end(), random);
        for (std::size_t i = 0u; i < count; ++i)
        {
            auto& node = scene.Nodes[order[i].Value - 1u];
            node.Enabled = random() % 7u != 0u;
            node.Local = { random() % 5u + 1u, random() % 100u };
            if (i != 0u && random() % 4u != 0u)
                node.Parent = order[random() % i];
        }
        Cache cache(scene, ids);
        std::shuffle(order.begin(), order.end(), random);
        for (int pass = 0; pass < 2; ++pass)
            for (const auto id : order)
            {
                Check(cache.Resolve(id) == Reference(scene, id), "Hierarchy differs from reference");
                ++comparisons;
            }
        Check(scene.EnabledQueries == count, "Enabled state was recomputed");
        Check(scene.TransformQueries <= count, "World transform was recomputed");
    }

    constexpr std::uint32_t count = 100'000u;
    Scene deep;
    deep.Nodes.resize(count);
    std::vector<Entity> ids(count);
    for (std::uint32_t i = 0u; i < count; ++i)
    {
        ids[i] = { i + 1u };
        deep.Nodes[i].Local = { 1u, 1u };
        if (i + 1u < count) deep.Nodes[i].Parent = Entity{ i + 2u };
    }
    Cache cache(deep, ids);
    for (std::uint32_t i = 0u; i < count; ++i)
    {
        const auto& world = cache.Resolve(ids[i]);
        Check(world.has_value() && world->Translation == count - i, "Deep chain differs");
    }
    Check(deep.EnabledQueries == count && deep.TransformQueries == count,
        "Deep chain did not reuse resolved ancestors");

    deep.Nodes[count / 2u].Enabled = false;
    Cache nextFrame(deep, ids);
    Check(!nextFrame.Resolve(ids.front()), "Disabled ancestor remained active");
    Check(nextFrame.Resolve(ids.back())->Translation == 1u, "Unrelated active root lost");
    std::cout << comparisons << " randomized reference comparisons; " << count
              << "-node reverse-ID chain; disabled-ancestor refresh passed\n";
}
