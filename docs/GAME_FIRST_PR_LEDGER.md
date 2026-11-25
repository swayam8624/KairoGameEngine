# Game-first integration ledger

Updated 2026-11-25. This is a review/apply queue, not an accepted build stamp.
The first ten cleanup PRs were independently confirmed merged via GitHub on 2026-11-24. Merge status does not imply combined native acceptance. Do not reset local dirty work.

## Historical apply order and current review queue

| Order | Repository | PR | Branch | Review head | Purpose | Status / merge SHA |
|---|---|---|---|---|---|---|
| 1 | KairoMath | [#4](https://github.com/swayam8624/KairoMath/pull/4) | fix/release-transform-warning | 6dfc0f34a123d191aae5e5110bc686c8ae9d1986 | Release/NDEBUG warning blocking Ubuntu build | Merged `f31f5a2feac8ebb958fef5bfc81a1fca50578e85` |
| 2 | KairoAI | [#6](https://github.com/swayam8624/KairoAI/pull/6) | refactor/gameplay-runtime-split | 35d4d4583986d860e54c10c6eff3f56bee12a471 | Gameplay-only target, assistant optional; CI matcher repair | Merged `5f09694af688a34be157fe3a00171fe7be8bcd00` |
| 3 | KairoEditor | [#21](https://github.com/swayam8624/KairoEditor/pull/21) | refactor/game-first-scene | 1a0b44c55603040a00a48866b95c983a6befe2f7 | Structural editing component preservation | Merged `da2369262e666b8769778ae35681f247573dbec0` |
| 4 | KairoGameEngine | [#42](https://github.com/swayam8624/KairoGameEngine/pull/42) | refactor/game-first | See PR current head (this ledger is part of it) | Core workspace and strict runtime profile | Merged `4b9c847f62dc0d16a914f86ea6dd402f3760b2d5` |
| 5 | KairoGameEngine | [#43](https://github.com/swayam8624/KairoGameEngine/pull/43) | refactor/runtime-resource-journal | 1733cd9921c2c1345b69bc1323499d76055069b6 | Linear resource journal and startup snapshot reuse | Merged `3fcd5cf623cb5c9b7d388046b79fe003d3ef3e2c` |
| 6 | KairoGameEngine | [#44](https://github.com/swayam8624/KairoGameEngine/pull/44) | fix/compute-integration-variant-header | 397c61526e996628b969831db4d9fe3ea677e87d | Optional compute test missing direct standard-library headers | Merged `da01ec78e134728b3c3ee3614332f06b6e30a396` |
| 7 | KairoGameEngine | [#45](https://github.com/swayam8624/KairoGameEngine/pull/45) | refactor/frame-scene-extraction | 63a3d376b3a3e9a0d43835583c49a69041d975d8 | Frame-local hierarchy memoization and shared entity enumeration | Merged `899db9f797a3a705bbff6a0fec813fe313eed96c` |
| 8 | KairoGameEngine | [#46](https://github.com/swayam8624/KairoGameEngine/pull/46) | refactor/gltf-pose-reuse | 59a8ec5b4d72e1afd38ab052ad3811e88fc6d8af | Immutable glTF rest snapshots; one animated hierarchy per instance | Merged `eb8982fd00c4a0c8c21d89aab86db33c0845841c` |
| 9 | KairoONNX | [#5](https://github.com/swayam8624/KairoONNX/pull/5) | fix/smoke-variant-includes | f48d08aa816ca0ccd98c05712c072d03a715c29a | Optional compute smoke test standard-library includes | Merged `572a0b49177584b85d726fe4cfc72a861f6ddd1e` |
| 10 | KairoGameEngine | [#47](https://github.com/swayam8624/KairoGameEngine/pull/47) | refactor/frame-asset-lookups | 3cb950eab1ac56b37dff16e060b404be2b59a52c | Per-extraction typed asset lookup cache | Merged `ae9c6737f134ede885d464fab5206ec47e58e834` |
| 11 | KairoRenderer | [#21](https://github.com/swayam8624/KairoRenderer/pull/21) | fix/vulkan-descriptor-invalidation | 7be12bfeef9a12590a18b309fc41eef477973706 | Vulkan descriptor generation reuse and mesh removal invalidation | Open draft; CPU policy passed |

Math, AI and Editor are sibling dependencies; review and test their branches before the strict engine integration. The #45 -> #46 -> #47 stacked sequence is now merged, and main contains the combined render bridge blob 1271bb9189e3469719d278ebb2a57b7389ef1101. Renderer #21 is an independent sibling PR, not yet merged. Do not merge just because an older run passed. The proposed lock refresh includes only the four already-merged sibling revisions (Math, AI, Editor, ONNX); the renderer pin must be updated separately after review. Historical evidence is not acceptance of this candidate.

Current integration candidate: [Engine #48](https://github.com/swayam8624/KairoGameEngine/pull/48), based on merged main; review the four refreshed pins and test before merging. Renderer #21 can be reviewed in parallel, then its accepted revision needs a separate pin update.

Renderer draw-state follow-up: [Renderer #22](https://github.com/swayam8624/KairoRenderer/pull/22), branch `refactor/vulkan-draw-state`, review head `b36e85de26402af95acfe808b55f69b1bd780079`. Stacked on #21; review and merge #21 first, then retarget/rebase #22. The lock candidate #48 can be reviewed independently; after both renderer PRs are accepted, refresh the renderer pin and rerun combined acceptance. No default branch writes by this pass.

Renderer instancing follow-up: [Renderer #23](https://github.com/swayam8624/KairoRenderer/pull/23), branch `feat/vulkan-opaque-instancing`, review head `c7459053ea7e99f5f2de1d8e8b5568341666e996`. Review after #21 and #22; retarget/rebase in order. Native Vulkan image/picking and shader package checks are required before acceptance.

Next renderer stack: [Renderer #24](https://github.com/swayam8624/KairoRenderer/pull/24), `feat/vulkan-frustum-culling`, head `c39d530f8632cc3ef212a08b3bf8165b3dca65a5`; then [Renderer #25](https://github.com/swayam8624/KairoRenderer/pull/25), `feat/vulkan-shadow-instancing`, head `119b72c39d9a527d48036b71831d9ec5c3d6ea6d`. Integrate #21 -> #22 -> #23 -> #24 -> #25 in order. Each of #23, #24 and #25 includes the same direct vector-header repair for the native smoke; when rebasing, retain that equivalent change only once. Do not advance the renderer lock before native acceptance.

Renderer follow-ups: [#26](https://github.com/swayam8624/KairoRenderer/pull/26) `refactor/vulkan-frustum-support` head `6d6d67eeaa23d02950e4dc30cf0fe9262b814833`, then [#27](https://github.com/swayam8624/KairoRenderer/pull/27) `refactor/vulkan-visible-run-planning` head `5d157eab5ab37df82d6b3777c9e72303001440fb`. Extend the stack #21 -> #22 -> #23 -> #24 -> #25 -> #26 -> #27. Independent [#28](https://github.com/swayam8624/KairoRenderer/pull/28) `fix/msvc-transaction-fault-injection` head `52c072f0e382138db368880a7c5b8b0d3b2b1654` can be reviewed in parallel, then incorporated into the accepted renderer revision. Do not confuse a skipped native smoke with a pass.

Next renderer stack: [#29](https://github.com/swayam8624/KairoRenderer/pull/29) `feat/vulkan-static-visibility-bvh`, head `071bd31d0506ec9de42a0abaf15d35fca12ac577`, then [#30](https://github.com/swayam8624/KairoRenderer/pull/30) `ci/required-vulkan-smoke`, head `3c23c3f3e2d1c1bb1f9b7d4cb602fbfab3aa31e2`. Review after #27; #28 independent. Do not pin the stack before strict native software Vulkan smoke and platform validation.

## Local Codex handoff

The owner reported that local CI patches and source fixes are saved separately, not pushed. Review those patches alongside this queue; if a Math or compute include fix already exists, apply the equivalent change once. Integrate the CI changes without overwriting the new narrow workflows or the game-first default. No access to the local Mac handoff contents is assumed here.

## Evidence observed

- GameEngine #42 prior head d4d4690fdc0dbb8670f3fe989cd1811553b3ea70: game-first scope and migration tests succeeded; macOS Clang, Windows Runtime MSVC and Ubuntu ASan/UBSan jobs succeeded; Ubuntu Clang failed on Math assert-only eps; integrated compute and Wave E failed at std::get of variant; Wave C and racing compile succeeded. Racing macOS integration was still running at observation. These are historical results, not validation of the final combination.
- AI #6 prior head 99c1d65db3ac09cf1a3dcbe055df14cbbf3f75b7: standard Build and Test and assistant ON matrix passed. OFF matrix built/tested but failed the target-name check; narrowed matcher to exact Ninja target names instead of checkout paths. Rerun required.
- Editor #21 head 1a0b44c55603040a00a48866b95c983a6befe2f7: observed editor workflow cancelled. No complete native regression result.
- Engine #43: local independent Clang C++23 ASan/UBSan harness exercised the exact modified header (verified git blob f95dcdbc65245ce767872974feb2d12dc6202bb6): 8,191 exhaustive failure masks for journal lengths 0–12, 65,536 handles with 17 capacity growths, alternating failure/retry and bind/unbind failure cases. Checked-in CTest regression and native integration still need execution.
- Math #4: KairoMath CI at 6dfc0f34a123d191aae5e5110bc686c8ae9d1986 succeeded (run 45429387759); combined engine build still pending.
- AI #6 updated head 35d4d4583986d860e54c10c6eff3f56bee12a471: Gameplay target isolation and Build and Test both succeeded (runs 45429251857 and 45429251802).
- Engine #44: Wave C succeeded; Wave E progressed to KairoONNXSmoke and failed six std::get variant expressions (job 138569950741). ONNX #5 adds the missing direct includes; integration rerun with approved ONNX pin required.
- Engine #43: standalone Runtime resource journal Actions run 45429139703 completed successfully; full integration still pending at observation.
- Engine #45: independent Clang C++23 ASan/UBSan harness against verified exact header blob 92f889024b5a24c1618f5a099828084afa5e4709 passed 102,014 randomized reference comparisons and a 100,000-node reverse-ID chain; active state and world transforms evaluated at most once per node, new cache reflected disabled ancestor. Full module tests and native frame timing pending. O(n) transient cache storage and binary-search lookup are tradeoffs; profile both shallow/sparse and deep scenes. Standalone Editor's pinned bridge revision is unchanged until integration is accepted.

- Engine #45: actual GitHub Actions Frame hierarchy cache run 45432411623 and CPU-only input guards run 45432411050 succeeded at the review head. Full build was still running at observation.
- Engine #46: regression added against existing EngineCore reference for distinct playback times, inverse bind matrices and rest reset. Full module validation pending. Caches per-asset node world matrices and referenced joint palettes; animated snapshots remain instance-local. No GPU timing claim.

- Engine #46: CPU-only input guards succeeded (run 45433360749); full module build queued at observation.
- Engine #47: exact header blob baadeb70e0407cdaad5ff67fff180cee56137ae5 and test blob cbeaf8df893d33e5b9b62c39f248733b136103e5 verified against local files; Clang C++23 ASan/UBSan passed 100,000 adjacent hits with no hashing, 4,096 unique resolutions, rehash, retry, frame refresh. Module regression pending. Additional temporary hash-map allocations and stable-registry-during-extraction assumption require profiling/review.

- Engine #47: actual Frame extraction caches workflow 45434709306 and CPU-only guards 45434709338 succeeded at exact review head. Logged 102,014 hierarchy comparisons and 100,000 adjacent hits with zero hashing.
- ONNX #5: Compute Component CI 45434417088 succeeded. Combined Wave E must be rerun against the approved ONNX pin.
- Renderer #21: standalone Vulkan descriptor invalidation sanitizer workflow 45440041611 succeeded; full renderer CI queued. Exact shader-visible comparison avoids unnecessary native generation replacement and fixes stale indices after mesh removal. Native Vulkan validation and timing pending.

- Renderer #22: pass-local pipeline and vertex/index buffer binding elision in shadow and main passes. Draw order, per-draw descriptors, push constants and transparent sorting retained. Added CTest and standalone sanitizer replay covering 100,000 repeated draws, 256,000 randomized draws, pass boundaries and failure retry. Exact committed header and test blob hashes matched local copies (e7a6c04ecfa21323d82894db2a5b5f97156f3ff3 and 87598b1113bf13e1723cc9e4286c339b35cdc2d3); local Clang C++23 ASan/UBSan replay passed. Native Vulkan smoke and timings pending; draw-call count unchanged.

- Renderer #22: GitHub Actions Vulkan draw binding state run 45440832992 and descriptor invalidation run 45440833086 succeeded. Full renderer CI cancelled; no native acceptance.
- Renderer #23: actual opaque instance-rate vertex stream and multi-instance indexed draws for contiguous static same-mesh/same-material runs. Existing transparent/masked/skinned and shadow paths unchanged. Per-instance model/normal matrices and uint IDs/receive-shadow flags. CPU planner, shader compile/link validation and native image/picking reference smoke added. Run 45444357600 passed planner sanitizers (100,000 compatible draws -> 1 instance run; 256,000 randomized draws), but strict GLSL link rejected the pre-existing mismatched varying names. Aligned varying names at unchanged locations across ordinary, skinned and instanced shaders; rerun pending. Descriptor allocation per draw remains. Verify packaging of instanced SPIR-V and native fence/resize behavior before shipping.

- Renderer #23: corrected ordinary/skinned/instanced GLSL link and SPIR-V validation succeeded (45444564191); full Ubuntu build subsequently failed solely in the new smoke source at vector comparison (job 138624651554). Direct `<vector>` include added to all three stacked branches; fresh build required. Full macOS job cancelled.
- Renderer #24: frustum bounds sanitizer workflow 45445212128 passed 800,000 sampled clip-space checks, 19,815 rejected boxes, boundary/reflection/eye-crossing and fail-open cases. This is CPU policy evidence; not a proof of native output equivalence or a timing gain. Static meshes only; no camera filtering of shadow casters, and original descriptor indices preserved.
- Renderer #25: separate shadow run plan and instanced depth shader; camera-offscreen casters remain included. Main/shadow instance records share a fence-retired upload buffer and staging capacity. Added planner checks and native comparison with offscreen casters enabled/disabled. Fresh shader and native tests pending at commit; no GPU timing claims.

- Renderer #25 exact head 119b72c39d9a527d48036b71831d9ec5c3d6ea6d: frustum, opaque/shadow shader contract, descriptor and draw policy workflows passed (45445604893, 45445604646, 45445604620, 45445604463). Full Ubuntu build and CTest job 138628389832 succeeded, with two Vulkan smokes explicitly skipped (12 of 14 tests executed successfully). Windows job 138628389512 failed at the pre-existing transactional fault-injection helper's C4702 warning-as-error; macOS job cancelled. #28 proposes a targeted test-only repair; MSVC rerun pending.
- Renderer #26: frustum sanitizer run 45447003172 passed 800,000 sampled points; all 19,815 reference rejections matched prepared support. Frame-prepared camera product and AABB plane support reduce repeated arithmetic; extreme inputs fail open. No measured native timing and no BVH yet.
- Renderer #27: compact surviving main/shadow index streams preserve original descriptor indices and stable order; invisible and noncasting gaps no longer fragment compatible batches. Added sparse 100,000-draw and randomized regressions; exact-head opaque/shadow instancing contract 45447141445, frustum 45447140728, descriptor 45447141411 and draw-state 45447140909 all passed. Full renderer CI 45447140962 still running at observation. No native performance claim.

- Renderer #27 full Ubuntu build/CTest succeeded (job 138633921470); Windows failed and macOS cancelled. Native Vulkan presentation skips remain unaccepted.
- Renderer #29: stable-geometry median BVH and conservative affine world bounds; individual fallback for moving, skinned, unsafe and projective geometry. Static BVH sanitizer workflow 45448968574 succeeded. CPU test uses 65,536 interleaved clustered objects and 320,000 world-space samples. Camera/material changes reuse hierarchy; exact model/mesh change or mesh removal invalidates. O(n) scene comparison and visibility output remain, and build/storage cost must be profiled.
- Renderer #30: required Lavapipe/Xvfb rendering smoke workflow executes native test binaries directly; exit 77 and validation errors fail. Added 259-object scene image equivalence and camera/model change picking checks. Actual execution pending; no hardware timing claim.

## November 25 integration refresh

GitHub confirms Renderer #21–30 (including independent #28) and Engine #48 merged. Together with the first ten PRs, all 21 previous PRs are merged. The strict Vulkan software-driver gate 45449116912 passed on review head 3c23c3f3e2d1c1bb1f9b7d4cb602fbfab3aa31e2: actual skinning and instancing/reference image, picking, offscreen shadow, large-scene camera/object motion tests executed under llvmpipe, with no detected validation errors. This is not physical-GPU performance evidence. Proposed renderer lock d7ec83fbf6b3a7fbbcc9d334904833a84527ba0e is the merged #30 commit; exact combined engine build still required.

New independent Metal stack: [Renderer #31](https://github.com/swayam8624/KairoRenderer/pull/31), fix/metal-large-skin-palettes, head ac619cbb322e06caa97c729a6a1f71dbcfc03c30. Replaces inline skin bytes with a retained frame MTLBuffer, aligned offsets and shared shadow/main data. Host-neutral sanitizer workflow 45490490646 passed 100,000 range checks; Apple native 255-joint smoke pending. [Renderer #32](https://github.com/swayam8624/KairoRenderer/pull/32), refactor/metal-encoder-state, head ec431a02973dbce084c74750571085b4a360688a, stacked on #31. Pass-local pipeline, geometry, depth, cull and texture/sampler state reuse; no draw reordering or draw-count reduction. Native Apple validation pending. Neither pending Metal branch is pinned by this lock candidate.

### Metal follow-ups (2026-11-25)

[Renderer #33](https://github.com/swayam8624/KairoRenderer/pull/33), feat/metal-opaque-instancing, head a327d11a2071d1baf863f1c0a5913ba45b58e6c3, stacked on #32. Opaque static instances use retained immutable MTLBuffer, aligned run offsets, native instanceCount and independent model/normal matrices and flat integer picking/receive-shadow varyings. Exact material/texture/cull compatibility; masked, blended, skinned and shadow draws retain original paths. CPU compatibility sanitizer 45551905990 passed 100,000 instances and fallback boundaries; native image/picking test not executed on Apple hardware.

[Renderer #34](https://github.com/swayam8624/KairoRenderer/pull/34), fix/metal-large-debug-stream, head 50a7de861bfd2c63961a945a78813d551b45e844, stacked on #33. Debug vertices use a retained buffer instead of oversized inline bytes; debug pipeline matches the ID attachment but disables its writes, with explicit depth-test/no-depth-write state. Added 256-line visual and ID-preservation test, and strict local Mac gate `bash scripts/require_metal_smoke.sh <configured-build-dir>`. Native validation pending.

#31 and #32 remain open. On #32 Ubuntu Clang and Windows MSVC passed (jobs 138787522652 and 138787523066), but macOS LLVM was cancelled (138787523096); strict Vulkan workflow 45490586110 passed. These results are not Metal native acceptance. Review #31 -> #32 -> #33 -> #34, then refresh renderer pin after the accepted revision is known. Engine #49 remains separate and pins the already-merged Vulkan revision only.

### Metal visibility and procedural world follow-ups (2026-11-25)

[Renderer #35](https://github.com/swayam8624/KairoRenderer/pull/35), feat/metal-camera-visibility, head 9188b697f47e452bc08a264e9090200c71b50729, stacked on #34. Mesh metadata includes local bounds; prepared zero-to-one clip-space frustum uses the actual Metal Y-flipped projection. Visible draw list is compacted, while skin and uncertain geometry fail open and shadow casters are not camera-filtered. Native camera move/image/picking smoke added; Apple execution pending.

[Renderer #36](https://github.com/swayam8624/KairoRenderer/pull/36), feat/metal-shadow-instancing, head a6b66559a6cac91bf3750eea31d597b4123c7e40, stacked on #35. Separate caster index stream and model-only 64-byte GPU instance stream, ordinary masked/skinned fallback retained. Exact-head Metal instance contract 45552413301 passed 100,000 instances and 33,334 independent shadow casters. Skin upload contract 45552413180 passed. Native Apple shadow comparison adapted from the passing Vulkan fixture remains unexecuted. Strict Vulkan and full renderer CI still running at observation. Earlier #34 strict Vulkan 45552041013 passed; its macOS job cancelled. Review #31 -> #32 -> #33 -> #34 -> #35 -> #36. Native Mac gate remains required.

Independent [EngineCore #26](https://github.com/swayam8624/KairoEngineCore/pull/26), feat/deterministic-world-heightcells, head f1824737fdb1a8a011c0decc510911e2090a7230. Adds version-1 integer-only coordinate seed and heightfield cell API to the existing WorldStreaming module; shared borders, negative coordinate handling, golden hashes, bounds/overflow tests and public module test. Not yet connected to cell loading, mesh creation, colliders, persistence or voxel generation. Host-neutral seed contract run 45552575663 passed at prior head 9665d529d5b76bcd49c7b6ca6821ebfadbe21f23 (49,152 shared border samples); current head adds only public module test, exact-head rerun and full module build pending. Review independently; no EngineCore or Metal pin advancement in Engine #49.

### Procedural terrain geometry and surface queries (2026-11-25)

EngineCore #26 exact-head seed contract 45552616502 passed; full CI 45552616358 was cancelled. [EngineCore #27](https://github.com/swayam8624/KairoEngineCore/pull/27), feat/seeded-terrain-mesh, head bad4645587c0aa584d337bbfa51f793d95c36b53, stacked on #26: canonical validated MeshArtifactData, cell-local meter vertices, upward triangle winding, tile UVs and global one-sample halo for central-difference normals. Double-precision cell origin is separate from float geometry. CPU sanitizer run 45553266969 passed 12,288 shared height/normal samples, 369,024 upward triangles and exact generator-height mapping; seed contract 45553267043 passed. Full module build 45553267105 pending at observation; macOS cancelled. Renderer can consume existing Mesh::FromArtifact, but no runtime upload/ownership/streaming integration yet.

[EngineCore #28](https://github.com/swayam8624/KairoEngineCore/pull/28), feat/seeded-terrain-surface-query, head 0b81f27bd0fdb1fc4b1f2346233097b604b19f06, stacked on #27: allocation-free vertical height and geometric face-normal query using the same fixed triangulation. Nominal double-precision surface differs slightly from quantized float vertices. Tests 30,752 randomized triangle queries and 1,001 boundary checks; Exact-head surface CI 45553390807 passed; mesh 45553390585 and seed 45553390314 also passed. Full module CI 45553390304 still running. Current authored ColliderShape and RuntimePhysicsBridge only expose box/sphere/capsule; this query is not a collision shape, swept contact test or automatic grounding. Review EngineCore #26 -> #27 -> #28 independently of the Metal stack; no pin changes in Engine #49. Next runtime work must integrate the existing cell lifecycle and ownership with procedural mesh loading rather than create a parallel streaming runtime.

### Procedural discovery and CPU terrain residency (2026-11-25)

[EngineCore #29](https://github.com/swayam8624/KairoEngineCore/pull/29), feat/procedural-cell-discovery, head 303b9a1c853bd8588f8747f38d55e154f1103133, stacked on #28. Bounded multi-observer discovery uses the existing closed-AABB load-distance rule, deterministic union, signed coordinate clipping and scan-budget rejection before registry mutation. ProceduralWorldStreamingRuntime creates version/config/seed/coordinate content keys and prunes only unloaded cells outside demand; keeps in-flight and resident cells until completion. Existing planner publishes reservations after allocating request construction finishes. Discovery sanitizer 45559574923 passed 2,178,000 brute-force comparisons, closed borders, observer order and scan cap. Added module travel and pending-load tests; full module CI 45559574906 pending at observation.

[EngineCore #30](https://github.com/swayam8624/KairoEngineCore/pull/30), feat/procedural-terrain-residency, head ab7aac6b4df24d8bcdaf5aa8508c9ada296bef2e, stacked on #29. ProceduralTerrainRuntime owns canonical CPU mesh artifacts, executes planned unloads/loads synchronously and acknowledges each request even after a mesh generation failure. Returns failure counts and first exception_ptr, retains no successful unloaded mesh. Added standalone ownership regression (20,001 completions, 2,856 injected failures) and module-level 200-teleport deterministic revisit test. Exact-head ownership CI 45559800177 passed; discovery 45559800290, seed 45559800248, mesh 45559800298 and surface 45559800257 all passed. Full module 45559800286 still running at observation. CPU estimated mesh payload is not total RAM or GPU bytes; temporary generation cost and GPU/physics ownership are not implemented. Review EngineCore #26 -> #27 -> #28 -> #29 -> #30; no new dependency pins in Engine #49.

### Procedural terrain native viewport (2026-11-25)

[Engine #50](https://github.com/swayam8624/KairoGameEngine/pull/50), feat/procedural-terrain-viewport, implementation head 4b1fb03927e6165bb1f5722494d710d72379d495, stacked after Engine #49 and dependent on EngineCore #26–30. Adds opt-in RuntimeProceduralTerrainBridge and KairoTerrainDemo. Native mesh ownership journal preallocates entries before uploads, detaches previous terrain draws before destruction, retains failed deletions, settles failed detach requests, and provides terminal retryable shutdown. Direct generation avoids keeping a duplicate CPU resident mesh set. Authored scene lights/draws preserved; terrain ID zero is nonpickable. Explicit caller render origin is not full floating-origin orchestration. Standalone failure-injection test and required native Vulkan fly-through CI use exact EngineCore candidate ab7aac6b4df24d8bcdaf5aa8508c9ada296bef2e in the disposable runner, with normal release lock unchanged. Host-neutral ownership job 139031699342 on run 45563228501 passed 10,000 uploads, detach rollback and 2,857 failed retirements with retry. Subsequent demo-only commit strengthens capture comparison against the cleared authored scene; native compilation/execution still pending. Full EngineCore #30 CI was cancelled, all five standalone contracts passed. Review EngineCore #26–30 and Engine #49 before #50. Do not enable the demo by default or advance lock to unaccepted candidate. See docs/PROCEDURAL_TERRAIN_VIEWPORT.md.

### Bounded background procedural terrain (2026-11-25)

[Engine #51](https://github.com/swayam8624/KairoGameEngine/pull/51), feat/procedural-terrain-background-jobs, implementation head ad9d9679ef24a9824d896a2f67d3d399a6e5d216; CI syntax fix 510fc235d3017153387a8199015c8a7d77e7a64c, stacked on #50. Optional `--async` uses a single CPU worker, fixed queued/running/ready slots, fair round-robin work and completion selection, exception_ptr propagation and joined shutdown. Renderer API remains render-thread-only. Planner load reservations count pending work; full queue defers and settles unused reservations, obsolete ready work outside keep radius is discarded, and native completion attempts per update are capped. Demo defaults to eight pending meshes/four completion attempts, tracks deferred/stale/peak pending counts, and adds a three-region convergence-based smoke with image comparison and bounded timeout. Synchronous reference path remains. Standalone test stresses 10,000 jobs, 1,428 injected failures, capacity eight, completion identities and joined shutdown. Local Clang C++23 ASan/UBSan executed successfully using exact Git blob hashes de59ca5e8c0bd587d4298edb21b9c927c0356949 and a3da88b2794cbd5f5725703412a7e8460e8e0a57 (10,000 jobs, 1,428 injected failures). Full native CI pending. Native #50 gate 45563348836 still running at observation. No accepted dependency lock changes. No claim of measured frame-time improvement, byte-accurate memory budget, multiworker scheduling or mid-generation cancellation. Review EngineCore #26–30, Engine #49, #50, then #51.

### Native terrain acceptance and parallel generation (2026-11-25)

Engine #49 full Build, Test, and Package workflow 45559904482 passed. #50 native terrain run 45563348836 passed (66 uploads, 44 unloads, three captures, successful shutdown). #51 native run 45565248560 passed both synchronous and asynchronous modes; async had 30 deferred requests and peak eight pending meshes; both modes produced 66 uploads, 44 unloads and three captures. Ownership and 10,000-job single-worker sanitizer tests passed on the same workflow. This is software Vulkan correctness, not physical GPU performance. Latest #51 native run 45565322052 also passed.

[Engine #52](https://github.com/swayam8624/KairoGameEngine/pull/52), fix/racing-renderer-test-targets, head 4012922b8e92e56d02f145f7fd7d5e0b106dc147, independent after #49. Racing compile job 139038588583 failed because six registered renderer test executables were not built. Added the six explicit targets without reducing CTest selection. Rerun pending; no assertion failure established.

[Engine #53](https://github.com/swayam8624/KairoGameEngine/pull/53), feat/procedural-terrain-worker-pool, implementation head a8dce793963cd7f19087ca4c55b827a5439035b1, stacked on #51. Configurable bounded worker count, rollback of partial worker creation, joined teardown, cancellation of queued obsolete work with planner acknowledgement, retained running-slot ownership. Added 10,000-job four-worker stress and explicit barrier-controlled overlap/cancellation test; third native regression uses --workers 4. New pool CI pending. Existing distance-based planner order is unchanged; no dynamic reprioritization, mid-generation interruption, measured speedup or GPU retirement optimization. Review #49 then #52 independently; EngineCore #26–30 before #50, #51 and #53. No release lock changes.

### Fence-based mesh retirement and large-world rendering (2026-11-25)

Engine #53 four-worker CPU ownership job 139057043829 passed: 10,000 jobs, 1,428 injected failures, capacity eight, four simultaneous workers, four queued cancellations and eight parallel completions. Native run 45570914115 and racing repair 45570859914 were still running at observation.

[Renderer #37](https://github.com/swayam8624/KairoRenderer/pull/37), perf/vulkan-fence-mesh-retirement, head 1dfdf827b005caa435ff73df751d279739b0145c, independent on merged Vulkan main. Mesh handles/draws become invalid immediately; native mesh buffers move to preallocated 256-entry retirement journal, released at successful existing frame fence or proven idle/lost fallback. Full journal forces idle, so this is deferred retirement, not an unconditional nonblocking guarantee. Native regression tests in-flight deletion, stale handles, surviving picks and capacity fallback. Host-neutral 102,400 deferred releases and 400 overflow checks; CI 45571522708 and strict Vulkan 45571522766 queued. No physical speedup claim, texture/Metal retirement or byte cap. Renderer lifetime operations remain render-thread operations outside frame recording.

[Engine #54](https://github.com/swayam8624/KairoGameEngine/pull/54), feat/procedural-terrain-floating-origin, implementation head 9da6bc5338c6ce3c8ff561b1d0aa07344eb6809d, stacked on #53. Cell-aligned double origin, subtract-before-float conversion shared by terrain and camera, demo --far-origin at (+1e9,-1e9) meters, same-world rebase image comparison (mean absolute RGBA byte error <=0.1) and unchanged residency requirement. CPU contract 100,000 large-coordinate rebases with eighth-meter local offsets and range rejection. Native/CPU run 45571755452 queued. This is terrain/demo rendering only; no authored-scene/physics/networking rebase. Review #49 -> #50 -> #51 -> #53 -> #54 after EngineCore #26–30; #52 after #49 independently; #37 independent of Metal #31–36. Release dependency lock unchanged.

### Merged terrain and renderer integration (2026-11-25)

All 39 previous campaign PRs have been verified merged, including Renderer #31–37, EngineCore #26–30 and Engine #49–54. The earlier review-queue entries above are historical. EngineCore merged head 7da2be438e84fa51b04057dc8cee780d478db74c and Renderer merged head 392c6758a23393c7da919b3944b428e25988435e are proposed as exact workspace pins. Renderer #37 final review head differs from the originally observed 1dfdf827; that earlier head's strict Vulkan run 45571522766 and ownership run 45571522708 passed. Engine #53 native three-mode terrain run 45570914115 passed (66 uploads, 44 unloads, three captures in each mode). Racing repair 45570859914 passed. Engine #54 CPU origin job 139059846022 passed 100,000 large-coordinate rebases; native large-world acceptance 45571755452 remains pending at this observation. New lock CI removes the temporary procedural candidate override; all tests must exercise exact merged pins. Native Metal still requires Apple acceptance. Do not infer physical GPU performance or whole-engine correctness from the software Vulkan runs.

[EngineCore #31](https://github.com/swayam8624/KairoEngineCore/pull/31) adds sparse, full-resolution-border terrain LOD meshes. Independent of this lock change: the proposed EngineCore pin intentionally excludes this unmerged PR. CPU topology CI 45574085470 queued at creation. LOD selection and transactional native mesh replacement remain next steps.

### Terrain LOD policy (2026-11-25)

[EngineCore #32](https://github.com/swayam8624/KairoEngineCore/pull/32), feat/terrain-lod-selection, head 9f067b204bf42010412f7f9318cd26985ee7618b, stacked on #31. Opt-in multi-observer LOD selector using horizontal closed-AABB distance divided by observer radius scale, power-of-two strides, strictly increasing switch distances and separate inward/outward hysteresis. Tests 20,000 threshold oscillations and 10,000 multi-observer comparisons; CI pending. No live mesh replacement or physical surface changes. Engine #55 exact merged lock acceptance run 45574412491 queued. Review #31 before #32; #55 is independent. Total campaign 42 PRs, 39 verified merged, three pending at observation.

### Live terrain LOD replacement (2026-11-25)

[Engine #56](https://github.com/swayam8624/KairoGameEngine/pull/56), feat/terrain-live-lod, implementation head 30cdcbc7a0fb3c2e4816ba4ce9cc785c0c564614, stacked on #55 and requiring EngineCore #31–32. Optional --lod adds initial stride selection and asynchronous resident mesh replacement using shared bounded worker slots. New loads get admission first, replacement jobs tracked separately from initial planner reservations; stale results are rejected before upload. Native store preallocates a 32-handle old-resource journal; failed uploads preserve old mesh; old handles remain owned until successful scene submission excludes them; failed retirements are retained for retry. Shutdown includes both active and retired handles. Local Clang C++23 ASan/UBSan execution of exact Git blobs b732b8d870b637614013f7b7bb13a7da7fde1a85 and 45328cffaf056af2bee5c5180b335ec37c9e7c04 passed existing 10,000 upload stress and 10,000 LOD replacements, failed upload, full journal and retirement retry. Native mixed-LOD smoke changes observer scale and requires executed resident replacements, bounded ownership and a nonblank capture. CI 45576274036 queued. Exact candidate EngineCore 9f067b204bf42010412f7f9318cd26985ee7618b used only in disposable native CI; BEFORE accepting #56, pin the actually merged LOD EngineCore revision and remove the temporary override. No geomorphing, physical collision approximation changes, measured GPU speedup or byte-accurate transient memory cap.

Earlier billion-meter native terrain run 45571755452 completed successfully, including rebased image comparison. Total campaign 43 PRs, 39 verified merged, four pending review. #55 independent; #31 -> #32; #56 after both stacks, with final accepted lock gate.

### Exact nominal LOD error measurement (2026-11-25)

EngineCore #31 exact-head LOD contract 45574355773 passed: 259,200 triangle checks and 57,600 invariant perimeter edges; stride/vertices/triangles = 1/1089/2048, 2/413/696, 4/205/280, 8/149/168, 16/133/136, 32/129/128. EngineCore #32 exact-head LOD selection contract 45574610031 passed (20,000 threshold oscillations, 10,000 multi-observer comparisons); earlier standalone procedural contracts also passed. Full module CI is still queued. Engine #55 native combined lock CI 45574669707 running, ownership passed; #56 native CI 45576499233 queued.

[EngineCore #33](https://github.com/swayam8624/KairoEngineCore/pull/33), feat/terrain-lod-error, head fee0bfa6b74d6847826e678333c1a941a004c989, stacked on #32. Exact nominal vertical error (rounded up to integer mm) from all vertices of the piecewise-affine triangle overlay, including fractional edge crossings. Integer grid coordinates, signed int32 height samples, int64 rational arithmetic; scanline raster bounds avoid empty triangle bounding boxes. ChooseSeededTerrainLodUnderError checks candidate strides from coarsest to finest without assuming monotonic error. Tests include synthetic 100/3 mm crossing (ceil 34), dense independently evaluated triangle samples, signed extreme heights, flat cell and 1024-interval grid. Error contract CI 45580228833 queued; implementation not yet accepted. This is a nominal vertical surface bound only, not screen-space error, float quantization, shading error or physical collision. Not wired into live renderer; expensive measurement should run in background or preprocessing with caching. Review #31 -> #32 -> #33; #56 depends on #31–32 and #55, not #33. Total 44 campaign PRs, 39 verified merged, five pending. No default branch modifications.

### Error-aware live terrain and Apple compile repair (2026-11-25)

EngineCore #33 exact-head nominal geometric-error CI 45580228833 passed 2,114,112 dense triangle samples, signed fractional crossings, signed extreme heights, and 1024-interval case. Example seed 42, cell (0,0), 32 intervals, lattice spacing 7, height range [-4000,12000] mm: stride/error(mm) 1/0, 2/1471, 4/2207, 8/6420, 16/7722, 32/8334. All eight standalone contracts passed; full module CI cancelled, not accepted. Engine #55 exact-lock native terrain CI 45574669707 passed. Engine #56 native CI 45580302665 passed 20 actual resident LOD replacements, three-region streaming, far-origin image comparison and clean shutdown. Separate macOS Racing integration job 139088372079 failed compiling MetalBackend.mm lines 732 and 860 with -Wtautological-constant-out-of-range-compare.

[Renderer #38](https://github.com/swayam8624/KairoRenderer/pull/38), fix/metal-instance-size-arithmetic, head f81208c6bbecd3068b9a944b3d69d841efcd3eb3, fixes both Metal overflow comparisons by reversing the inequality to stride > SIZE_MAX / nonzero count; no warning suppression or allocation-layout change. Metal instance/skin host-neutral contracts 45585186540 and 45585186548 passed. macOS LLVM job 139104861077 cancelled; Apple recompilation and native smoke remain outstanding. Pin actually merged fix before rerunning combined Apple acceptance.

[Engine #57](https://github.com/swayam8624/KairoGameEngine/pull/57), feat/terrain-error-aware-lod, implementation head 408dca78150a9faace2c39e7f2bdfccf63121a5e, stacked on #56 and dependent on EngineCore #33. Optional --error-mm N generates fixed per-cell profiles of supported strides on initial CPU worker loads, chooses coarsest admissible detail within distance demand, retains profile with native resident mesh and reuses it during replacements. No assumption of monotonic error; profiles discarded on unload. Native smoke checks resident profile coverage and maximum nominal error <=1000 mm. Ownership sanitizer job 139105873130 passed 10,000 profiles and 110,000 exhaustive-oracle cached selections, plus existing LOD replacement and four-worker tests. Native CI 45585486775 running. Temporary exact review candidate override points to EngineCore #33 head; pin the final merged dependency and remove override before release. Not screen-space error, geomorphing, persistent cache or physical hardware performance. Total 46 campaign PRs, 39 verified merged, seven pending at observation. Review Renderer #38 independently; EngineCore #31 -> #32 -> #33; Engine #55 -> #56 -> #57, then final merged-lock gate.

### Perspective terrain LOD and first playable terrain exploration (2026-11-25)

Engine #57 native terrain run 45585486775 passed: error ceiling 1000 mm, maximum resident measured nominal error 973 mm, 20 resident replacements, 66 uploads, 44 unloads, three captures and clean shutdown. Engine #55 combined build 45574669574 passed Ubuntu Clang, Ubuntu ASan/UBSan, Windows MSVC and compute stack, but macOS failed on the two known Metal instance overflow comparisons. Renderer #38 strict Vulkan 45585186584 passed; full renderer CI cancelled, so the Apple fix is not yet validated.

[Engine #58](https://github.com/swayam8624/KairoGameEngine/pull/58), feat/terrain-screen-error, implementation head 68e01dc99e547038b24e5fb82e71a9976cd750e3, stacked on #57. Perspective displacement estimate uses camera basis, focal length in pixels, minimum cell depth, maximum radial camera-space extent and cached nominal vertical error. Near-plane/behind-camera cells fail closed to full detail; initial loads use full detail and then schedule admissible replacements with 20% coarsening headroom. CPU ownership job 139114329517 passed 1,000,000 sampled point pairs, near-plane fallback and billion-meter translation. Native CI 45587962382 running; targets 20 px at settled camera. Transient asynchronous budget violations are possible; not a certified FP, visibility, rasterization or shading bound.

[Engine #59](https://github.com/swayam8624/KairoGameEngine/pull/59), feat/terrain-explorer, implementation head de88c12fcfed1b26b3dc172e2ec2eb571deed45b, stacked on #58. Adds separate KairoTerrainExplorer executable (opt-in terrain build), WASD, arrow look, left Shift sprint and Escape. Uses absolute double point-grounded movement, bounded substeps, normalized diagonal speed, canonical surface samples, full-detail residency check and sampled grade limit. Distant terrain uses existing streaming/LOD. Native smoke requests 300 movement updates and three nonblank captures across streamed terrain, requires loads/unloads and shutdown. CPU test covers 10,000 billion-meter steps, missing cells and grade rejection. CI 45588373182 queued at observation. This is a controllable exploration slice, not capsule physics, continuous collision, jumping, or a finished game. Next game-first acceptance: native run, physical character grounding and interaction, then seeded voxel building.

Total campaign 48 PRs, 39 previously verified merged, nine pending at observation. Review Renderer #38 independently; Core #31 -> #32 -> #33; Engine #55 -> #56 -> #57 -> #58 -> #59. Final acceptance requires actual merged dependency pins and removing the temporary EngineCore candidate override; rerun native Metal and cross-platform tests. No direct default-branch mutations.

### Explorer jump and Craft foundations (2026-11-25)

Engine #58 native Vulkan run 45587962382 passed: 20 px projected error target, maximum settled estimate 13.6299 px, 20 resident replacements. Engine #59 CPU walking contract passed (job 139115744847); native explorer runs 45588373182 and 45588550016 still in progress at observation.

[Engine #60](https://github.com/swayam8624/KairoGameEngine/pull/60), feat/terrain-explorer-jump, implementation head a933ddbd47b158e217e35066996ff3b6592c6f61, stacked on #59. Adds Space edge-triggered ballistic point jumping, gravity in spatial/temporal substeps, sampled canonical terrain landing, no double jump, missing cells and tall step barriers. CPU ASan/UBSan job 139123842345 passed, apex 1.2742 m, 10,000 grounded large-world steps and landing/barrier tests. Native jump then travel smoke queued in run 45590694819. Not capsule collision or exact swept collision.

[Engine #61](https://github.com/swayam8624/KairoGameEngine/pull/61), feat/voxel-world-interaction, implementation head fbad4d34c6d3b4b916f2ceb5b11c710a02c3470d, stacked on #60. Bounded baseline-relative voxel edit map (air ID 0), reversion reclaims space; signed-grid DDA returns first solid and unique face-adjacent air placement. Unknown cell stops traversal. Exact computed ties advance all axes; no arbitrary-precision tie guarantee. 20,000 randomized ray-box oracle comparisons and explicit corner, negative boundary, unknown and budget cases. CI 45590992468 queued. No player-facing edits or persistence yet.

[Engine #62](https://github.com/swayam8624/KairoGameEngine/pull/62), feat/voxel-chunk-meshing, implementation head 0db87541ee728084bfbfb906361d235ba71e0bbd, stacked on #61. Material-preserving greedy opaque rectangular quads, signed absolute chunk origin, chunk-local integer corners, one-cell halo, no internal solid-solid faces across chunk boundaries; missing exterior flagged for remesh, missing interior rejected. CPU tests cover 40 randomized 16^3 chunks against expanded unit-face oracle, materials, winding, known/unknown halo, solid 16^3 cube expected 1536 exposed unit faces -> 6 quads. CI 45591202132 queued. No native rendering, transparent materials, T-junction stitching or live remesh yet. Next: native mesh adapter, deterministic voxel baseline, transactional edit-triggered remeshing, playable place/remove controls and persistence.

Total campaign 51 PRs, 39 previously verified merged, 12 pending at observation. Engine #55 -> #56 -> #57 -> #58 -> #59 -> #60 -> #61 -> #62; Core #31 -> #32 -> #33, Renderer #38 independent. Final accepted dependency pins and Metal gate still required. No direct default-branch modifications.

### Native Craft prototype and persistence (2026-11-25)

Engine #59 native Vulkan run 45588373182 passed: 300 moving updates, x=256 from x=16 (240 m traversal), 58 loads, 32 unloads, three captures and clean shutdown. Engine #61 ownership job 139124835710 passed 20,000 DDA rays against independent slab oracle, 4,600 hits, bounded edits and signed/unknown/corner cases. #60 native jump remains queued; #62 geometry ownership/native still queued.

[Engine #63](https://github.com/swayam8624/KairoGameEngine/pull/63), feat/voxel-native-mesh, implementation head 46b47d28deb6b51ea30efdb4503bc266f7e7703f, stacked on #62. Finite deterministic 48 x 16 x 48 block world, nine 16^3 chunks, grass/dirt/stone/placed material IDs, indexed local triangles and sharp normals/vertex colors, owner/face-neighbor dirty mask. CPU tests validate columns, seed difference, mesh vertices/indices and boundary ray edit/restore. CI 45591844695 queued.

[Engine #64](https://github.com/swayam8624/KairoGameEngine/pull/64), feat/craft-interactive-native, implementation head e1e95d85eefa54430e304919a3f514a690a44559, stacked on #63. New KairoCraft executable creative fly mode, ray remove/place, synchronous dirty-chunk uploads, candidate occupancy commit only after successful scene publication. Fixed-size mesh transaction journal preserves both generations after partial submission failure until recovery; retryable destruction; empty chunks supported. CPU test 10,000 three-chunk edits and fault injection. Native smoke requires baseline/removal/placement/restoration captures and exact restored image. CI 45592081697 queued, no native acceptance claim.

[Engine #65](https://github.com/swayam8624/KairoGameEngine/pull/65), feat/craft-world-saves, implementation head f2e3a2bc52d00053697ce57b3b3eee5078e139d7, stacked on #64. Canonical versioned little-endian seed+sorted overrides with checksum, rejects invalid/duplicate/noncanonical/truncated/oversized records. Explicit --save path startup load, F5 saves via same-directory temporary file and rename (single writer; no fsync/power-loss guarantee); stored seed takes precedence. CPU 10,000 random edits, 36,864 restored voxels, 100 corruptions and malformed records; native in-memory edited-world roundtrip. CI 45592289632 queued. Next: verify native Craft and CPU tests, then voxel character collision, streamed chunk residency, inventory and crafting.

Total campaign 54 PRs, 39 previously verified merged, 15 pending at observation. Review Engine #55 through #65 in stack order, after required EngineCore #31–33 and independent Renderer #38; update merged pins and remove review override before release. No default branches directly modified. Finite creative prototype, no multiplayer, infinite voxel streaming, voxel body collision or measured performance.

### Craft survival and native streaming (2026-11-25)

[Engine #66](https://github.com/swayam8624/KairoGameEngine/pull/66), feat/craft-survival-controller, implementation head 1476175791e6ad18931f127decdbc25b0e51e90f, stacked on #65. Upright AABB body, bounded <=1/8-unit per-axis motion, nonpenetrating contact bisection, gravity, jump, wall sliding, missing-cell blocking, block placement overlap rejection. Optional --survival on finite Craft sample retains creative default; boundary/floor containment. CPU test includes floor/wall/ceiling, high-speed fall, 10,000 large-coordinate steps; native smoke checks jump and capture. CI 45593821945 queued.

[Engine #67](https://github.com/swayam8624/KairoGameEngine/pull/67), feat/craft-streaming-window, implementation head 7c426754e36733074e1c48304ddaad9de473a381, stacked on #66. Seeded signed X/Z baseline with one 16-voxel editable vertical band; exact finite v1 equivalence. Stable fixed-capacity window planner preserves overlapping slots, recycles only expired slots, pure transaction plan and exact face-neighbor dirty mask. CPU tests 10,000 window transitions, one-column 20/25 overlap, signed mapping, billion-meter mesh and halo-domain rejection. CI 45594077069 queued.

[Engine #68](https://github.com/swayam8624/KairoGameEngine/pull/68), feat/craft-streamed-native, implementation head b8121ba99bfb5b7d3a8be7ba63c20b5054088fbc, stacked on #67. KairoCraftStream creative sample with 25-slot stable native window, cell-relative render origin, synchronous mesh generation/upload, transactional coordinate/handle publication and sparse in-memory edits preserved across eviction. Native smoke designed to check 80 loads, 55 evictions, 20 retained slots, six captures, billion-meter teleport and exact edited/reverted images on return. CI 45594282560 queued. No actual execution claim. Streamed-world disk persistence, background mesh jobs, survival-body integration and multiple vertical chunk bands still required. Previous #62–65 native/CPU CI remains queued or running; no newer successful evidence observed.

Total campaign 57 PRs, 39 previously verified merged, 18 pending review. Engine #55 through #68 in stack order; Core #31–33 and independent Renderer #38 dependencies; final merged-lock and Metal acceptance required. No default branch mutations.

### Streamed survival, asynchronous generation and inventory (2026-11-25)

Executed previous evidence: Engine #62 exact-head run 45591323290 passed CPU/native, 184,619 exposed unit faces expanded and compared, 165,274 greedy quads; solid 16^3 chunk reduced 1536 faces to six quads. #63 run 45591844695 passed, 9 seeded chunks, 510 quads, 1020 triangles. #65 implementation run 45592289632 passed native remove/place/restore with exact baseline image and clean shutdown, plus 10,000 three-chunk publication transactions, 36,864 restored voxels, 100 corruptions and 7,129 overrides serialized to 99,838 bytes. Explorer native jump peak 1.27375 m passed. #67 ownership job 139135431374 passed AABB collision (apex 1.2742 m, wall/ceiling/unknown/high-speed fall and 10,000 large-coordinate steps) and 10,000 window transitions, 176,898 retained slots, 73,102 loads. #67 native in progress; #68 native queued.

[Engine #69](https://github.com/swayam8624/KairoGameEngine/pull/69), feat/craft-stream-survival-save, implementation head da1dbbdcab601d4b837bdff0137b709a9e7996c6, stacked on #68. Streamed survival controller, safe placement, signed-coordinate v2 persistence, F5/startup save, v1/v2 separation. Tests include 10,000 edits, billion-meter coordinates, 100 corruptions, halo-domain rejection; native survival jump and save roundtrip. CI 45598961770 queued.

[Engine #70](https://github.com/swayam8624/KairoGameEngine/pull/70), feat/craft-async-meshing, implementation head e6a32eaf7ce11a931e4580f8eaf89056ad913aa0, stacked on #69. Optional 1–8 workers, fixed eight queue slots, immutable snapshot per window, generation identifiers, cancelled queued jobs and discarded stale results. Nonblocking interactive polling; complete candidate window uploaded/published on render thread; old scene remains until ready. Edits invalidate pending snapshots. CPU synchronous-oracle test across 40 transitions and stale generation; native four-worker smoke. CI 45599192333 queued. Upload/edit remesh remain synchronous; no measured latency.

[Engine #71](https://github.com/swayam8624/KairoGameEngine/pull/71), feat/craft-survival-inventory, implementation head 8b9338bc56f6b1e1dbcb51bb61b2892f4a41c8e9, stacked on #70. Four material stacks (999 each), selected material, mining credit and placement debit only on successful native edit, body/occupancy guards. V3 session envelope wraps v2 world plus inventory; v2 upgrades empty, v1 rejected. CPU 10,000 mine/place conservation cycles, failed edit and corruption tests; native inventory edit/restore smoke. CI 45599450571 queued. Creative can alter same world; not anti-cheat. No recipes/HUD/player pose yet.

Total campaign 60 PRs, 39 previously verified merged, 21 pending at observation. Engine #55 through #71 in stack order, required Core #31–33 and independent Renderer #38. Final actual merged pins and Metal acceptance still required. Next: multiple vertical voxel chunks, cave generation, granular upload budgets, recipes and proper HUD. No default branch modifications.

### Bounded GPU staging, vertical caves and recipes/HUD (2026-11-25)

[Engine #72](https://github.com/swayam8624/KairoGameEngine/pull/72), feat/craft-upload-budget, implementation head d761040591fcedf4e82bcf0f9552935c61c5012d, stacked on #71. Worker mode stages dirty native meshes in fixed slots with --uploads N call budget per update. Old scene remains until complete publication. Stale staging abort, retryable retirement and partial-submit restoration preserve ownership. Added 10,000 incremental staging cycles and fault cases. Native smoke uses two-upload cap. CI 45600204466 queued. Count budget only; startup and edit remesh remain synchronous; no measured timing/byte limit.

[Engine #73](https://github.com/swayam8624/KairoGameEngine/pull/73), feat/craft-vertical-caves, implementation head cd283bea1efed2e997eebb5464422eb19bf8b6f6, stacked on #72. Separate KairoCraftCaves target uses signed 16^3 XYZ chunks, stable 3x3x3 window, six vertical bands (-64 to 31), deterministic connected galleries, chambers and shafts. Generalized immutable worker queue, four uploads/update, creative and AABB movement, in-memory block edits and face-neighbor remesh. CPU tests 10,000 XYZ plans and worker oracle; native smoke designed for 90 loads, 63 evictions, 18 retained during vertical shift, four captures and far-world edit revisit. CI 45600533758 queued. Cave mode has no disk/session save or inventory yet; legacy terrain remains unchanged. Not natural cave noise, local lighting or unlimited vertical streaming.

[Engine #74](https://github.com/swayam8624/KairoGameEngine/pull/74), feat/craft-recipes-hud, implementation head ddb21c998ffc225b1e11e130bd38afb45c47a7bf, stacked on #73. R/T atomic recipes consume dirt/stone and produce building blocks, reject insufficient inputs/stack overflow. Streamed survival mode camera-aligned emissive geometry hotbar, numeric counts, selection border and crosshair, shared native quad. CPU test 10,000 randomized transactions/layouts; native HUD-on/off capture comparison. CI 45600946227 queued. Depth-tested geometry at reference 960x540, not a dedicated screen overlay; caves remain separate. No tool durability, advanced crafting grid or texture atlas.

Total campaign 63 PRs, 39 previously verified merged, 24 pending at observation. Engine #55 through #74 in stack order; Core #31–33 and independent Renderer #38. All three new feature PRs implemented; none has completed CI as of this observation. Actual merged lock, Apple/Metal validation, cave session integration and UI compositor remain open. No default branch changes.

### Unified cave survival session (2026-11-25)

Branch `feat/craft-cave-survival-session`, stacked on #74. Separate v4 cave session codec persists the seed, ordered signed XYZ overrides and four inventory stacks with selected slot; rejects older incompatible world formats, invalid coordinates/materials, malformed inventory, checksum corruption and oversized records. Native cave survival reuses transactional mining/placement, R/T recipes, 1–4 selection, camera-aligned hotbar and crosshair; F5 explicitly saves through same-directory temporary rename. CPU regression adds 10,000 conservation cycles, 10,000 far underground overrides, 100 corruptions, canonical encode/decode and incompatible format rejection. Native smoke exercises edit retention, save codec and HUD-on/off image comparison. CI pending; no power-loss durability, player pose persistence, dedicated screen compositor or general GPU latency claim. Renderer #38 macOS job 139104861077 rerun requested successfully after cancellation. No default branch modifications.

### Exact combined gameplay review candidate (2026-11-25)

Candidate branch `integration/gameplay-native-candidate` stacks on #75. It pins Core #33 review head fee0bfa6b74d6847826e678333c1a941a004c989 (includes #31–32) and Renderer #38 review head f81208c6bbecd3068b9a944b3d69d841efcd3eb3 in the actual lock. Other 24 lock entries remain unchanged. The terrain workflow removes its post-lock override and checks both software Vulkan and a separate native Metal build/capture job against that exact lock. This is an explicitly disposable review candidate, not the final merged release lock: after sibling merges, replace with their actual main revisions and repeat exact-lock acceptance.

Confirmed earlier: #55, #56 and racing integration failed MetalBackend.mm warnings-as-errors at lines 732 and 860; renderer #38 addresses these. Core #31–33 terrain contracts, Linux and Windows jobs passed, their macOS jobs were cancelled. Reruns requested for jobs 139068470508, 139069322255 and 139088123995; renderer macOS job 139104861077 also queued. Native candidate results are pending. Do not treat an unavailable presentation session as a passing Metal capture; local Apple Silicon interactive verification remains separate.

### Integration repair and cave spawn safety (2026-11-25)

The malformed candidate workflow was reconstructed from #75 in 9ee2aca; GitHub accepted all three jobs (ownership, native-vulkan, native-metal) in run 45602979574. Subsequent candidate work replaces hardcoded survival spawn with deterministic bounded search of the edited authoritative world before selecting resident chunks. The CPU regression covers 1,024 seeds, determinism, unsupported shafts, unavailable/empty/solid worlds and an actual serialized edited spawn column. Search covers 17x17 horizontal cells and supported surface positions only; if no admissible position exists, startup fails explicitly rather than spawning inside geometry. Creative camera startup and session format are unchanged. Native acceptance and the new CPU regressions remain pending.

### Responsive Craft HUD (2026-11-25)

Separate feature branch on the combined candidate, preserving #76's acceptance head. Both Craft demos share camera-plane projection with uniform scaling, a centered crosshair and bottom-aligned hotbar. Interactive framebuffer changes explicitly resize the offscreen target, excluding zero-sized minimized windows; smoke defaults remain 960x540. Exact standalone header/test blobs f2a3a037d673ebcb3a89c03d36101b69a7deb702 and ee6259563e820e6c4fe5149ef9d2f112b451b700 passed 100,000 seeded randomized projections locally (Clang 17, Linux x86_64, ASan/UBSan). Native portrait 480x720 and wide 1280x480 smoke now checks actual capture dimensions, HUD-on/off difference, world restoration and original 960x540 restoration in both demos. Native resized rendering and module compilation remain unverified. This is still depth-tested scene geometry, not the final screen-space compositor.

### Native Vulkan Craft overlay (2026-11-25)

Renderer #39 introduces bounded opaque pixel rectangles after world draws in the Vulkan viewport color attachment, before capture; depth and picking attachment remain untouched. Engine candidate pins its exact review commit 0c0c85625637b365910113c7e444e0687bac03c2. Both Craft demos route Vulkan HUDs through the rectangle list, avoid HUD mesh allocation, paint backing before glyphs and share quantized pixel edges across sizes. Metal/OpenGL/D3D12 retain the earlier camera-aligned fallback. Added CPU shared-edge and bounds regressions, plus renderer-native clipping/order/picking/capture regression. Native and new CPU results pending. This is a review lock, not the accepted merged dependency snapshot. Alpha blending, text shaping and native backend parity remain open.

### Vulkan/Metal native HUD parity candidate (2026-11-25)

Renderer #40 adds Metal's scissored fullscreen triangle overlay, color-only attachment write mask, always-pass no-write depth and common capability facade. Both Craft demos now use the native rectangle API on supported Vulkan/Metal backends and retain the earlier geometry fallback only on OpenGL/D3D12. Exact Renderer review pin f63f55c6b3c5e4b01196e06b5fb2fd4f9e9d0419 includes #39. Renderer smoke adds exact native overlay pixels, picking, clipping, ordering and restoration; existing engine portrait/wide tests exercise the new path. CPU-only layout test verified exact blobs 5b96d6c81a43a4918dc87556986bb1c920cd4f8d and d3835b5e60ee31176389ac7455c10502408688b0 with Clang 17 ASan/UBSan: 100,000 randomized cases passed. Native compile, MSL and GPU checks pending. Do not accept unmerged review pins as release dependencies.

### Native HUD publication isolation (2026-11-25)

Separates native HUD publication from world mesh submission and camera movement. A key containing enable state, inventory, selected slot and physical viewport extent suppresses unchanged updates; disabled keys collapse to one state. Failed renderer submission leaves the previous key retryable. Existing fallback geometry behavior unchanged. Standalone Clang 17 C++23 ASan/UBSan passed 100,000 idle frames, 100,000 seeded state transitions (32,561 state publications) and injected failure retry. Exact local source hashes 30f7490ecc963a1f57d2376d6ceeee4e9aea1421 and e82dad8ce9f62747319942f06a0f21a9963fc808; native integration remains pending. No measured GPU speedup claimed.

### Survival vitals and fall damage (2026-11-25)

Both Craft survival demos track 20 hit points and sampled peak-to-landing fall distance, with three world units safe. Physical support must be present in committed voxel queries, so artificial streaming barriers do not cause fall damage. Lethal damage returns to the existing spawn routine and resets health, retaining inventory/world edits. Adds responsive health bar and title display; native HUD key includes health. Health is session-local and resets on load; save formats unchanged. No combat, hunger or item loss yet. Standalone exact Clang 17 ASan/UBSan tests passed 100,000 randomized falls plus jump, virtual boundary, lethal saturation and respawn. Native player integration still pending; fall height uses frame samples rather than substep apex.

### Timed survival mining (2026-11-25)

Survival left mouse hold is now material-dependent (grass .30s, dirt .45s, stone 1.50s, building block .90s). Changing target/material, releasing input, unavailable targets, death or failed edits clear progress. Per-frame elapsed contribution capped at .1s, no carry-over. Inventory/world commit still goes through existing transactional mining. Cave survival protects the bottom layer from mining; creative mode and legacy save format unchanged. HUD gains a quantized progress bar. Local exact standalone Clang 17 C++23 ASan/UBSan test passed 100,000 randomized mining holds and explicit reset/failure cases; header/test blobs a8ab31002ebe0e5c4e549aa46bc88b2debbe5da9 and baa43112ddd4db8636618287adcfb4bba7175494. Both native smoke paths now perform the timed hold through the actual mesh/edit/inventory transaction and check the material and inventory every step. CPU integration regressions add 10,000 timed mine/place conservation cycles, full-stack rejection, and HUD progress/cache transitions. These additional integrated tests and native gameplay acceptance remain pending; equipment/durability not implemented.

### Equipment progression and repaired overlay candidate (2026-11-25)

Craft a stone pick (F: 2 dirt + 3 stone), toggle equipped/stowed (G), 2x mining speed for stone/building, 64 successful eligible edits to break. Failed/full inventory edits consume no durability; speed changes reset accumulated hold. HUD durability bar and cache key; existing inventory material counts stay compatible. v5 checked envelope carries equipment state around v3 surface or v4 cave payload; old v2/v3 surface and v4 cave saves load without equipment. Health remains session-local. Standalone local Clang 17 C++23 ASan/UBSan: 100,000 randomized equipment transactions, 64-use breakage, speed reset and failed commit passed. Exact tested blobs 97356092d345aa6810eeb7b5dd238fa8da6ee3e6, 2bec832b0d8de65d46bbf3cf4a2be830603b3524, f87c844515a6d3bfbb6ed95d1974c64918436847. Native smoke now crafts and uses the pick through actual mesh/edit/inventory transactions on both demos, verifies durability and roundtrips the v5 envelope. Full CPU regressions cover every single-byte corruption of a surface fixture, invalid state with correct checksum, truncation, nested envelope rejection, cave payload type separation and HUD publication. Native integrated and codec tests pending.

Renderer #40 Linux and required Vulkan build failed due duplicate local variable in VulkanInstancingSmoke; both #39/#40 fixed, this candidate pins corrected #40 head 8d002fa441ed6748470c4e38373776e9d35f5d44 and already-merged Core #33 tree 523ed41da74ee117256281a3ebc45cde91368bc6. Baseline merged-lock acceptance is separate Engine #83. No claim that the overlay repair passes native tests yet.

### Actual standalone Vulkan presentation candidate (2026-11-25)

Renderer #39 repaired exact head 8cc5f13d5b616d43da371c707190d4c3e48faa2b passed required software Vulkan smoke 45624713524, including native depth-independent viewport UI, clipping, painter order, picking and untouched image pixels. Renderer #40 repaired head 8d002fa441ed6748470c4e38373776e9d35f5d44 still awaits native Metal. Renderer #41 implements missing default standalone swapchain draw: sample HDR viewport with fullscreen triangle, preserve editor callback, rebind descriptor by viewport generation after fence, global image dependencies, correct orientation and UNORM/SRGB handling. New required Xvfb test samples the actual visible window after viewport resize, not the renderer's capture. This candidate pins #41 exact review head 1312532edf3d19123ffba66172df74e6a51cc01a; native presentation not yet validated.

Equipment v5 outer codec exact blob 5ac607962efd8bc5f779d9d5c1217ea50484daca passed a local isolated Clang C++23 ASan/UBSan harness: 10,000 synthetic v3/v4-tagged envelope roundtrips and 40,000 corruption/truncation/invalid-state/size-limit rejections, using identical CraftSave checksum and byte helper implementations extracted from existing source. This verifies outer envelope only; the checked-in regression compiles against full real headers and the integrated surface/cave codec tests separately exercise genuine world data. New merged-lock baseline Engine #83 remains queued; do not conflate with this later candidate.

### First-person mouse controls (2026-11-25)

Surface and cave demos capture mouse by default, use raw motion when supported, Tab toggles capture and arrow keys remain fallback. Mouse samples use per-pixel deltas (not scaled again by dt), focus/capture changes reset baseline and mining progress, button actions require release after recapture. Cursor warp cap and pitch/yaw constraints preserve camera basis. Standalone Clang 17 C++23 ASan/UBSan passed 100,000 randomized mouse/focus transitions, exact header/test blobs a7f8a8ec158079897145ac4a7ef964d45f79d93f and fdec532be1e45de6f64575907d2129fe55f23d06. Actual GLFW focus, raw input and interactive behavior need native verification. No claim of completed game.

### Fixed-clock survival nutrition

20 Hz survival clock with idle drain every 30 seconds, 3x drain while moving and sprinting, sprint disabled at hunger <=4, starvation damage and food-funded regeneration every four seconds under respective conditions. E consumes two harvested grass blocks for six hunger (prototype forage abstraction). Full hunger or insufficient inputs do not spend materials. Nutrition paused when unfocused, reset on death/load; health and hunger remain session-local. HUD/cache includes hunger. Local Clang C++23 ASan/UBSan passed 100,000 randomized frame partition comparisons, 100,000 forage transactions, explicit sprint, starvation, regeneration, respawn and stall guards. Exact tested blobs b631d874687630b56e3b1a1b0f0b930023279338, 5a76b2c335c50f3f23f9e5e3319419d58fb4726b, 3f74af0fafdf1a32015b64d97e8b4b64f837e467. Native integrated results pending. Time partition equivalence applies to constant activity within each interval, not arbitrary input changes.

### Edited surface spawn safety

Surface survival startup and respawn now search the authoritative edited world for physical support and collision-free headroom, within 16 cells of origin. The chosen window is settled before the physics solver validates it. Creative startup does not need a survival spawn. Local exact-source Clang C++23 ASan/UBSan passed 10,000 signed randomized support fixtures and explicit edited shaft, top-layer, unknown/empty/solid worlds and actual voxel movement. Header/test blobs 1b17ddf8d87781be4e1d6ab85a7b9dcda299dde6 and cd770b18fe150cad12bb371360c91edc6eb117c6; existing VoxelPlayer and VoxelInteraction blobs also matched. Native startup integration awaits CI. Exhausting the finite search produces an explicit error rather than silently spawning on the artificial floor.

Both nutrition and surface-spawn standalone regressions also passed GCC 14.2 C++23 ASan/UBSan with warnings as errors (same exact source). Added integrated HUD fill bounds, invalid hunger and visibility-canonicalized cache regressions; their full-header CI execution is pending.

### Focus-safe gameplay actions

Both interactive Craft loops pause physics, hunger, world updates and input actions while unfocused, resetting the frame clock and continuing event polling. Seven independent release-to-rearm latches gate jump, recipes, equipment, forage and saving; held keys on startup or focus regain cannot synthesize a new action. Existing mouse release gate remains. Exact standalone Clang and GCC C++23 ASan/UBSan passed 700,000 randomized key samples with focus resets, 161,096 accepted actions and explicit held-key cases. Header/test blobs 3d3c99c3f740a9bc11b2b74c509dd2d730c4ec86 and e2a7548229c5a20e5a105e5ba1e6664f7570389c. Native focus/minimize behavior awaits real window testing; no changes to headless smoke loops.

### Cross-system CPU survival journey acceptance

Reconstructed real source files from the exact candidate by GitHub blob hash, without substitute helpers or baseline stubs. Clang 17 and GCC 14.2 C++23 with -Wall -Wextra -Wpedantic -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer, leak detection and halt-on-UB each passed the existing full CraftInventoryTests (20,000 mine/place cycles, real v2/v3/v5 codec roundtrip and corruption rejection), plus 512 deterministic survival journeys (256 seeds per surface/cave world) in the new test. Journey uses actual world ray queries, timed edits, inventory, equipment, recipes, food/healing, HUD bounds, real nested world/equipment encoding, canonical reload and spawn/collision. It is a CPU subsystem integration test, not interactive player traversal, GPU publication or native presentation.

Exact existing inventory test 0a5027e2446c3e1dda0a9f8cf4b5dfd723791f8c; full world dependencies CraftSave ce516f0a634c96d9cba1f8101de77acc9ff82ae3, CraftStreamSave bc5e6adee11cf82693ecf22ef0fa9aeb0227c3c1, CraftInventory 7b250a8f8535cfa3527406e949a33733976c97ee, CraftStreamingWindow 7c4291cbf287da2228d4fa2439d32bfd1b62dc4d, CraftWorld 8c1c96186b2d67f003267d8bd64804f1c0c4cd6d, VoxelChunkSurface eb86aa49ec68f41f3e87071652e700d75a267bdf, VoxelInteraction 18557fc2d5ace936591dd5a6af2f21440ad3f500, CraftCaveSession 63f1dc5f500c87642ed5be23c90d5919ee5aae71, CraftCaveWorld 05ff96a0286f500fa5f254783e13ba71a29ffcfe, CraftCaveSpawn f7727fadb7ac9ea566b16309a13ef2794ed357fe, CraftRecipesHud f86253bf6f0da162e6601fdb5a422f56f578e742. New exact journey test bbbb933fc212d301565594da2c276b7fd0882828. Native acceptance and merged-lock baseline remain separate and pending.

### Reusable SDK-free Craft CPU gate

Added scripts/run_craft_cpu_contracts.sh with 23 named C++23 contracts, strict warnings, ASan/UBSan, isolated temporary outputs and fail-closed missing/unknown-test checks. Replaces the 23 duplicated compile/run snippets in the ownership job with the same test inventory (compared programmatically). The script exact blob 989d81ebfd1d55bbdf2985381e458e1c276b8df4 passed bash syntax and selected 7/7 contracts on Clang 17 in the reconstructed source workspace. The full 23-test invocation requires the remaining repository files and is pending CI. Native tests unchanged.

### Shared save-file transaction

New shared bounded regular-file read and same-filesystem replacement writer,
used by both demos. An atomic directory reservation prevents check-then-open
races between cooperating writers; failure leaves the previous save intact
and removes only this writer's staged file/reservation. A stale reservation
fails closed. No fsync or adversarial filesystem guarantee. Exact header
dfcdb23da3b637177a76fdcfa1941853f9f3cf77 and test
1a0340020962a0dc0e678b32ed47087e0f9ce132 passed on Clang and GCC C++23,
strict warnings, ASan/UBSan: 256 actual disk replacements each, size caps,
competing writer refusal and failed rename cleanup. Registered as 24th CPU
contract and native CTest gate. Native Windows/macOS filesystem behavior
and demo integration pending. Added 256 actual encoded world-session disk roundtrips (128 seeds per world) on each compiler, verifying world edits, inventory, tool state and wrong-world rejection. Current tested exact blobs ebe97d3ab55c64ff9573208b18f79fa61fc4055c and ef82f481cd44df47de2ba6a88d3269e083f2f848. Prior #87 head unchanged.

### Relocatable development bundle candidate

Renderer #42 embeds all 12 SPIR-V blobs at build time, eliminating the
absolute runtime build shader directory. Exact generator, manifest and word
comparison test verified locally on synthetic binary fixtures: 63,426 words
on Clang/GCC, invalid magic/size rejected and deterministic output. Native
shader/module verification remains pending. This engine branch pins review
head 6b70ed2ed15fbd2c57ae601609afd2978c62e240. Added development bundle staging
with copied binaries, SHA-256 manifest, relative launchers, separate save paths
and refusal to overwrite an existing output among cooperating writers.
Local CMake 3.31 fixture test passed paths with spaces, copy hashes, default
arguments, explicit argument forwarding and existing-output rejection.
Exact staging script blob 8b2b38b74a56528dc5ca25f8a9594475ea2e2c43.
The fixture executables are scripts, not actual Craft builds. Native Vulkan
and Metal CI run copied surface/cave executables outside the build tree.
No claim of self-contained distribution, Windows launcher execution or
native acceptance; earlier integration candidates unchanged.

### Surface day/night lighting and MSVC repair

Renderer #39–42 repaired C4457 (viewport overlay extent shadowed swapchain
extent in VulkanTriangle::Record). Latest exact renderer pin
617f3eceb26b56932f91788f0d6629edd2f13cf4. Fresh Windows/native validation
pending. Earlier queued review heads remain unchanged.

Surface daylight uses a ten-minute simulation cycle, 240 quantized samples,
bounded frame contribution, continuous sky/ambient/sun colors and normalized
sun direction. Updates authored light/environment without regenerating
terrain meshes. Local Clang and GCC C++23 ASan/UBSan passed all 240 phase
samples and 100,000 randomized frame partition comparisons per compiler.
Exact header d0ae9a75574b2c612e13d42e3a1d992980c5d0d0 and test
597c62379e14da8198e108a5b7adc699c0922a28. Native smoke checks noon/night
capture difference, unchanged residency/edit counts and exact restored
baseline. Full native acceptance remains pending. No persistent world clock,
dynamic shadows, weather, mobs or underground lighting change in this pass.

### Block target and placement feedback

Both demos submit depth-tested debug AABBs for the current aimed block and
face-adjacent placement candidate. Use int64 subtraction before float
conversion and a small expansion to avoid coplanar edges. Cache by target,
render origin, placement and preflight state; re-query after editing.
Target gold/red and placement green/red reflect inventory, protection,
bounds and player overlap. Does not promise edit capacity or native
publication success. Local Clang/GCC C++23 ASan/UBSan passed 100,000 idle
target frames and 100,000 rebased geometry fixtures per compiler, plus
color changes, empty state and retry. Exact header
a063d0da09c827e4568a2ae081a8b6eb6d423a50 and test
d0df5d0b697be0f0a7ca02ff0cc5da662063173f. Native surface smoke checks
outline image change and removal. Full native acceptance pending.

### Autosave and exit checkpoint

Both games opt into automatic saving only with an explicit path. Dirty stamp
covers committed world edits, inventory counts/selection and equipment.
30-second focused-time checkpoint, five-second retry after write failure,
F5 immediate checkpoint and dirty exit flush. Continuous edits cannot postpone
saving. Existing shared file writer retains previous payload on failure.
Local Clang/GCC C++23 ASan/UBSan passed 100,000 idle frames, 100,000 randomized
frame partition comparisons and 430 simulated checkpoint events per compiler.
Exact header ab3e5a5342b67fd31c869252fcd5d0608d77c567 and test
9be9f45251b577c3584c7817cfc25477e3ae369f. Scheduler test does not write actual
files; prior shared disk transaction regression covers I/O separately.
Native interactive exit/save acceptance pending. No crash-durability claim.

## Final integration gate

1. Preserve the local working tree and inspect pending diffs and the merged revision history.
2. Apply approved sibling revisions and CI infrastructure; run standalone checks.
3. Refresh engine workspace lock using the actual approved sibling commit SHAs, not branch names or guessed revisions. Preserve historical evidence; do not mark an untested lock accepted.
4. Build game-clang and strict game-player-clang, run CTest with explicit native Metal smoke on Mac and cross-platform CI. Exercise editor undo/duplicate/prefab and the runtime resource journal.
5. Separately run legacy optional compute and portfolio workflows if retaining those components.
6. Record exact revisions, host/compiler, failures and skips. Only then mark the combined game-first build accepted.

Future passes should append entries here; keep the merge queue and exact-head evidence distinct from the roadmap.
