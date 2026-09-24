#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

import Kairo.Foundation.Spatial;
import Kairo.Foundation.Geometry.AABB;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Foundation.PhysicsMath;
import Kairo.Foundation.Math.Vector;
import Kairo.Assets;
import Kairo.Renderer;
import Kairo.EngineCore.WorldStreaming;

namespace
{
    using Clock = std::chrono::steady_clock;

    template<class Function>
    double Milliseconds(Function&& function)
    {
        const auto begin = Clock::now();
        function();
        return std::chrono::duration<double, std::milli>(
            Clock::now() - begin).count();
    }
}

int main(int argc, char** argv)
{
    using kairo::foundation::math::Vec3f;
    using namespace kairo::foundation::spatial;
    using namespace kairo::foundation::physics;
    using namespace kairo::assets;
    using namespace kairo::renderer;
    using namespace kairo::engine;

    // Spatial: 4096 static primitives, 512 deterministic accelerated queries.
    std::vector<SpatialPrimitive> primitives;
    primitives.reserve(4096u);
    SpatialID nextId = 1u;
    for (std::size_t z = 0u; z < 64u; ++z)
        for (std::size_t x = 0u; x < 64u; ++x)
        {
            const Vec3f min{
                static_cast<float>(x) * 1.25f, -0.5f,
                static_cast<float>(z) * 1.25f };
            primitives.push_back({
                nextId++,
                SpatialAABB::FromMinMax(min, min + Vec3f{ 0.75f, 1.0f, 0.75f }),
                0x1u });
        }
    BVH bvh;
    const double spatialBuildMs = Milliseconds([&]
    {
        BVHBuildSettings settings;
        settings.MaxLeafSize = 8u;
        settings.UseMortonOrdering = true;
        bvh = BuildBVH(primitives, settings);
    });
    std::uint64_t spatialHits = 0u;
    const double spatialQueryMs = Milliseconds([&]
    {
        for (std::size_t sample = 0u; sample < 512u; ++sample)
        {
            const float x = static_cast<float>((sample * 17u) % 64u) * 1.25f;
            const float z = static_cast<float>((sample * 29u) % 64u) * 1.25f;
            const auto query = SpatialAABB::FromMinMax(
                Vec3f{ x - 2.0f, -1.0f, z - 2.0f },
                Vec3f{ x + 4.0f, 1.0f, z + 4.0f });
            spatialHits += QueryAABB(bvh, query).IDs.size();
        }
    });
    if (!bvh.IsValid()) return 10;

    // Physics: deterministic 36-body scene, report accumulated internal profile.
    PhysicsWorld world;
    world.Settings.EnableSleeping = false;
    RigidBodyDesc floorDesc;
    floorDesc.Type = BodyType::Static;
    floorDesc.Mass = StaticMassProperties();
    const BodyID floor = world.CreateRigidBody(floorDesc);
    (void)world.AddCollider(floor, PlaneCollider{ Vec3f::Up(), 0.0f });
    for (int z = 0; z < 6; ++z)
        for (int x = 0; x < 6; ++x)
        {
            RigidBodyDesc body;
            body.Type = BodyType::Dynamic;
            body.State.Position = Vec3f{
                static_cast<float>(x) * 1.05f - 2.625f,
                1.0f + static_cast<float>((x + z) % 4) * 1.05f,
                static_cast<float>(z) * 1.05f - 2.625f };
            body.Mass = SphereMassProperties(0.45f, 1.0f);
            const BodyID id = world.CreateRigidBody(body);
            (void)world.AddCollider(id, SphereCollider{ 0.45f });
        }
    double physicsProfileMs = 0.0;
    const double physicsWallMs = Milliseconds([&]
    {
        for (int step = 0; step < 360; ++step)
        {
            world.Step(1.0f / 120.0f);
            physicsProfileMs += world.LastStepProfile().StepMs;
        }
    });
    for (const auto& body : world.Bodies())
        if (!std::isfinite(body.State.Position.x)
            || !std::isfinite(body.State.Position.y)
            || !std::isfinite(body.State.Position.z)) return 11;

    // Assets: content-addressed DDC write/read at moderate scale.
    const auto ddcRoot = std::filesystem::temp_directory_path()
        / "kairo-wave-c-benchmark-ddc";
    std::error_code cleanupError;
    std::filesystem::remove_all(ddcRoot, cleanupError);
    DerivedDataCache cache(ddcRoot);
    std::vector<DerivedDataKey> ddcKeys;
    ddcKeys.reserve(256u);
    std::uint64_t ddcChecksum = 0u;
    const double ddcWriteMs = Milliseconds([&]
    {
        for (std::size_t index = 0u; index < 256u; ++index)
        {
            std::vector<std::byte> bytes(4096u);
            for (std::size_t byte = 0u; byte < bytes.size(); ++byte)
                bytes[byte] = static_cast<std::byte>(
                    (index * 31u + byte * 17u) & 0xffu);
            const auto source = FingerprintBytes(bytes);
            const auto key = MakeDerivedDataKey(
                source, AssetType::Mesh, "kairo.wave-c", "1",
                "entry=" + std::to_string(index));
            cache.Store(key, bytes);
            ddcKeys.push_back(key);
        }
    });
    const double ddcReadMs = Milliseconds([&]
    {
        for (const auto& key : ddcKeys)
        {
            const auto bytes = cache.Load(key);
            if (!bytes.empty())
                ddcChecksum += std::to_integer<unsigned int>(bytes.front());
        }
    });
    std::filesystem::remove_all(ddcRoot, cleanupError);

    // Renderer: compile and execute a 256-pass transient chain.
    RenderGraph graph;
    std::vector<RenderResourceHandle> resources;
    resources.reserve(256u);
    for (std::size_t index = 0u; index < 256u; ++index)
        resources.push_back(graph.AddResource({
            "Transient-" + std::to_string(index),
            RenderResourceKind::Texture, 4096u, true }));
    (void)graph.AddPass("Pass-0", {
        { resources[0], RenderAccessMode::Write,
          RenderResourceState::ColorAttachment } });
    for (std::size_t index = 1u; index < resources.size(); ++index)
        (void)graph.AddPass("Pass-" + std::to_string(index), {
            { resources[index - 1u], RenderAccessMode::Read,
              RenderResourceState::ShaderRead },
            { resources[index], RenderAccessMode::Write,
              RenderResourceState::ColorAttachment } });
    CompiledRenderGraph compiled;
    const double renderCompileMs = Milliseconds([&] { compiled = graph.Compile(); });
    RenderGraphExecutionProfile renderProfile;
    const double renderExecuteWallMs =
        Milliseconds([&] { renderProfile = compiled.Execute(); });
    if (compiled.AliasSlots().size() > 2u
        || compiled.TransientAllocationBytes() > 8192u
        || renderProfile.Passes.size() != 256u) return 12;

    // Streaming: 4096 cells and a moving observer, while enforcing budgets.
    WorldStreamingConfig streamingConfig;
    streamingConfig.CellSize = 64.0;
    streamingConfig.LoadRadius = 220.0;
    streamingConfig.KeepRadius = 320.0;
    streamingConfig.MaximumLoadsPerUpdate = 24u;
    streamingConfig.MaximumUnloadsPerUpdate = 48u;
    streamingConfig.MaximumCommittedCells = 96u;
    streamingConfig.MaximumCommittedBytes = 96u * 4096u;
    WorldStreamingRuntime streaming(streamingConfig);
    for (int z = 0; z < 64; ++z)
        for (int x = 0; x < 64; ++x)
            streaming.RegisterCell({
                { x, z },
                "world/" + std::to_string(x) + "_" + std::to_string(z) + ".cell",
                4096u, (x + z) % 5, false });

    std::size_t peakCells = 0u;
    std::uint64_t peakBytes = 0u;
    const double streamingMs = Milliseconds([&]
    {
        for (int step = 0; step < 96; ++step)
        {
            const std::array observers{
                WorldStreamingObserver{
                    kairo::foundation::math::Vec3d{
                        32.0 + static_cast<double>((step * 41) % (64 * 64)),
                        0.0,
                        32.0 + static_cast<double>((step * 67) % (64 * 64)) },
                    1.0 }
            };
            const auto plan = streaming.PlanUpdate(observers);
            for (const auto& request : plan.Unloads)
                streaming.CompleteUnload(request.Coordinate, true);
            for (const auto& request : plan.Loads)
                streaming.CompleteLoad(request.Coordinate, true);
            peakCells = std::max(peakCells, streaming.CommittedCellCount());
            peakBytes = std::max(peakBytes, streaming.CommittedBytes());
        }
    });
    if (peakCells > streamingConfig.MaximumCommittedCells
        || peakBytes > streamingConfig.MaximumCommittedBytes) return 13;

    const std::string json =
        "{\n"
        "  \"schema\": \"kairo.wave-c.benchmark.v1\",\n"
        "  \"spatial\": {\"primitives\": 4096, \"queries\": 512, \"build_ms\": "
            + std::to_string(spatialBuildMs) + ", \"query_ms\": "
            + std::to_string(spatialQueryMs) + ", \"hits\": "
            + std::to_string(spatialHits) + "},\n"
        "  \"physics\": {\"dynamic_bodies\": 36, \"steps\": 360, \"wall_ms\": "
            + std::to_string(physicsWallMs) + ", \"profile_sum_ms\": "
            + std::to_string(physicsProfileMs) + "},\n"
        "  \"assets\": {\"ddc_entries\": 256, \"entry_bytes\": 4096, \"write_ms\": "
            + std::to_string(ddcWriteMs) + ", \"read_ms\": "
            + std::to_string(ddcReadMs) + ", \"checksum\": "
            + std::to_string(ddcChecksum) + "},\n"
        "  \"renderer\": {\"passes\": 256, \"compile_ms\": "
            + std::to_string(renderCompileMs) + ", \"execute_wall_ms\": "
            + std::to_string(renderExecuteWallMs) + ", \"profile_ms\": "
            + std::to_string(renderProfile.TotalMilliseconds)
            + ", \"alias_slots\": " + std::to_string(compiled.AliasSlots().size())
            + ", \"transient_bytes\": "
            + std::to_string(compiled.TransientAllocationBytes()) + "},\n"
        "  \"streaming\": {\"cells\": 4096, \"updates\": 96, \"plan_ms\": "
            + std::to_string(streamingMs) + ", \"peak_cells\": "
            + std::to_string(peakCells) + ", \"peak_bytes\": "
            + std::to_string(peakBytes) + "}\n"
        "}\n";

    std::cout << json;
    if (argc > 1)
    {
        std::ofstream output(argv[1], std::ios::binary | std::ios::trunc);
        if (!output) return 20;
        output << json;
        if (!output) return 21;
    }
    return 0;
}
