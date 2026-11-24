#pragma once

#include <functional>
#include <unordered_map>
#include <utility>

namespace kairo::runtime::renderbridge::detail
{
    // Cache successful resolutions for a single immutable extraction only.
    // The resolver remains the authoritative typed registry/binding check.
    // Recreate the cache every extraction, so removal, replacement or rebinding
    // between frames cannot reuse a stale result. No cross-frame state.
    // A pointer/reference stored as a Value must outlive the extraction.
    template<class Key, class Value, class Hash = std::hash<Key>>
    class FrameAssetCache final
    {
    public:
        template<class Resolve>
        [[nodiscard]] const Value& Get(const Key& key, Resolve&& resolve)
        {
            // Adjacent entities often share the same asset. Avoid hashing
            // on these runs; unordered_map references survive rehashing.
            if (m_Last && m_Last->first == key) return m_Last->second;
            auto found = m_Values.find(key);
            if (found == m_Values.end())
            {
                // Resolve before publishing, so a failed lookup never leaves
                // a placeholder or masks an error on a subsequent attempt.
                auto value = std::invoke(std::forward<Resolve>(resolve));
                found = m_Values.emplace(key, std::move(value)).first;
            }
            m_Last = &*found;
            return m_Last->second;
        }

        FrameAssetCache() = default;
        // The fast-path pointer borrows the map's nodes. Do not let implicit
        // copy/move operations accidentally preserve a pointer into another map.
        FrameAssetCache(const FrameAssetCache&) = delete;
        FrameAssetCache& operator=(const FrameAssetCache&) = delete;
        FrameAssetCache(FrameAssetCache&&) = delete;
        FrameAssetCache& operator=(FrameAssetCache&&) = delete;

    private:
        using Map = std::unordered_map<Key, Value, Hash>;
        Map m_Values;
        const typename Map::value_type* m_Last = nullptr;
    };
}
