# Kairo development log

Append dated entries; never rewrite failed results as successful. For every
change record the reason, affected source, actual result, tests run, evidence
links, remaining limitations and next dependency. Distinguish committed,
merged, pinned, CI-passed and host-verified. This file is the workspace
chronology; component-local logs should link back to it.

## 2026-11-25 — M50 and teaching campaign

- Base: GameEngine `c489bf16f1aa4f697d20a9c160da287d85bb3b0f` (merged #105).
- Open candidates at inspection: [#106](https://github.com/swayam8624/KairoGameEngine/pull/106),
  [#107](https://github.com/swayam8624/KairoGameEngine/pull/107),
  [#108](https://github.com/swayam8624/KairoGameEngine/pull/108),
  [#109](https://github.com/swayam8624/KairoGameEngine/pull/109);
  Editor [#22](https://github.com/swayam8624/KairoEditor/pull/22), [#23](https://github.com/swayam8624/KairoEditor/pull/23);
  Core [#34](https://github.com/swayam8624/KairoEngineCore/pull/34),
  [#35](https://github.com/swayam8624/KairoEngineCore/pull/35),
  [#36](https://github.com/swayam8624/KairoEngineCore/pull/36),
  [#37](https://github.com/swayam8624/KairoEngineCore/pull/37).
  These are candidates, not accepted workspace pins.
- Define [M50](docs/MILESTONE_50.md), create the [study order](STUDY_ORDER.md)
  and extend in-source explanations in the following files.

| Source | New explanation blocks | Verification |
|---|---:|---|
| [Runtime/KairoPlayer/RuntimeFrameClock.hpp](Runtime/KairoPlayer/RuntimeFrameClock.hpp) | 3 | Non-comment lines identical |
| [Runtime/KairoPlayer/RuntimeInputGate.hpp](Runtime/KairoPlayer/RuntimeInputGate.hpp) | 9 | Non-comment lines identical |
| [Runtime/KairoPlayer/RuntimePickupPool.hpp](Runtime/KairoPlayer/RuntimePickupPool.hpp) | 9 | Non-comment lines identical |
| [Runtime/KairoPlayer/CraftVitals.hpp](Runtime/KairoPlayer/CraftVitals.hpp) | 9 | Non-comment lines identical |
| [Runtime/KairoPlayer/CraftNutrition.hpp](Runtime/KairoPlayer/CraftNutrition.hpp) | 8 | Non-comment lines identical |
| [Runtime/KairoPlayer/CraftActionLatch.hpp](Runtime/KairoPlayer/CraftActionLatch.hpp) | 5 | Non-comment lines identical |
| [Runtime/KairoPlayer/CraftDaylight.hpp](Runtime/KairoPlayer/CraftDaylight.hpp) | 8 | Non-comment lines identical |
| [Runtime/KairoPlayer/CraftCombatReward.hpp](Runtime/KairoPlayer/CraftCombatReward.hpp) | 5 | Non-comment lines identical |

No C++ code changes in these eight files. Native tests not run for this
documentation pass. The first-party inventory is a tracking boundary, not
a claim that all files are annotated. Next: implement and test the first
under-floor gameplay capability and continue annotation in dependency order.

## 2026-11-25 — quest/dialogue foundation

Added [RuntimeQuestDialogue.hpp](Runtime/KairoPlayer/RuntimeQuestDialogue.hpp), [CPU regression](Runtime/KairoPlayer/tests/RuntimeQuestDialogueTests.cpp), and [terminal RPG sample](Samples/QuestDialogue/QuestDialogueDemo.cpp). Prerequisites form an authored DAG; duplicate completions are idempotent, restore is transactional, and dialogue choices can gate or award objectives. Added CTest target and 500 seeded chain fixtures. Candidate implementation only until test execution is observed. Native Player integration, disk save envelope and visual authoring remain open.

### Local verification of quest/dialogue

Copied the three candidate source files into an isolated Linux test directory and verified their Git blob identities. Initial GCC -Werror compilation caught an ignored nodiscard return; fixed explicitly. Recompiled the corrected header with GCC and Clang (C++23, -Wall -Wextra -Werror): both passed the complete regression executable and 500 seeded chains. GCC-built terminal demo also rejected a locked choice and reached Quest complete with input 2,1,1,2. Corrected header blob: 04f77dc01c9d6837b9a6615f7392929dec11f312; test blob: e2685c3650608a02a44fa23d3369a826dc89d64a; demo blob: 0d9e78b1f3c69a67636427ce9d3e5e2560d2cee0. This is isolated CPU verification, not the full workspace build, native rendering, or disk persistence gate.

## 2026-11-25 — foundation study catalogs

Indexed 205 filtered paths across 8 exact pinned component trees: KairoMath, KairoGeometry, KairoSpatial, KairoPhysicsMath, KairoPhysicsEngine, KairoAssets, KairoECS, KairoReflection. Added component file checklists to STUDY_ORDER.md. The other sibling trees remain linked but not yet enumerated. These inventories do not certify per-function comment coverage.

## 2026-11-25 — runtime, rendering and tooling study catalogs

Indexed another 390 paths across 9 pinned trees: KairoAI, KairoEngineCore, KairoRenderer, KairoEditor, KairoRayTracer, KairoGPU, KairoSIMD, KairoScheduler, KairoHub. Cumulative sibling catalogs: 17/26 repositories and 595 paths. These are file checklists, not completed annotations.

## 2026-11-25 — remaining research and DCC study catalogs

Indexed another 105 paths across 9 pinned trees: KairoONNX, KairoTransformers, KairoMacPerception, KairoPipelineCore, KairoBlender, KairoProductionTools, KairoHoudini, KairoMaya, KairoNuke. All 26 sibling inventories now present, 700 matching paths. This completes baseline file navigation, not all-function annotation.

## 2026-11-25 — analytic two-bone IK

Implemented a pure Cartesian two-bone kernel in [RuntimeTwoBoneIK.hpp](Runtime/KairoPlayer/RuntimeTwoBoneIK.hpp), with bounded input validation, inner/outer reach shell, folded and collinear-pole behavior. [Tests](Runtime/KairoPlayer/tests/RuntimeTwoBoneIKTests.cpp) passed under GCC and Clang with C++23 -Wall -Wextra -Werror: 10,000 seeded cases per compiler plus singularity and invalid inputs. [Terminal demo](Samples/AnimationLab/TwoBoneIKDemo.cpp) reached a valid target and clamped both unreachable targets. Locally tested blob IDs: 91a57d786ed88c631e47ddaf160a3a14a1becbf7 (header), 38ee1ac831a44ea3cb241dc99aaba43405b43a68 (tests), d8168d82dbfc1896b8d113e8b2e50660571c9ec3 (demo). No native joint-rotation/skin presentation tested. Next integrate pose adapter.

## 2026-11-25 — quest disk persistence

Added [RuntimeQuestSave.hpp](Runtime/KairoPlayer/RuntimeQuestSave.hpp) and dialogue position restoration. Reuses CraftSaveFile rather than duplicating filesystem logic. GCC and Clang C++23 -Wall -Wextra -Werror both passed disk roundtrip/replacement, every truncated prefix, trailing bytes, unknown node and rollback tests. Two separately executed demo sessions saved and restored progress to a completed quest. Tested blobs: header 124387420ddda5c02dc7edd04fa8c4fe5fb5e0fd; dialogue 1382f8b7a8699df17f0c0437235b180d7d889524; regression 1f50f70423ff25552d91e8e3e32a236995c76f2d; demo ec8ade0f53e47a86dbbd3eb4ee520c692f4adb48; helper ebe97d3ab55c64ff9573208b18f79fa61fc4055c. Native graphical NPC/Player integration remains open; no authenticity or cross-definition migration promise.

## 2026-11-25 — compatible-skeleton animation retargeting

Added [RuntimeAnimationRetarget.hpp](Runtime/KairoPlayer/RuntimeAnimationRetarget.hpp), quaternion normalization and rest-relative rotation mapping, name/parent checks, target proportion preservation, scaled root displacement and unmapped rest joints. GCC and Clang C++23 -Wall -Wextra -Werror both passed 5,000 seeded poses each, noncommuting quaternion fixture and invalid inputs. The [five-frame CPU clip sample](Samples/AnimationLab/RetargetDemo.cpp) retained target length and scaled displacement. Tested blobs: e7a9eb1ac38e60c1a1ee660979f3e4a0e887660c (header), b4210541aeb8ead62f025de50eac65311b5e5899 (tests), 83ccbae66775a1677d44c43cdb9cd021d79ff011 (sample). Compatible-axis scope only; no glTF import adapter or rendered rig acceptance yet.

## 2026-11-25 — native loopback network and authoritative movement

Added versioned bounded wire protocol, integer world-state authority with strict next-tick validation, move-only TCP socket ownership, exact framing, POSIX/Winsock adapter and two-process sample. GCC and Clang C++23 -Wall -Wextra -Werror each passed 10,000 seeded ticks, duplicate/future rejection and byte-order fixtures; each built sample pair agreed on tick 10, position (10,0). Tested blobs: protocol 92e0f1baa1b053b21bcf1cc1206f3f5e3a958a41; transport 868ac93811ab75eeb79f4d9387a9c1441dc9eb5b; tests aab5fd36ef5183f5aa3c6c600b1c7da5acd311a1; lab 785477312031f2490d1197c94675b92d4eae105c; runner b5415d191db4dd1f9b24f9a41d022deff6de6329. Linux loopback only observed; no internet-facing service, rendered multiuser game or reconciliation evidence.

## 2026-11-25 — client prediction and reconciliation

Added [RuntimeNetworkPrediction.hpp](Runtime/KairoPlayer/RuntimeNetworkPrediction.hpp), bounded 256-input journal, stale/future snapshot handling, authoritative correction and pending-input replay. Integrated it into the actual loopback client. GCC and Clang with C++23 -Wall -Wextra -Werror each passed 64,000 simulated delayed/dropped/reordered snapshot ticks, explicit divergence/correction, journal saturation and resumed prediction; each native pair again converged on tick 10. Tested blobs: header 49034205b47f1cf0a17c34b3e8526d5327e2aa48, test 83821ddd199cdae6f9fdf5bf1d0b615df88ea6d1, lab 3034bc4c9682acdede6df7f27ebee963a2fbfea7. Snapshot impairment is simulated; reliable ordered input delivery is assumed. No UDP retransmission or real physics replication claim.

## 2026-11-25 — teaching pass: audio, GPU lifetime and persistence

Added 36 teaching blocks in six existing headers; all non-comment lines compared identical before commit. The changes document every principal function/ownership boundary in these files, including SPSC acquire/release memory ordering, PCM quantization, partial device writes, GPU handle rollback, bounded save I/O, and autosave scheduling.

| File | New explanation blocks |
|---|---:|
| [RuntimePcm16.hpp](Runtime/KairoPlayer/RuntimePcm16.hpp) | 2 |
| [RuntimePcmQueue.hpp](Runtime/KairoPlayer/RuntimePcmQueue.hpp) | 11 |
| [RuntimePcmPump.hpp](Runtime/KairoPlayer/RuntimePcmPump.hpp) | 5 |
| [RuntimeRenderResourceTracking.hpp](Runtime/KairoPlayer/RuntimeRenderResourceTracking.hpp) | 4 |
| [CraftSaveFile.hpp](Runtime/KairoPlayer/CraftSaveFile.hpp) | 5 |
| [CraftAutosave.hpp](Runtime/KairoPlayer/CraftAutosave.hpp) | 9 |

This was a comment-only pass. No new native build run. Full all-file annotation is still open.

## 2026-11-25 — weather and daylight composition

Added [RuntimeWeather.hpp](Runtime/KairoPlayer/RuntimeWeather.hpp), seeded 30-second epochs with quantized transitions and lighting modulation. The existing [Craft streamed game](Runtime/KairoPlayer/CraftStreamDemo.cpp) accepts --weather; weather advances during active frames and changes the actual submitted sky, sun and ambient lighting through the existing scene publication path. GCC and Clang C++23 -Wall -Wextra -Werror each passed 120,000 seeded transitions and lighting-bound checks. Header blob c7b115916da35e7a688ee5d4515d734c7139f332; test blob 2aa6a606a2a58efbea964e3a66e418f3985fc1e1. Native renderer build/capture, visible precipitation and persistence of weather time remain unverified or unimplemented.

## 2026-11-25 — visible rain geometry and native gate

Added [RuntimeRainStreaks.hpp](Runtime/KairoPlayer/RuntimeRainStreaks.hpp): deterministic camera-local falling streak positions, bounded 64 instances, 10 Hz sampling and absolute double coordinates. Both GCC and Clang C++23 -Wall -Wextra -Werror passed 27,200 seeded rain frames per compiler, including large-coordinate bounds and fall/wrap invariants. Header blob 2ffb0b6b2dc153f185f8dcc4703c5975ab2e42de; test blob e515c3e197b17720ef5207a9bc68d09ecb1906c1. Craft streamed Player now submits actual streak geometry using an existing unit cube, and smoke path compares wet sky, rain, removal and restored baseline. Native capture test is added but not executed here. No rain collision or persistent weather clock.

## 2026-11-25 — conservative terrain water

Added [RuntimeWaterGrid.hpp](Runtime/KairoPlayer/RuntimeWaterGrid.hpp), a bounded depth-only conservative flow model distinct from the existing EngineCore density diffusion. It transfers volume across four-neighbor terrain edges with donor limiting, nonnegative depth, closed boundaries and validated dimensions. Both GCC and Clang C++23 -Wall -Wextra -Werror passed 20,000 random-bed time steps per compiler with conservation tolerance 1e-8, plus symmetric pulse, dry wall, lake-at-rest and bounds fixtures. Header blob 96b2865f6317a9ba595b7773b8ac45a98bf5c462 and test blob 3ca0bbea9573e907a0237b80ea82cbc93e3e820c. Added a basin ASCII sample (not yet run at this entry); GPU surface adapter, momentum and waves remain open.

### Water lab execution

The exact sample blob 0b78d533478ff67699ce7845928534a8f1733115 built and ran under GCC and Clang with C++23 -Wall -Wextra -Werror. Both produced byte-identical output, reporting volume 8 at steps 0,20,40,60,80,100. This is a terminal visualization, not a native surface renderer.

## 2026-11-25 — tetrahedral soft-body dynamics

Added [RuntimeSoftBody.hpp](Runtime/KairoPlayer/RuntimeSoftBody.hpp), fixed-step XPBD compliant edges and tetrahedral signed volume constraints with inverse-mass weighting, pinned vertices, velocity reconstruction, and infinite ground plane. GCC and Clang C++23 -O2 -Wall -Wextra -Werror passed 24,000 seeded drop steps per compiler, plus rest, pinned and invalid input fixtures. Each tested trajectory reproduced exactly within the same compiler, stayed above ground, and retained volume within 5% of rest. Exact tested blobs: 46d2854299ada180ac0a1a865ba6a1cece41acff (header), c26ac9653344703c8a1aa05bdae4c7d0fb70abd4 (tests). No arbitrary mesh, self collision, material calibration, or native deformation presentation claim. The solver is currently a Player-side portable kernel; future PhysicsEngine integration should share the constraint implementation rather than duplicate it.

## 2026-11-25 — native simulation lab candidate

Added [SimulationLab.cpp](Runtime/KairoPlayer/SimulationLab.cpp) with two modes using the actual pinned RendererRuntime API: depth-column water and a dynamic tetrahedron mesh with outward face normals. Shared cube handle, replacement mesh publication before retirement, fixed-step solver calls, bounded smoke comparing native images and numerical invariants, and Linux OpenGL CTest registration. Reviewed against existing Craft/Terrain and pinned RenderScene/Mesh/Renderer interfaces. No native build/capture was run here; the new source is a candidate. Remaining limitations: opaque water columns, small tetra fixture, no native Editor authoring, no general deformable GPU buffer update or physical coupling.

## 2026-11-25 — predetermined rigid fracture

Added [RuntimeBreakableCube.hpp](Runtime/KairoPlayer/RuntimeBreakableCube.hpp), one-shot impulse threshold, eight equal subcubes, mass/volume/center-of-mass and impulse momentum checks, authored separation speed, fixed-step gravity/ground restitution and reset. GCC and Clang C++23 -O2 -Wall -Wextra -Werror each passed 1,000 fracture events and 240,000 body steps. Tested blobs fa733c83c79aa533ebb1dbe1814cb6258106a039 and b6eaa1cc30f8f5768a5992a0f295822ce5649081. Added --destruction mode to SimulationLab, native capture and ground gate (not run here). Predetermined cube topology only; no fragment rotation, inter-fragment collisions, arbitrary mesh fracture or contact-event integration.

## 2026-11-25 — teaching pass: terrain and voxel mesh architecture

Added 40 teaching blocks across five existing runtime headers. Compared all non-comment lines to the preceding exact revision: identical. The explanations cover negative-coordinate floor division, precision rebasing, bounded spawn search, occupancy/material contracts, stable streaming slots, halo ownership, and deterministic greedy meshing.

| File | New explanation blocks |
|---|---:|
| [TerrainRenderOrigin.hpp](Runtime/KairoPlayer/TerrainRenderOrigin.hpp) | 3 |
| [CraftSurfaceSpawn.hpp](Runtime/KairoPlayer/CraftSurfaceSpawn.hpp) | 3 |
| [CraftWorld.hpp](Runtime/KairoPlayer/CraftWorld.hpp) | 12 |
| [CraftStreamingWindow.hpp](Runtime/KairoPlayer/CraftStreamingWindow.hpp) | 12 |
| [VoxelChunkSurface.hpp](Runtime/KairoPlayer/VoxelChunkSurface.hpp) | 10 |

No native build performed for this comment-only change. All-files annotation remains open.

## 2026-11-25 — authored UI document and shipping bridge

Added [RuntimeMenuDocument.hpp](Runtime/KairoPlayer/RuntimeMenuDocument.hpp), bounded canonical menu document, overlap/identity/label/action validation and serialization. GCC and Clang C++23 -Wall -Wextra -Werror each passed 1,000 geometry roundtrips and malformed document fixtures. Tested blob IDs d14bb5d4cad8e8ccdabd156eb8860f633caf32e7 and 4598ccbe6d9d49ddbfddfebacd6aee3fb608ec42. The existing shipping bridge optionally loads Config/Menu.kmenu into RuntimeUIScene and LocalizationCatalog during project validation. Added copied playable sample and headless CTest gate. Native input/overlay adapter remains open and full module build has not run.

## 2026-11-25 — native menu interaction candidate

Added [RuntimeMenuFocus.hpp](Runtime/KairoPlayer/RuntimeMenuFocus.hpp), deterministic wrapping focus, release-to-arm keys, pointer capture/cancellation and bounded bitmap glyphs. GCC/Clang C++23 -Wall -Wextra -Werror each passed 41,600 focus movements plus pointer/latch/alphabet fixtures; exact tested blobs a9c2577a3c5ada694121af3a9803bdd28401e3bd and 8574e3507ae55272730e6e4eafcacefac9b44082. [RuntimeMenuHost.cppm](Runtime/KairoPlayer/RuntimeMenuHost.cppm) connects the existing UI scene to native GLFW input, framebuffer layout, visible focus/labels, larger text and actual resume/quit. Main pauses simulation during menu frames. Vulkan/Metal overlay only, bounded ASCII glyphs, focused title label but no OS accessibility tree, native build/capture not run. Graphical editor design remains open.

## 2026-11-25 — licensed offline asset catalog

Added [catalog CLI](scripts/kairo_asset_catalog.py), strict bounded manifest, searchable metadata, local-path and symlink validation, explicit SPDX allowlist, SHA256 and license evidence, exclusive staging and project-recipe export. Added [generated GLB demo](Samples/AssetCatalog/create_demo_catalog.py) and [regressions](scripts/tests/test_kairo_asset_catalog.py). Three Python test methods passed, including tamper, license, duplicate, traversal, symlink and overwrite rejection. Ran actual generator, search and stage; the 860-byte generated GLB passed the exact existing #106 recipe importer's glb_bytes validation (importer blob ec0e54f491834bf84b242eb302a40a890f708be4). Exact catalog/test/generator blobs 5dfd7abcf425b888b787e51383afedb69ae96487 / 6c04b8bd31eab595e3cee1f3b74fc0fb5bb22710 / 881fcaf240ff126fb3ab99321ac62eb78e61deee. No complete native project import or hosted marketplace; license authenticity is not established by metadata. #106 is still a candidate; retain staged license evidence when distributing a project. Local path checks assume a non-concurrently-mutating catalog.

## 2026-11-25 — explicit native plugin ABI

Added [C ABI](Runtime/KairoPlayer/RuntimePluginABI.h), [loader](Runtime/KairoPlayer/RuntimePluginLoader.hpp), [shared-library extension](Samples/PluginLab/ExamplePlugin.cpp), [CLI host](Samples/PluginLab/PluginLab.cpp), and actual dynamic-loading regression. GCC/Clang built separate good/bad .so modules, and each passed 10,001 action calls, overflow rejection and four bad manifest/ABI fixtures. Initial test filename contained '+' from the compiler name and was rejected by the intended portable basename policy; reran with good-gcc.so/good-clang.so and passed. CLI host returned double-value: 42 for input 21. Exact tested blob IDs: ABI 49c86622b483c8a4b6f39c4430295ce75aff949f, loader 4a112a409c28dabca2fa7146fe790934df24c398, extension 1c1f52ff421cb48f8fb91212ce153de956801c17, tests 8f6e7391385ed80d899e3c61c301593512800325, host 5bc89219e4980cb7bac26a5092946d36cd5e0ba7. Linux only observed; native libraries require explicit trust and are not sandboxed. No editor lifecycle hooks or hot reload. CTest registered.

## 2026-11-25 — teaching pass: shipping and production adapters

Added 23 explicit ownership, input/output, and collaborator comments across two existing module files. Non-comment lines compared identical. Covered shipping state versus full scene state, UI replacement lifetime, replay hashing, mixer/device distinction, optional production manifests, budget validation and streaming observer coordinates. No native build run for the comment-only change.

- [RuntimeShippingBridge.cppm](Runtime/KairoPlayer/RuntimeShippingBridge.cppm): 15 new explanation blocks.
- [RuntimeProductionSystemsBridge.cppm](Runtime/KairoPlayer/RuntimeProductionSystemsBridge.cppm): 8 new explanation blocks.
