# Asset → Scene → GPU source-teaching pass — integration record

**Date:** 2026-11-22. **Integration target:** active default `main` branches. This ledger records completed **comment/documentation commits**, not a native build, an asset-cook, validation-layer evidence, or a merge of historic divergent PRs.

## Source coverage completed in this pass

| Repository and source | New teaching sections | What the reader can now trace |
| --- | ---: | --- |
| [KairoGameEngine / Player `RuntimeRenderBridge.cppm`](../Runtime/KairoPlayer/RuntimeRenderBridge.cppm) | 18 | RuntimeProject borrowing, load order, texture semantics, GLTF animation clocks, GPU handle tracking, teardown exceptions |
| [KairoGameEngine / `RealtimeSceneBridge.cppm`](../Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm) | 15 | Typed import and derived caches, borrowed registry, glTF node/skin source, camera, material/texture maps, per-frame CPU extraction |
| [KairoRenderer / `AssetAdapters.cppm`](https://github.com/swayam8624/KairoRenderer/blob/main/AssetAdapters.cppm) | 8 | PBR material factors, URI semantics, cycle-safe glTF hierarchy, static versus skinned model matrices |
| [KairoEngineCore / `RuntimeComponents.cppm`](https://github.com/swayam8624/KairoEngineCore/blob/main/RuntimeComponents.cppm) | 5 | Stable authored typed asset handles, visibility masks, material slot policy, imported scene instance identity |
| [KairoAssets / `TextureImporter.cppm`](https://github.com/swayam8624/KairoAssets/blob/main/TextureImporter.cppm) | 9 | Canonical import recipe, stb source-byte decoding, finite HDR path, RAII decoder cleanup |
| [KairoAssets / `Importer.cppm`](https://github.com/swayam8624/KairoAssets/blob/main/Importer.cppm) | 6 | 1 GiB bounded source read, content+recipe key, cached typed artifacts, publish-before-provenance |
| [KairoAssets / `ImportDatabase.cppm`](https://github.com/swayam8624/KairoAssets/blob/main/ImportDatabase.cppm) | 6 | Thread-safe record copies, candidate-swap replacement, revision/content distinction |
| **Implementation total** | **67** | **Seven implementation files** |

## Existing test assertions annotated

| Test file | Sections | What is verified in SOURCE |
| --- | ---: | --- |
| [Shared bridge tests](../Runtime/KairoRealtimeRenderBridge/tests/RealtimeSceneBridgeTests.cpp) | 5 | Material/light/environment conversion, scene expansion/layers, static/skinned transform rule, invalid animation binding, camera |
| [Editor bridge tests](https://github.com/swayam8624/KairoEditor/blob/main/tests/SceneRenderBridgeTests.cpp) | 6 | Identical shared CPU extraction, missing bindings, authored lighting, builtin meshes, OBJ import/cache hit |
| [Renderer unit tests](https://github.com/swayam8624/KairoRenderer/blob/main/tests/RendererTests.cpp) | 2 | Material PBR channel conversions and glTF URI/transform semantics |
| [EngineCore unit tests](https://github.com/swayam8624/KairoEngineCore/blob/main/tests/EngineCoreTests.cpp) | 1 | Portable scene components, visibility, validation before mutation |
| [Assets Phase 2 tests](https://github.com/swayam8624/KairoAssets/blob/main/tests/Phase2AssetTests.cpp) | 5 | Source TGA stb decode, mip chain, material and glTF artifact persistence, importer registry versions |
| **Test commentary total** | **19** | **Five test files, NOT executed here** |

**New deep guide:** [From a saved scene to a GPU draw](ASSET_SCENE_TO_GPU_LIFETIME_LAB.md) with a C++23 API sketch, identity/lifetime table, exact glTF static-versus-skinned transform example, texture semantic rules, evidence map and concrete failure-injection exercises.

All five repositories' README and linked source reading map reference that guide. Also repaired two stale cross-repository `docs/teaching-walkthrough-20261121` URLs in KairoEditor `SceneRenderBridge.cppm` and KairoAssets `docs/SOURCE_READING_ORDER.md` to refer to the active `main` sources.

## End-to-end call graph

```text
KairoAssets AssetID / typed metadata
  -> ImportSourceAsset [bounded read + fingerprint + derived cache]
  -> ParseTextureDerivedArtifact / ParseMeshDerivedArtifact /
     ParseGltfSceneDerivedArtifact / ParseMaterialDerivedArtifact
  -> KairoRenderer CPU Mesh, PBRMaterial, TextureArtifactData
  -> RuntimeRenderBridge::LoadAssets
       RendererRuntime::CreateMesh / CreateTexture [native GPU allocation]
       RenderAssetBindings::BindMesh/BindTexture/BindMaterial/BindGltfScene
  -> EngineCore::Scene [Entity, WorldTransform, render layers, scene instances]
  -> RealtimeSceneBridge::BuildRenderScene [CPU-neutral draws + ObjectID]
  -> RendererRuntime::SetRenderScene / DrawFrame
  -> Vulkan/Metal command encoding, native GPU queue synchronization
```

An `AssetID` is durable; a `DerivedDataKey` identifies a content+recipe cached artifact; a `MeshHandle`/`TextureHandle` identifies an uploaded resource in a particular renderer; an `Entity.Value` identifies an authored scene object and supplies a pick ObjectID. These are not interchangeable.

## Follow-up correctness tasks (not fixed during this doc pass)

1. **Upload-to-binding rollback:** `RuntimeRenderBridge::EnsureTexture` and `LoadAssets` may create a GPU handle before the corresponding binding/map/vector bookkeeping completes. Failure injection should confirm the new handle is not stranded.
2. **Noexcept cleanup visibility:** `ReleaseResources` catches and discards renderer destruction errors. Test a submitted scene that still references a destroyed texture and verify the bridge reports/handles failure and retires it.
3. **Importer file size:** `LoadRenderMaterial` reads a registered path into a size-derived vector without an explicit pre-allocation size budget comparable to the generic 1 GiB import source cap. Add a malicious/oversized file test.
4. **Device recreation:** old renderer handles and descriptors are not durable across native device rebuild; rebind from stable asset IDs and reconstruct all native resources.
5. **glTF and selection:** add native GPU reference-image tests for animated skinning, stable ObjectID pixel readback, and generation-aware entity deletion between submission/readback.
6. **Source-level test ≠ execution:** existing CTest names and Catch2 assertions have been read; exact-head C++23 superbuild, native shader validation, GPU driver tests, sanitizers and import stress trials remain separate.

The original 31 historic documentation PRs are tracked separately in [documentation merge reconciliation](DOCUMENTATION_MERGE_RECONCILIATION_20261122.md). None should be called merged based solely on these new direct-to-main commits.
