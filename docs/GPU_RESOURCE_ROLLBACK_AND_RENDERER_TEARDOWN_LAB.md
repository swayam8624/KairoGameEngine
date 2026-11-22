# GPU resource rollback and renderer teardown: exception-safety across four ownership layers

This is the follow-on **correctness pass** after [from a saved scene to a GPU draw](ASSET_SCENE_TO_GPU_LIFETIME_LAB.md). It includes implementation changes and new CPU-only fault-injection tests, not just explanatory comments.

**Scope actually repaired:** Player-owned handle tracking and unbind-before-delete; post-native-allocation C++ map-publication failures in Metal, Direct3D12 and OpenGL. **Not repaired or verified in this particular rollback pass:** native device-loss recreation, general driver errors, asynchronous upload batching or hot-reload. **Later update:** the [Vulkan frame recovery implementation](https://github.com/swayam8624/KairoRenderer/blob/main/docs/VULKAN_FRAME_FAILURE_RECOVERY_LAB.md) has now been added separately; live WSI validation is still outstanding.

Study the production code in this order:

1. [`RuntimeRenderResourceTracking.hpp`](../Runtime/KairoPlayer/RuntimeRenderResourceTracking.hpp) and [`RuntimeRenderBridge.cppm`](../Runtime/KairoPlayer/RuntimeRenderBridge.cppm)
2. [Player fault-injection test](../Runtime/KairoPlayer/tests/RuntimeRenderResourceTrackingTests.cpp)
3. [KairoRenderer `detail/NativeResourcePublication.hpp`](https://github.com/swayam8624/KairoRenderer/blob/main/detail/NativeResourcePublication.hpp) and [renderer fault-injection test](https://github.com/swayam8624/KairoRenderer/blob/main/tests/NativeResourcePublicationTests.cpp)
4. [`MetalRenderer.cppm`](https://github.com/swayam8624/KairoRenderer/blob/main/MetalRenderer.cppm), [`Direct3D12Renderer.cppm`](https://github.com/swayam8624/KairoRenderer/blob/main/Direct3D12Renderer.cppm) and [`OpenGLRenderer.cppm`](https://github.com/swayam8624/KairoRenderer/blob/main/OpenGLRenderer.cppm)
5. [`Renderer.cppm`](https://github.com/swayam8624/KairoRenderer/blob/main/Renderer.cppm) and [`VulkanRenderer.cppm`](https://github.com/swayam8624/KairoRenderer/blob/main/VulkanRenderer.cppm) for the contract that native resources outlive submitted draws

## 1. The original failure: C++ can forget a real GPU allocation

Consider this simplified historical pattern:

```cpp
const auto handle = renderer.CreateTexture(texture); // Native GPU image exists
textureSettings.emplace(assetID, settings);           // Allocation may throw
bindings.BindTexture(assetID, handle);                 // Allocation may throw
ownedTextureHandles.push_back(handle);                 // Allocation may throw
```

If any later insertion throws, no `handle` has reached the cleanup journal. The enclosing constructor catches and runs `ReleaseResources()`, but it can only destroy handles it **knows exist**. A backend image can therefore be live while the Player has lost its identity.

This is a C++ *strong exception-safety* problem, not a shader algorithm or GPU throughput issue. The root is the sequence “acquire external resource, then do potentially throwing bookkeeping.”

## 2. The repaired acquisition sequence

The new [`CreateTrackBind`](../Runtime/KairoPlayer/RuntimeRenderResourceTracking.hpp) helper implements:

```text
owned.reserve(size+1)      // May throw, but native allocation has NOT occurred
handle = createGPU()       // May throw without returning a handle
owned.push_back(handle)    // For uint64_t handles and reserved capacity: no alloc/throw
bind(assetID,handle)       // May throw, but owner journal ALREADY has handle
return handle
```

This is used by both `EnsureTexture` and the ordinary mesh/glTF primitive upload loops in `RuntimeRenderBridge::LoadAssets`. The helper requires no-throw-copyable/move-assignable handles. The actual renderer `MeshHandle` is an unsigned 64-bit integer; the texture handle is likewise a simple renderer value handle.

The associated [CPU regression test](../Runtime/KairoPlayer/tests/RuntimeRenderResourceTrackingTests.cpp) injects a binding exception and checks the resource handle remains in the ownership vector. A separate creation exception must leave the vector unchanged.

A subtle remaining boundary: on binding failure, `CreateTrackBind` leaves the handle tracked **on purpose**. The current use is during `RuntimeRenderBridge` construction, whose outer catch calls `ReleaseResources`. The same algorithm cannot be used as an arbitrary runtime hot-reload transaction without an explicit undo plan for partially inserted bindings.

## 3. Submitted draw references must be removed before destruction

Before this fix, `RuntimeRenderBridge::~RuntimeRenderBridge()` iterated handles and asked `RendererRuntime::DestroyMesh/DestroyTexture` to delete them, but the renderer still had the most recently **submitted RenderScene**. Native backends can and do refuse destruction of referenced resources. In particular, Vulkan, Metal, OpenGL and D3D12 reject destroying a texture currently bound to an active submitted draw or environment.

The destructor suppressed every deletion exception and then cleared its tracking vectors. Even if a backend properly refused an unsafe delete, the Player lost its opportunity to retry and could report a quiet shutdown.

The repaired [`ReleaseSceneResources`](../Runtime/KairoPlayer/RuntimeRenderResourceTracking.hpp) contract is:

```text
if no tracked native resources: return success

renderer.SubmitRenderScene(RenderScene{})   // Drop future draw/material references
   if this throws:
      skip ALL destruction
      KEEP both resource journals
      return nonzero failure count

ReleaseReverse(meshHandles, renderer.DestroyMesh)
ReleaseReverse(textureHandles, renderer.DestroyTexture)
   on success: remove exactly that released handle from its journal
   on failure: retain exactly that handle for potential retry; count failure
```

The renderer's native destruction path is still responsible for GPU retirement: for example, Vulkan waits for device idle before destroying GPU buffer/image owners, and Metal waits on its most recent command buffer when deleting native meshes/textures. **Clearing a CPU scene alone does not establish GPU completion.** It only prevents a *new* scene reference from invalidating a later deletion.

The Player's destructor remains `noexcept`: it reports nonzero cleanup failures to standard error without throwing through destruction. Constructor rollback likewise reports failed cleanup while rethrowing the original load error. If renderer unbinding itself fails, the helper refuses to attempt potentially invalid native destruction. Unreleased native handles remain the responsibility of the enclosing backend's ultimate device destruction; this is diagnostic best effort, not proof of graceful recovery after device loss.

This contract assumes the bridge is the owner of the currently submitted Player scene and the renderer outlives the bridge. An unrelated client must not concurrently reuse the same renderer scene while the bridge closes.

## 4. Why a native backend can leak even when the Player tracks handles

Now go a layer lower. Previously, the Metal and D3D12 adapters had this pattern:

```cpp
const auto handle = nativeBackend.CreateMesh(vertices, indices, skinning);
m_Meshes.emplace(handle, jointCount); // Map allocation can throw
return handle;
```

If `emplace` threw, the native backend still owned the uploaded `MTLBuffer` or D3D12 resource, but the renderer's handle table was not updated. Player-side tracking cannot fix this because the enclosing `CreateMesh` function never successfully returned a handle to its caller.

The new [`CreateAndPublishNativeResource`](https://github.com/swayam8624/KairoRenderer/blob/main/detail/NativeResourcePublication.hpp) helper stages this boundary:

```text
nativeHandle = Create()                   // Native allocation
try:
    Publish(nativeHandle)                 // std::unordered_map/set insertion
catch:
    try Rollback(nativeHandle)            // Call native DestroyMesh/Texture
    if rollback fails:
        throw explicit combined-failure diagnostic
    else:
        rethrow the ORIGINAL map-insertion failure
return nativeHandle
```

`MetalRendererRuntime` and `Direct3D12RendererRuntime` now use this for both mesh and texture publication. The helper is intentionally constrained to a **new, not-yet-submitted resource**. It is not a generic deferred GPU destruction queue and cannot recover from a driver that has lost its device.

The [KairoRenderer CPU-only test](https://github.com/swayam8624/KairoRenderer/blob/main/tests/NativeResourcePublicationTests.cpp) checks four cases: success; a thrown map insertion with successful native rollback; a native creation throw with no published handle; and a double failure that reports the rollback failure instead of silently claiming cleanup succeeded.

## 5. The corresponding OpenGL issue

OpenGL already had explicit catch-and-delete around initial VAO/VBO or texture uploads. But `NextHandle` and `m_Meshes.emplace` / `m_Textures.emplace` occurred **after** that catch. If handle allocation or C++ map insertion then failed, the GL names created earlier were outside the cleanup path.

[`OpenGLRenderer.cppm`](https://github.com/swayam8624/KairoRenderer/blob/main/OpenGLRenderer.cppm) now wraps these final publication steps in their own try/catch. A failure deletes the not-yet-published VAO and vertex/index/skin buffers via `DeleteMesh(gpu)`, or deletes the newly created `GL_TEXTURE_2D` name. These are real native-resource cleanup changes; no OpenGL driver test is claimed from inspecting source.

OpenGL's context is made current before these operations. As with other backends, this does not solve context loss, concurrent rendering, or device-reset policy.

## 6. Why Vulkan's situation is different

[`VulkanTriangle.cppm`](https://github.com/swayam8624/KairoRenderer/blob/main/VulkanTriangle.cppm) constructs `std::unique_ptr<VulkanHostBuffer>` and `std::unique_ptr<VulkanTexture2D>` objects and moves them into its maps. That gives map-insertion failure a different RAII cleanup path from Metal/D3D12's backend-owned numeric handles.

**Later implementation update:** native-buffer and four other Vulkan wrapper constructor-failure cleanups are now committed and explained in [Vulkan constructor recovery](https://github.com/swayam8624/KairoRenderer/blob/main/docs/VULKAN_PARTIAL_CONSTRUCTION_ROLLBACK_LAB.md). The prior reset-on-acquire fence hazard has also received a [source-level WSI frame recovery correction](https://github.com/swayam8624/KairoRenderer/blob/main/docs/VULKAN_FRAME_FAILURE_RECOVERY_LAB.md). Neither has yet been validated under fault-injected native Vulkan drivers.

## 7. Cross-layer lifetime table

| Stage | What can throw? | Owner's protection after this pass |
| --- | --- | --- |
| Asset importer / derived cache | parser, file I/O, settings, cache publish | CPU-only typed artifact transaction in KairoAssets |
| Native CreateMesh/CreateTexture | driver, native allocation, buffer upload | Native create contract; existing backend-specific guards |
| Metal/D3D12: publish handle | C++ map/set allocation | Newly added publish→rollback helper |
| OpenGL: publish GL name | numeric handle exhaustion, C++ map allocation | Newly added catch→DeleteMesh/glDeleteTextures |
| Player: publish AssetID binding | map/vector allocations | reserve journal first; record returned handle before binding |
| Player: release submitted scene | backend validation / device state | clear the submitted RenderScene; on failure skip unsafe cleanup |
| Player: native destroy | driver failure / still-in-flight reference | backend retirement plus reverse release; keep failed handle entries |
| Device loss/recreation | irrecoverable backend state | **Not implemented in this pass** |

## 8. Run the seven CPU-only CTest targets

In a configured sibling-repository C++23 Kairo workspace:

```bash
# Seven portable ownership, Vulkan frame/upload and material input checks.
cmake --build build/kairo --parallel 4 --target \
  KairoPlayerRenderResourceTrackingTests \
  KairoRendererNativeResourcePublicationTests \
  KairoRendererVulkanFrameRecoveryJournalTests \
  KairoRendererTransactionalGenerationTests \
  KairoRendererTextureMipUploadPlanTests \
  KairoRendererVulkanUploadRetirementPolicyTests \
  KairoMaterialFileSizeBudgetTests

ctest --test-dir build/kairo --output-on-failure -R \
  'KairoPlayerRenderResourceTrackingTests|KairoRendererNativeResourcePublicationTests|KairoRendererVulkanFrameRecoveryJournalTests|KairoRendererTransactionalGenerationTests|KairoRendererTextureMipUploadPlanTests|KairoRendererVulkanUploadRetirementPolicyTests|KairoMaterialFileSizeBudgetTests'
```

The **seven** regression binaries are CPU-only: two cover create/bind/destroy ownership callbacks, one covers Vulkan frame state, one fault-injects [transactional descriptor generation](https://github.com/swayam8624/KairoRenderer/blob/main/docs/TRANSACTIONAL_VULKAN_DESCRIPTOR_GENERATIONS_LAB.md), one validates [texture mip staging and offsets](https://github.com/swayam8624/KairoRenderer/blob/main/docs/VULKAN_TEXTURE_MIP_UPLOAD_PLAN_AND_FAILURE_LAB.md), one [classifies queue upload retirement and lost-device cleanup](https://github.com/swayam8624/KairoRenderer/blob/main/docs/VULKAN_UPLOAD_QUEUE_RETIREMENT_AND_DEVICE_LOSS_LAB.md), and one validates material input length. The [standalone direct compiler runner](../scripts/run_renderer_resource_lifetime_regressions.sh) invokes all seven without a GPU or CMake superbuild. They do not require a window or native graphics device. **The Kairo superbuild configuration** may still require Vulkan/GLFW, a C++23 modules compiler and other sibling repositories. A standalone C++23 algorithm smoke test was also compiled locally with GCC 14 and Clang 17, but this does not certify either exact-head repository target.

Before declaring the backend fix production-verified, separately run the native OpenGL, Vulkan, Metal or Direct3D12 smoke tests for the target platform; use fault-injection allocators or targeted native validation for the very rare map-allocation failure branches. Record the repo commit SHA, selected backend, GPU/driver, compiler, fault injected and GPU resource-leak evidence.

## 9. Next engineering tasks, not quietly assumed complete

**Subsequent Vulkan update:** [five wrappers now have explicit partial-construction rollback](https://github.com/swayam8624/KairoRenderer/blob/main/docs/VULKAN_PARTIAL_CONSTRUCTION_ROLLBACK_LAB.md) for command buffers, mapped host buffers, frame semaphores/fences, swapchain views, and depth attachments. Their native failure-injection verification remains outstanding. A [source-level Vulkan frame recovery change](https://github.com/swayam8624/KairoRenderer/blob/main/docs/VULKAN_FRAME_FAILURE_RECOVERY_LAB.md) now resets the fence immediately before a successful queue submission attempt, drains unconsumed acquire semaphores before rebuild, and blocks reuse after recovery failure. Native WSI validation, device-loss reinitialization, asynchronous transfer cleanup and explicit Player shutdown reporting remain outstanding.

A true resource hot-reload needs a generation-tagged renderer-handle mapping or transactional replacement: allocate new native objects, build a complete new binding set and scene, publish together at a safe frame boundary, and retire old generations only after queue completion. Reusing a numeric handle across device rebuilds without generation checks risks stale scene references even if every individual texture allocation succeeds.

**Teaching principle:** a native resource exists before a C++ map insertion finishes; a C++ handle existing does not prove a GPU operation has completed; and releasing a referenced resource requires changing the submitted scene and waiting for in-flight GPU work. The correct ownership proof spans all three boundaries.
