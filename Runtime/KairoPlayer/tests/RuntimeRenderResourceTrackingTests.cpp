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

        using kairo::player::detail::ReleaseSceneResources;
        std::vector<Handle> meshJournal{ 101u, 102u };
        std::vector<Handle> textureJournal{ 201u };
        bool submittedSceneStillReferencesHandles = true;
        std::vector<std::string> releaseOrder;

        // The current RenderScene must be cleared before any backend is
        // asked to destroy its native mesh or texture allocations.
        const auto successfulTeardown = ReleaseSceneResources(
            meshJournal, textureJournal,
            [&]
            {
                releaseOrder.push_back("unbind");
                submittedSceneStillReferencesHandles = false;
            },
            [&](Handle handle)
            {
                Check(!submittedSceneStillReferencesHandles,
                    "Mesh destruction preceded submitted scene unbind");
                releaseOrder.push_back("mesh" + std::to_string(handle));
            },
            [&](Handle handle)
            {
                Check(!submittedSceneStillReferencesHandles,
                    "Texture destruction preceded submitted scene unbind");
                releaseOrder.push_back("texture" + std::to_string(handle));
            });
        Check(successfulTeardown == 0u && meshJournal.empty() &&
              textureJournal.empty(), "Successful teardown retained handles");
        Check(releaseOrder == std::vector<std::string>{
            "unbind", "mesh102", "mesh101", "texture201" },
            "Unbind-first release sequencing changed");

        // A rejected or failed scene-unbind must prevent destruction of ALL
        // resources, leaving both journals for a safe later recovery attempt.
        meshJournal = { 301u };
        textureJournal = { 401u };
        bool destroyedWhileBound = false;
        const auto clearFailures = ReleaseSceneResources(
            meshJournal, textureJournal,
            [] { throw std::runtime_error("injected unbind failure"); },
            [&](Handle) { destroyedWhileBound = true; },
            [&](Handle) { destroyedWhileBound = true; });
        Check(clearFailures == 3u && !destroyedWhileBound &&
              meshJournal == std::vector<Handle>{ 301u } &&
              textureJournal == std::vector<Handle>{ 401u },
            "Unbind failure lost or destroyed still-submitted resources");

        // Simulated device recovery allows an explicit later retry, with
        // release order still deterministic and both journals emptied.
        const auto recovered = ReleaseSceneResources(meshJournal, textureJournal,
            [] {}, [](Handle) {}, [](Handle) {});
        Check(recovered == 0u && meshJournal.empty() &&
              textureJournal.empty(), "Failed teardown recovery retry");

        // Exhaust all failure patterns on a small journal. The callback must
        // see reverse order exactly once, and retained handles must retry in
        // the original reverse order, even when failures are interleaved.
        for (unsigned mask = 0u; mask < 256u; ++mask)
        {
            std::vector<Handle> journal{ 0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u };
            std::vector<Handle> expected;
            for (Handle handle = 0u; handle < 8u; ++handle)
                if ((mask & (1u << handle)) != 0u) expected.push_back(handle);
            std::vector<Handle> attempted;
            const auto failures = ReleaseReverse(journal, [&](Handle handle)
            {
                attempted.push_back(handle);
                if ((mask & (1u << handle)) != 0u)
                    throw std::runtime_error("injected retirement failure");
            });
            Check(attempted == std::vector<Handle>{ 7u, 6u, 5u, 4u, 3u, 2u, 1u, 0u },
                "Mixed retirement did not visit original reverse order");
            Check(journal == expected && failures == expected.size(),
                "Mixed retirement lost or reordered failed handles");
            attempted.clear();
            Check(ReleaseReverse(journal, [&](Handle handle) { attempted.push_back(handle); }) == 0u,
                "Retry of mixed retirement failed");
            Check(attempted == std::vector<Handle>(expected.rbegin(), expected.rend()),
                "Retried handles were not destroyed in reverse creation order");
            Check(journal.empty(), "Retry did not empty journal");
        }

        // Count capacity changes, not timing, so this test is stable on CI.
        // Geometric reserve must keep 65,536 uploads to at most 17 growths.
        std::vector<Handle> large;
        std::size_t growths = 0u;
        for (Handle handle = 0u; handle < 65'536u; ++handle)
        {
            const auto previous = large.capacity();
            (void)CreateTrackBind(large, [&, handle]
            {
                Check(large.capacity() > large.size(), "Native create preceded capacity reservation");
                return handle;
            }, [](Handle) {});
            if (large.capacity() != previous) ++growths;
        }
        Check(growths <= 17u, "Resource journal growth is not geometric");
        const auto largeFailures = ReleaseReverse(large, [](Handle handle)
        {
            if ((handle & 1u) != 0u) throw std::runtime_error("injected alternating failure");
        });
        Check(largeFailures == 32'768u && large.size() == 32'768u,
            "Large alternating cleanup retained the wrong count");
        for (std::size_t index = 0u; index < large.size(); ++index)
            Check(large[index] == index * 2u + 1u, "Large retry order corrupted");
        Check(ReleaseReverse(large, [](Handle) {}) == 0u && large.empty(),
            "Large journal retry did not retire all handles");

        std::puts("KairoPlayer renderer resource tracking tests passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Render resource tracking regression: %s\n", error.what());
        return 1;
    }
}
