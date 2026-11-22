# Secure material import: validate size before allocating, parse bytes after reading

This is a small correctness and C++23 teaching continuation across [`RealtimeSceneBridge.cppm`](../Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm), [`MaterialFileSizeBudget.hpp`](../Runtime/KairoRealtimeRenderBridge/MaterialFileSizeBudget.hpp), [`MaterialFileSizeBudgetTests.cpp`](../Runtime/KairoRealtimeRenderBridge/tests/MaterialFileSizeBudgetTests.cpp), [KairoAssets `MaterialArtifact.cppm`](https://github.com/swayam8624/KairoAssets/blob/main/MaterialArtifact.cppm) and [KairoAssets `DerivedArtifact.cppm`](https://github.com/swayam8624/KairoAssets/blob/main/DerivedArtifact.cppm).

**Why this matters:** even the most defensive binary parser cannot protect you from an enormous allocation that the caller performs **before** invoking the parser.

## 1. The original failure path

The shared Editor/Player bridge loads a registered, derived material artifact from disk. The prior source performed:

```cpp
auto length = input.tellg();
std::vector<std::byte> bytes(static_cast<std::size_t>(length));
input.seekg(0);
input.read(reinterpret_cast<char*>(bytes.data()), length);
return ParseMaterialDerivedArtifact(ParseDerivedArtifact(bytes));
```

The underlying `DerivedArtifact` parser checks the declared payload count against a 1 GiB cap, and `MaterialArtifact` validates a structured PBR record. But the caller already tried to allocate `length` bytes. A corrupted or adversarial filesystem record could force huge heap allocation, memory exhaustion, or an exception before the parser's budget checks were reached.

The correct order is **measure → validate bounded length → allocate → read → parse typed envelope → parse fixed-schema material**.

## 2. Why one MiB is intentionally generous

A canonical `MaterialArtifactData` stores fixed-size PBR scalar/vector factors, a small alpha mode and double-sided bit, plus at most five optional 16-byte typed texture `AssetID` records. Its binary payload has **no variable-length texture pixels, animation clips, arbitrary strings or image mip arrays**. Those belong to separate texture/source artifacts.

Thus even a 1 MiB outer material file is orders of magnitude larger than the canonical record. The chosen `MaximumMaterialArtifactFileBytes = 1 MiB` is a defensive pre-allocation cap, not the maximum size of *all* asset categories. A future version adding variable-size material graphs must revise and version this bound explicitly.

`CheckedMaterialFileBytes(std::streamoff length)` rejects negative or above-limit stream lengths with an exception **before** any potentially large heap allocation. The caller then resizes a bounded `std::vector<std::byte>`, reads exactly those bytes, and passes the result to the existing strict versioned artifact and material parsers.

This is a *length gate*, not cryptographic integrity or a proof that a registered project path is safe. Registry path containment and actual file content validation remain independent boundaries.

## 3. Who owns which data

```text
AssetRegistry (durable typed MaterialAssetHandle)
   -> registered project-local path
   -> std::ifstream at end, tellg()
   -> CheckedMaterialFileBytes [1 MiB upper bound BEFORE resize]
   -> owned std::vector<std::byte>
   -> ParseDerivedArtifact [canonical outer type/version/payload]
   -> ParseMaterialDerivedArtifact [five texture slots + PBR factors]
   -> MakePBRMaterial [typed texture-ID -> renderer TextureHandle resolver]
   -> Vulkan/Metal descriptor and GPU resource binding
```

No step in `LoadRenderMaterial` itself creates a GPU image or a descriptor set. The CPU parser returns typed material data, after which `RuntimeRenderBridge::LoadAssets` resolves and uploads texture resources using the native backend.

`tellg()` may return a negative value on error. Converting that to unsigned `size_t` first would be dangerous; the size gate takes signed `std::streamoff` and checks negativity explicitly before casting to `std::uintmax_t` for the bound comparison.

## 4. Actual regression assertions

The CPU-only [size-budget test](../Runtime/KairoRealtimeRenderBridge/tests/MaterialFileSizeBudgetTests.cpp) covers lengths of zero, a small canonical-sized value, **exactly the maximum**, **one byte over the maximum**, negative `tellg`, and the maximum `std::streamoff` representable size. None of those tests needs to allocate a giant vector or open a giant sparse file.

The test is registered in [the shared bridge CMake file](../Runtime/KairoRealtimeRenderBridge/CMakeLists.txt) as `KairoMaterialFileSizeBudgetTests` when `BUILD_TESTING=ON`. The [standalone CPU regression script](../scripts/run_renderer_resource_lifetime_regressions.sh) also compiles it directly with a C++23 compiler, alongside the Player handle-journal, renderer publication, and Vulkan frame-state tests.

These tests prove only the *numeric gate's behavior*. The existing KairoAssets serialization/parser tests remain responsible for the actual typed binary representation. An integration test must still register a material AssetID, write a real canonical derived file, load it, then confirm altered/truncated/oversized files fail at the intended stage.

**Automated guard:** the [Player/asset CPU workflow](../.github/workflows/cpu-player-resource-guards.yml) runs `MaterialFileSizeBudgetTests.cpp` and the Player handle-journal regression with GCC and Clang on relevant repository pushes/PRs. The [manual standalone runner](../scripts/run_renderer_resource_lifetime_regressions.sh) can also test the renderer's frame-state journal and native-publication helper when the sibling checkout is present.

## 5. Deeper lessons

A file-size cap is not just a memory-usage optimization. It is part of the trust boundary between authored/editor-controlled storage and runtime allocations.

For a **variable-size** image or mesh artifact, validate separate dimension, record-count and cumulative byte budgets, reject multiplication overflow before allocation, and ensure decoded payload size equals the declared dimensions. For a **fixed-size** material, an independent tight file cap is practical and avoids accepting gigabytes merely because the generic derived-cache format allows larger image payloads.

If the schema changes to contain procedural graphs, long strings or embedded shader code, introduce a new format version with explicit bounded field lengths, a validated aggregate budget, and tests for exact-maximum, maximum+1, overflow and truncated inputs.

**Evidence status:** the size gate is wired to the real source loader and a CPU regression test is committed. The exact multi-repository native superbuild was not run during this change. The new test neither uploads a GPU material nor demonstrates remote/hostile filesystem path containment.
