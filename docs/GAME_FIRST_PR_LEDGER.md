# Game-first integration ledger

Updated 2026-11-24. This is a review/apply queue, not an accepted build stamp.
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

## Final integration gate

1. Preserve the local working tree and inspect pending diffs and the merged revision history.
2. Apply approved sibling revisions and CI infrastructure; run standalone checks.
3. Refresh engine workspace lock using the actual approved sibling commit SHAs, not branch names or guessed revisions. Preserve historical evidence; do not mark an untested lock accepted.
4. Build game-clang and strict game-player-clang, run CTest with explicit native Metal smoke on Mac and cross-platform CI. Exercise editor undo/duplicate/prefab and the runtime resource journal.
5. Separately run legacy optional compute and portfolio workflows if retaining those components.
6. Record exact revisions, host/compiler, failures and skips. Only then mark the combined game-first build accepted.

Future passes should append entries here; keep the merge queue and exact-head evidence distinct from the roadmap.
