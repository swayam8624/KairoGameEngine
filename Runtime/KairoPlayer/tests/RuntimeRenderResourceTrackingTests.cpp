// CPU-only regression suite for the exact handle-adoption/cleanup helpers
// used in RuntimeRenderBridge.cppm. No GLFW window or GPU is required:
// simulated callbacks let us inject create, bind and destroy exceptions.
#include "../RuntimeRenderResourceTracking.hpp"

#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }
}

int main()
{
    try
    {
        using kairo::player::detail::CreateTrackBind;
        using kairo::player::detail::ReleaseReverse;
        using Handle = std::uint64_t;

        std::vector<Handle> owned;
        std::vector<Handle> bound;
        int created = 0;

        // A successfully created handle must already be present in the
        // cleanup journal when binding begins (binding can allocate/throw).
        const Handle first = CreateTrackBind(owned,
            [&] { ++created; return Handle{ 11u }; },
            [&](Handle handle)
            {
                Check(owned.size() == 1u && owned.back() == handle,
                    "Handle was published before tracking");
                bound.push_back(handle);
            });
        Check(first == 11u && created == 1 && bound == std::vector<Handle>{ 11u },
            "Initial tracked upload did not bind");

        // If a typed AssetID -> MeshHandle/TextureHandle binding throws,
        // the newly created native-backed handle is STILL in the journal,
        // so RuntimeRenderBridge's constructor catch can release it.
        bool bindThrew = false;
        try
        {
            (void)CreateTrackBind(owned,
                [&] { ++created; return Handle{ 22u }; },
                [&](Handle handle)
                {
                    Check(owned.back() == handle, "Binder saw untracked handle");
                    throw std::runtime_error("injected binding failure");
                });
        }
        catch (const std::runtime_error&) { bindThrew = true; }
        Check(bindThrew && owned == std::vector<Handle>{ 11u, 22u },
            "Binding failure dropped an acquired handle");

        // If native resource creation throws, no handle is returned and the
        // journal stays unchanged; the binder must never be invoked.
        bool createThrew = false;
        try
        {
            (void)CreateTrackBind(owned,
                [&]() -> Handle
                {
                    ++created;
                    throw std::runtime_error("injected creation failure");
                },
                [&](Handle) { throw std::runtime_error("unexpected binder call"); });
        }
        catch (const std::runtime_error&) { createThrew = true; }
        Check(createThrew && owned == std::vector<Handle>{ 11u, 22u },
            "Creation failure changed the resource journal");

        (void)CreateTrackBind(owned,
            [&] { ++created; return Handle{ 33u }; },
            [&](Handle handle) { bound.push_back(handle); });
        Check(created == 4 && owned.size() == 3u,
            "Later successful creation was not tracked");

        std::vector<Handle> attemptedDestroy;
        const std::size_t firstFailures = ReleaseReverse(owned,
            [&](Handle handle)
            {
                attemptedDestroy.push_back(handle);
                if (handle == 22u)
                    throw std::runtime_error("injected GPU cleanup failure");
            });

        Check(firstFailures == 1u &&
              attemptedDestroy == std::vector<Handle>{ 33u, 22u, 11u },
            "Cleanup did not reverse creation order or count failure");
        Check(owned == std::vector<Handle>{ 22u },
            "Failed GPU destruction was silently removed from journal");

        // A later attempt after scene retirement/device recovery can retry
        // the failed handle instead of losing ownership information.
        const std::size_t retryFailures = ReleaseReverse(owned,
            [&](Handle handle)
            {
                Check(handle == 22u, "Unexpected handle during cleanup retry");
                attemptedDestroy.push_back(handle);
            });
        Check(retryFailures == 0u && owned.empty() &&
              attemptedDestroy.back() == 22u,
            "Cleanup retry did not release the remaining handle");

        // Already-empty cleanup is an idempotent no-op.
        Check(ReleaseReverse(owned,
            [&](Handle) { throw std::runtime_error("unexpected destruction"); }) == 0u,
            "Cleanup on an empty journal was not idempotent");

        std::puts("KairoPlayer renderer resource tracking tests passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Render resource tracking regression: %s\n", error.what());
        return 1;
    }
}
