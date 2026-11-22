# From a saved Scene to a GPU draw: asset identity, import caching, glTF skinning and runtime lifetime

This is a cross-repository **C++23 source-learning laboratory** that follows the actual implementation. It is not a generic engine architecture proposal and does not claim that the GPU was exercised during this documentation pass.

Read these in order:

- [KairoAssets `AssetRegistry.cppm`](https://github.com/swayam8624/KairoAssets/blob/main/AssetRegistry.cppm), [`TextureArtifact.cppm`](https://github.com/swayam8624/KairoAssets/blob/main/TextureArtifact.cppm), [`DerivedDataCache.cppm`](https://github.com/swayam8624/KairoAssets/blob/main/DerivedDataCache.cppm): stable typed IDs, import provenance, owned CPU artifacts
- [KairoEngineCore `RuntimeComponents.cppm`](https://github.com/swayam8624/KairoEngineCore/blob/main/RuntimeComponents.cppm) and [`Scene.cppm`](https://github.com/swayam8624/KairoEngineCore/blob/main/Scene.cppm): authored entity/hierarchy, renderable component, scene instances and visibility
- [`RealtimeSceneBridge.cppm`](../Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm): shared Editor/Player conversion, `RenderAssetBindings`, `BuildRenderScene`, glTF animation and picking IDs
- [`RuntimeRenderBridge.cppm`](../Runtime/KairoPlayer/RuntimeRenderBridge.cppm): the Player actually imports content, asks `RendererRuntime` to upload it, tracks created handles and advances animation
- [KairoRenderer `AssetAdapters.cppm`](https://github.com/swayam8624/KairoRenderer/blob/main/AssetAdapters.cppm), [`VulkanTriangle.cppm`](https://github.com/swayam8624/KairoRenderer/blob/main/VulkanTriangle.cppm) and [texture upload laboratory](https://github.com/swayam8624/KairoRenderer/blob/main/docs/TEXTURE_UPLOAD_AND_RENDER_TARGET_TRANSITIONS_LAB.md): material/primitive conversion and native command recording
- [KairoEditor `SceneRenderBridge.cppm`](https://github.com/swayam8624/KairoEditor/blob/main/SceneRenderBridge.cppm): compatibility aliases, rather than a second scene extractor

## 1. Five identities that must not be mixed

One authored scene can be loaded and displayed through many renderer instances. Each identity exists for a different reason.

| Identity | Example or owner | Valid for |
| --- | --- | --- |
| `AssetID` in `MeshAssetHandle`, `MaterialAssetHandle`, `TextureAssetHandle`, `SceneAssetHandle` | KairoAssets registry, serialized project/scene | Stable authored references across file renames and saves |
| `Entity{Value}` | EngineCore `Scene` | The scene's stable authored object, hierarchy and pick identity |
| `DerivedDataKey` | KairoAssets import pipeline/cache | Content/importer/settings-specific reusable CPU artifact |
| `MeshHandle`, `TextureHandle` | `RendererRuntime` and backend resource map | One process/device instance's uploaded GPU resources |
| `VkImage`, `VkBuffer`, `MTLTexture` | native renderer backend | Real GPU storage with asynchronous lifetime constraints |

A `MeshAssetHandle` is **not** a `MeshHandle`. A `TextureAssetHandle` is **not** a `TextureHandle`. An authored `Entity.Value` may be copied into a draw's `ObjectID` for picking, but it does not grant ownership of a mesh buffer or proof that the picked draw is still active when an asynchronous readback returns.

`RenderAssetBindings` stores a **borrowed reference** to `AssetRegistry`, plus lookup maps from durable `AssetID` to process-local renderer handles and PBR values. This bridge does not allocate or destroy GPU memory. It must not outlive its referenced registry and must be repopulated when the renderer/device is rebuilt.

## 2. A minimal CPU-only authored scene

The following is an illustrative API sketch. It assumes `MeshID` and `MaterialID` are already registered typed asset IDs and all C++ modules are available.

```cpp
import Kairo.EngineCore;
import Kairo.Assets;
import Kairo.Runtime.RealtimeSceneBridge;

// An EngineCore scene owns authored records; no VkDevice or MTLDevice exists here.
kairo::engine::Scene scene;

const auto building = scene.CreateEntity("Building");
// Use durable typed asset identities, not the renderer's runtime mesh handles.
scene.SetMeshRenderer(building, {
    { MeshID },       // MeshAssetHandle: registered persistent geometry
    { MaterialID },   // MaterialAssetHandle: registered persistent material
    true,             // Visible
    {}                // No additional material slots
});
scene.Transform(building).Local.Translation = { 3.0f, 0.0f, 0.0f };

// Elsewhere, after native upload, RenderAssetBindings connects authored IDs
// to the renderer device's actual opaque handles.
kairo::runtime::renderbridge::RenderAssetBindings bindings(registry);
bindings.BindMesh({ MeshID }, rendererMeshHandle);
bindings.BindMaterial({ MaterialID }, material);

// Extract CPU draw commands; native GPU recording happens later.
const auto frame = kairo::runtime::renderbridge::BuildRenderScene(
    scene, bindings);
```

The `MeshRendererComponent::Validate` gate checks nonempty typed mesh and material handles, bounded material slots and a nonzero render-layer mask. `Scene::SetMeshRenderer` replaces the component **only after validation**. The EngineCore test verifies that an invalid replacement does not destroy the previous valid component.

`Scene::RenderableEntities` enumerates ascending Entity IDs and skips hidden renderers or entities disabled by any ancestor. `BuildRenderScene` then intersects each component's `RenderLayers` with the caller's requested mask, resolves the uploaded mesh handle, evaluates `Scene::WorldTransform`, and copies the material into a `MeshDraw`. The resulting `ObjectID` is `Entity.Value` rather than the MeshHandle. No GPU allocation occurs inside this extraction function.

A practical limitation visible in this source: the simple `MeshRenderer` case calls `MaterialForSlot(0)`; it does not automatically expand every additional authored submesh slot into independent draws. Imported `SceneInstance` data is handled through a different primitive-expansion path.

## 3. Importing the same OBJ twice changes cache behavior, not identity

In [`ImportRenderMesh`](../Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm), the bridge looks up the stable `MeshAssetHandle` in the `AssetRegistry`. It requires a source-file origin and the expected `OBJMeshImporter::Identifier()` string. It constructs an `ImportRecord`, calls `ImportSourceAsset(projectRoot,record,importer,registry,imports,cache)`, parses the resulting typed `MeshArtifactData`, and adapts it to a renderer-neutral `Mesh`.

It returns the converted geometry, a `DerivedDataKey` and whether the derived cache was reused. The returned geometry is **CPU-owned**, not yet a `VkBuffer`.

The real [KairoEditor import test](https://github.com/swayam8624/KairoEditor/blob/main/tests/SceneRenderBridgeTests.cpp) writes a three-vertex, one-triangle OBJ into a temporary project, registers its typed metadata, and calls `ImportRenderMesh` twice. The first result reports a cache miss and materializes a key; the second reports a cache hit, an identical key, and equivalent geometry. An unknown importer ID must throw rather than silently accepting incompatible source.

The *persisted* asset identity does not change because the importer recomputed an artifact; the cache key may change when bytes, importer version or settings change, without allocating a new `AssetID`.

## 4. Material import must preserve numeric and texture semantics

[`RuntimeRenderBridge::LoadAssets`](../Runtime/KairoPlayer/RuntimeRenderBridge.cppm) first traverses typed material metadata. For each material artifact it resolves five channel references:

| Channel | Import interpretation | Why |
| --- | --- | --- |
| Base color | sRGB / not normal | Color samples need decoding before lighting |
| Normal | Linear / normal map | Vector components cannot use gamma decoding |
| Metallic-roughness | Linear / data | Numeric roughness/metalness channels |
| Emissive | sRGB / not normal | Encoded emitted-color channels |
| Occlusion | Linear / data | Numeric visibility/multiplicative factor |

`EnsureTexture` remembers which `TextureImportSettings` were used for an authored `AssetID`. If the same source is later referenced with incompatible `ColorSpace` or `NormalMap` semantics, it explicitly throws. This is preferable to reusing a GPU texture with the wrong sampled format.

For one channel, the call chain is:

```text
material's typed TextureAssetHandle
  -> ImportRenderTexture(projectRoot,asset,semanticSettings,...)
  -> ImportSourceAsset [CPU decode, derived recipe/provenance/cache]
  -> ParseTextureDerivedArtifact [validated CPU mip bytes]
  -> RendererRuntime.CreateTexture [native GPU image]
  -> BindTexture(asset,TextureHandle) [process-local lookup only]
  -> MakePBRMaterial(materialArtifact,ResolveTexture)
  -> VulkanMaterialDescriptors.Rebuild [borrow view/sampler, copy PBR UBO]
```

The KairoRenderer converter translates metallic/roughness, color/alpha and other PBR scalar factors, and it resolves each present typed texture. An **absent optional** channel can become an invalid handle and later a neutral fallback; an **explicitly referenced but unresolvable** texture is an error. These are different situations.

`LoadRenderMaterial` reads the registered derived material artifact with a **new pre-allocation limit of 1 MiB**. The size gate checks signed `tellg()` output before constructing the byte vector; the existing derived and material parsers then validate the actual envelope and fixed-size record. See the [material input trust-boundary laboratory](MATERIAL_ARTIFACT_BOUNDARY_AND_FILE_BUDGET_LAB.md) and its GPU-free length regression test. Hostile path containment and full filesystem/parser integration remain separate validation tasks.

## 5. Native mesh uploads are owned by the renderer, not by the binding map

The Player's `RuntimeRenderBridge` is created with borrowed references to `RendererRuntime` and a validated `RuntimeProject`. `LoadAssets` creates actual GPU resources, using the renderer's `CreateTexture` and `CreateMesh` methods.

For each eligible builtin or imported OBJ mesh:

```text
AssetRegistry metadata
  -> MakeBuiltinRenderMesh OR ImportRenderMesh
  -> portable Renderer::Mesh
  -> RendererRuntime.CreateMesh
  -> MeshHandle
  -> RenderAssetBindings.BindMesh(durable AssetID, MeshHandle)
  -> record MeshHandle in m_OwnedMeshes
```

The word “owns” is used at **two levels**: `RuntimeRenderBridge` tracks *which handles it created and should release*, while the selected Vulkan/Metal backend **actually owns native resource memory**. Destroying the binding map alone does not deallocate VkBuffers. Destroying the RendererRuntime before the Player bridge makes its borrowed renderer reference invalid.

**Follow-on repair update:** [GPU resource rollback and renderer teardown laboratory](GPU_RESOURCE_ROLLBACK_AND_RENDERER_TEARDOWN_LAB.md) documents the actual correction to this historical failure path. `RuntimeRenderBridge` now uses `CreateTrackBind` to reserve a journal slot and record each newly created mesh/texture handle **before** binding maps can throw. `ReleaseResources` first submits an empty RenderScene, then destroys journaled handles in reverse order, retaining failures for potential retry and reporting nonzero counts through constructor rollback or the noexcept destructor. Native Metal/D3D12 and OpenGL publication-after-upload error paths were also repaired separately. These are source-level changes plus CPU fault-injection tests; they do not establish native GPU/device-loss recovery.

## 6. SceneInstance: one authored entity can generate many GPU draws

A `SceneInstanceComponent` stores one persistent `SceneAssetHandle`, visibility, shadow policy and layer mask. It does not flatten all glTF nodes into independent EngineCore entities or store GPU handles in the scene.

`RenderAssetBindings::BindGltfScene` requires a validated `GltfSceneArtifactData`, matching counts between render primitives and supplied mesh handles, valid node and primitive indices, membership of each primitive in its node, and matching `SkinIndex`. It stores both render primitive bindings and the original glTF metadata needed for skinning.

The render extraction uses the authored `Scene::WorldTransform(entity)` as the outer placement. For a **static** primitive, the draw model is:

```text
Model_static = SceneInstanceWorld * GltfNodeWorld
```

For a **skinned** primitive, the draw model is instead the outer placement, while the asset-space skin palette encodes joint transforms:

```text
Model_skinned = SceneInstanceWorld
SkinPalette[j] = JointWorld[j] * InverseBind[j]
```

Multiplying a skinned node-world matrix into the draw model as well would apply part of the transformation twice. The same imported glTF can therefore legitimately produce different model matrices for static and skinned primitives.

The [shared bridge test](../Runtime/KairoRealtimeRenderBridge/tests/RealtimeSceneBridgeTests.cpp) constructs a scene instance at world X=3. Its static child node has a +1 local X offset; rest-pose model translation is X=4. An animation moves a joint by +1 at half-time: the static child becomes X=5. The skinned draw model remains at X=3, but its single joint palette matrix now contains +1. These are source assertions, **not outputs of a native GPU skinning test**.

The same asset scene may emit several draw calls, all marked with its authored **Entity.Value** as the pick `ObjectID`. Picking identifies the scene instance, not a particular imported glTF primitive. Per-primitive editing/picking would require another stable identifier model, not a renderer handle masquerading as an authored ID.

## 7. Animation state is runtime-owned, with explicit and implicit modes

The Player maintains a per-entity `RuntimeAnimationPlaybackState` map. Gameplay can call `SetAnimation`, `SetAnimationByName`, pause, or set the clock explicitly. Selecting the currently active clip without restart preserves the clock instead of replaying walk/run at time zero every frame.

`StepAnimations(deltaSeconds)` requires a finite bounded delta, looks up still-existing SceneInstance entities, resolves glTF metadata, advances time by `delta * PlaybackRate`, applies `Loop` or `Clamp`, then publishes `SceneAnimationOverrides` for the shared `BuildRenderScene` function. Nonexistent/deleted entities have their clock/override removed.

Without explicit gameplay control, a scene with animations is assigned a compatibility default of looping clip zero. These clocks are **process-local**, not proven to be serialized in `.kscene` or restored from a saved project.

A `SceneAnimationOverrides` `Find` pointer is borrowed; it becomes invalid when that map is destroyed or mutated. The Renderer Scene builds a fresh copy of the sampled draw/palette data for the next render submission.

## 8. Visibility, hierarchy, lights, camera and environment

EngineCore's `Scene::Entities()` sorts stable entity IDs, rather than allowing `unordered_map` iteration order to decide draw ordering. `Scene::IsActiveInHierarchy` walks the parent chain and disables an entity if **any** ancestor is disabled.

`BuildRenderScene` chooses renderable components and scene instances that are visible and active. It intersects requested render layers; a zero mask is an error rather than a request for an empty or all-visible scene.

Light conversion is centralized in `MakeRenderLight`:

- Directional authored lux intensity is divided by 10,000 to obtain renderer intensity units.
- Local point/spot/area authored candela is divided by 100.
- World-space position/direction comes from EngineCore's hierarchy transform.
- Shadow policy is copied into a renderer-friendly flag.

For an authored point light at 500 candela, the [shared bridge test](../Runtime/KairoRealtimeRenderBridge/tests/RealtimeSceneBridgeTests.cpp) expects renderer intensity 5. An environment with ambient 0.25 and environment-intensity multiplier 2 produces renderer ambient 0.5. A registered environment texture is resolved to the process-local TextureHandle.

The camera selection routine chooses an active primary camera, otherwise the first active camera, otherwise a renderer default; it throws if two active primary cameras are encountered. Its world transform, perspective/orthographic mode, near/far and FOV/size values are translated without a GPU API dependency.

## 9. Editor and Player share conversion, not device ownership

KairoEditor's [`SceneRenderBridge.cppm`](https://github.com/swayam8624/KairoEditor/blob/main/SceneRenderBridge.cppm) re-exports the shared `Kairo.Runtime.RealtimeSceneBridge` API into the Editor namespace for compatibility. It does not duplicate the implementation. This avoids two copies of glTF transform composition, PBR semantic resolution or light intensity conversion drifting apart over time.

Editor and Player still have **different session/ownership lifetimes**: an Editor preview might rebuild assets while the user edits, while `RuntimeRenderBridge` holds a Player project's imported GPU handles for its runtime lifetime. Both produce the same neutral `RenderScene` shape and submit it through a chosen backend. Neither stores native Vulkan or Metal handles in the authored Scene.

## 10. What is already tested, and what is not

The checked-in source defines several relevant CTest/Catch2 units:

| Source | Tested contract |
| --- | --- |
| [`RealtimeSceneBridgeTests.cpp`](../Runtime/KairoRealtimeRenderBridge/tests/RealtimeSceneBridgeTests.cpp) | fake handle binding, authored ObjectID, layers, light/environment factors, camera, static scene expansion, animated glTF pose and skin palettes |
| [KairoEditor `SceneRenderBridgeTests.cpp`](https://github.com/swayam8624/KairoEditor/blob/main/tests/SceneRenderBridgeTests.cpp) | shared extraction from an Editor namespace, invalid/duplicate asset bindings, builtin meshes and first-import/cache-hit OBJ reuse |
| [KairoRenderer `RendererTests.cpp`](https://github.com/swayam8624/KairoRenderer/blob/main/tests/RendererTests.cpp) | PBR conversion, glTF hierarchy and semantic texture resolver, backend-neutral draw contracts |
| [KairoEngineCore `EngineCoreTests.cpp`](https://github.com/swayam8624/KairoEngineCore/blob/main/tests/EngineCoreTests.cpp) | authored component validation, stable Entity IDs, hierarchy and hidden renderable filtering |

The superbuild registers `KairoRealtimeRenderBridgeTests` only when `BUILD_TESTING` is enabled **and** `Catch2::Catch2WithMain` is available. KairoGameEngine's CMake requires sibling Kairo repositories in `KAIRO_WORKSPACE_ROOT`; the source is **not** vendored into the integration repo.

A suitable configured C++23 modules workspace can use:

```bash
cmake -S KairoGameEngine -B build/kairo -G Ninja \
  -DKAIRO_WORKSPACE_ROOT="$(pwd)" \
  -DBUILD_TESTING=ON

cmake --build build/kairo --target KairoRealtimeRenderBridgeTests --parallel 4
ctest --test-dir build/kairo -R '^KairoRealtimeRenderBridgeTests$' --output-on-failure
```

In these example commands `$(pwd)` must refer to the directory containing sibling repos named `KairoGameEngine`, `KairoAssets`, `KairoEngineCore`, `KairoRenderer`, etc. Host toolchains, Catch2, GLFW, and native API SDK requirements may prevent some configurations; configuration success is itself a needed gate.

**Not yet proven by this documentation pass:** GPU upload/destruction under exceptions, rebuilding a backend after device loss, saving/restoring animation clocks, actual native viewport pixels and selection round-trip, or cross-platform numerical skinning parity.

## 11. A concrete future integration test plan

Start with a small authored scene and registered typed assets, then test in increasing scope.

**CPU-only identity and conversion:** write/read a `.kscene` with one mesh, one scene instance, parent transform, camera, light, environment, and a PBR material. Verify stable AssetIDs and Entity IDs after reload; re-run `BuildRenderScene` with fake renderer handles and check the same ObjectIDs and world transforms.

**Importer/cache correctness:** alter one OBJ/source texture byte, importer version or texture semantic setting. Check a new `DerivedDataKey` is produced when appropriate while the stable asset identity remains unchanged. Verify the wrong linear/sRGB reuse cannot be silently bound.

**glTF parity:** animate a joint at fractional clip time; compare static node world matrix and skinned asset-space palette to a CPU reference. Test missing inverse bind, invalid skin indices, source/artifact mismatch and absent clips separately.

**GPU lifecycle:** create and destroy real meshes/textures in both Vulkan and Metal backends; submit one scene, retire its work, clear the submitted render scene, then destroy handles. Inject a failure immediately after CreateTexture but before BindTexture to confirm rollback and no native leak. Restart a renderer and prove an old process-local handle cannot be used as a valid resource in the new device.

**Selection:** emit one static mesh and one glTF instance; render an object-ID buffer, wait for native readback and resolve the picked Entity ID against the still-current Scene. Define how stale picks are handled when an entity is destroyed between submit and readback.

Keep the exact component repo SHAs, compiler, OS, GPU/driver, numeric tolerances and captured outputs with the test record. Source comments and a passing CPU bridge test are **not a substitute for the native GPU lifecycle verification**.
