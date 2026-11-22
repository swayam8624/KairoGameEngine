#pragma once

// Small, platform-free part of KairoPlayer's GPU handle lifetime contract.
// An authored AssetID is durable, but a MeshHandle/TextureHandle is only valid
// for one live RendererRuntime. Native resources must be recorded for teardown
// BEFORE later binding/index allocations are allowed to throw.
//
// Include this header in the global module fragment of RuntimeRenderBridge.cppm.
// Keeping the algorithm independent from Vulkan/Metal allows failure-injection
// tests to run without a window, GPU driver, or C++ module scanner.

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace kairo::player::detail
{
    // Stage CPU vector capacity BEFORE invoking an externally visible GPU
    // create callback. Once the renderer hands back a handle, push_back must
    // not allocate/throw: even if Bind() fails, the constructor's rollback
    // already knows which native resource it needs to release.
    //
    // Ownership semantics:
    //  - create: acquires a native-backed process-local handle;
    //  - owned: tracks that handle, not the actual VkImage/MTLBuffer object;
    //  - bind: publishes a typed authored-ID -> handle mapping after tracking.
    //
    // Bind failure deliberately LEAVES the handle in owned: the enclosing
    // RuntimeRenderBridge constructor's catch calls ReleaseResources(), which
    // reverses every successful GPU allocation. Do not use this helper for
    // in-place hot-reload without designing a separate binding rollback path.
    template<class Handle, class Create, class Bind>
    [[nodiscard]] Handle CreateTrackBind(std::vector<Handle>& owned,
        Create&& create, Bind&& bind)
    {
        static_assert(std::is_nothrow_copy_constructible_v<Handle>,
            "Renderer handles must be noexcept-copyable values.");
        static_assert(std::is_nothrow_move_assignable_v<Handle>,
            "Renderer handles must be noexcept-movable for reverse cleanup.");

        if (owned.size() == owned.max_size())
            throw std::length_error("Renderer handle tracking capacity exhausted.");
        owned.reserve(owned.size() + 1u); // may throw; no new GPU handle yet

        const Handle handle = std::invoke(std::forward<Create>(create));
        owned.push_back(handle);            // capacity reserved; no new alloc
        std::invoke(std::forward<Bind>(bind), handle); // may throw; now tracked
        return handle;
    }

    // Release in reverse creation order. A successful destructor callback
    // removes its handle from the ownership journal. On a failure, RETAIN that
    // handle so a second teardown attempt can retry instead of forgetting it.
    //
    // Native cleanup errors cannot escape a noexcept destructor. Returning a
    // count lets the caller report unsuccessful teardown; it is not evidence
    // that the underlying GPU device can recover from a device-lost error.
    template<class Handle, class Destroy>
    [[nodiscard]] std::size_t ReleaseReverse(std::vector<Handle>& owned,
        Destroy&& destroy) noexcept
    {
        static_assert(std::is_nothrow_move_assignable_v<Handle>);
        std::size_t failures = 0u;
        for (std::size_t index = owned.size(); index != 0u; --index)
        {
            try
            {
                std::invoke(destroy, owned[index - 1u]);
                owned.erase(owned.begin() +
                    static_cast<std::vector<Handle>::difference_type>(index - 1u));
            }
            catch (...) { ++failures; }
        }
        return failures;
    }
}
