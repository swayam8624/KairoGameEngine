#include "../FrameAssetCache.hpp"
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }
    struct Hash
    {
        static inline std::size_t Calls = 0;
        std::size_t operator()(int key) const noexcept
        {
            ++Calls;
            return std::hash<int>{}(key);
        }
    };
    using Cache = kairo::runtime::renderbridge::detail::FrameAssetCache<int, std::string, Hash>;
}
int main()
{
    Cache cache;
    int resolves = 0;
    const auto resolve = [&](int key) { ++resolves; return std::to_string(key); };
    const auto& first = cache.Get(42, [&] { return resolve(42); });
    Hash::Calls = 0;
    for (int i = 0; i < 100'000; ++i)
        Check(cache.Get(42, [&] { return resolve(42); }) == "42", "Adjacent hit differs");
    Check(Hash::Calls == 0 && resolves == 1, "Adjacent hits hashed or resolved");

    // Rehash repeatedly; borrowed references and last-hit pointer must remain valid.
    for (int i = 0; i < 4096; ++i)
        Check(cache.Get(i, [&] { return resolve(i); }) == std::to_string(i), "First lookup differs");
    Check(first == "42", "Rehash invalidated borrowed value");
    for (int i = 4095; i >= 0; --i)
        Check(cache.Get(i, [&] { return resolve(i); }) == std::to_string(i), "Reverse hit differs");
    Check(resolves == 4096, "Repeated nonadjacent asset was resolved again");

    bool failed = false;
    try
    {
        (void)cache.Get(-1, []() -> std::string { throw std::out_of_range("missing"); });
    }
    catch (const std::out_of_range&) { failed = true; }
    Check(failed, "Missing asset failure hidden");
    Check(cache.Get(-1, [] { return std::string("recovered"); }) == "recovered",
        "Failed lookup was cached");
    Cache nextFrame;
    Check(nextFrame.Get(42, [] { return std::string("replacement"); }) == "replacement",
        "Frame-local cache retained a stale value");
    std::cout << "100000 adjacent hits without hashing; 4096 unique resolutions; rehash, retry and frame refresh passed\n";
}
