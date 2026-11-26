# M50 — no feature below 50%

The first product milestone is a functional vertical slice for **every**
feature in the previous completion assessment, including the least mature
areas. This is not the legacy 95% portfolio target. Percentages are
engineering rubric levels, not code coverage or measured market parity.

| Level | Required evidence |
|---|---|
| 0 | No implementation |
| 25 | Isolated design or prototype, with known limitations |
| 50 | A documented reusable implementation, a runnable narrow end-to-end use case, and passing relevant tests at the exact accepted head |
| 75 | Authored workflow, persistence and native integration across supported hosts, with failure handling |
| 100 | Production polish, compatibility, performance, failure handling and release evidence against a frozen scope |

No feature can be promoted to 50 solely because an API or PR exists. Existing
features previously estimated above 50 must be re-audited against this
rubric; do not regress or hide them in an average.

## Initial below-floor queue

| Area | Owner | Gate for 50 |
|---|---|---|
| Advanced graphics effects | Renderer | A parameterized effect runs through the real frame graph and passes a native capture. |
| Soft bodies | PhysicsEngine | A deformable mesh with constraints, contacts and reproducible integration tests. |
| Fluids | EngineCore / Renderer | An authored fluid fixture advances, renders and has conservation/bounds checks. |
| Destruction | PhysicsEngine / Player | A breakable rigid object creates bounded fragments with contact and reset tests. |
| Water/oceans | Renderer / Player | An authored water surface with motion, basic shading and native capture. |
| Weather | EngineCore / Player / Renderer | Seeded weather transitions change actual scene lighting/precipitation in a playable fixture. |
| UI designer | Editor / Player | Author a menu, save/load it, interact using pointer and keyboard in Player. |
| Quest/dialogue | EngineCore / Player | Branching dialogue and prerequisite objectives execute with state save/reload in a sample. |
| Inverse kinematics | EngineCore | Two-bone target solver with reachable/unreachable targets and pose demonstration. |
| Animation retargeting | EngineCore / Assets | Map two compatible skeletons and play a clip with rest-pose handling and oracle tests. |
| Accessibility | Editor / Player | Keyboard-only authored menu, focus, text scaling and meaningful labels verified in game. |
| Network transport | EngineCore / Player | Two local processes exchange bounded versioned packets with malformed input tests. |
| Client-server multiplayer | EngineCore / Player | Authoritative host and one client exchange inputs/state in a playable sample. |
| Multiplayer synchronization | EngineCore / Player | Sequence/tick validation, snapshots and reconciliation under simulated latency/loss. |
| Matchmaking | Player / networking | Local session directory with create/list/join, expiry and capacity tests; no hosted service claim. |
| Asset marketplace | Editor / Assets | Local or public licensed catalog, provenance, import and explicit permissions; no paid marketplace claim. |
| Plugin ecosystem | Editor / EngineCore | Versioned plugin manifest and one loadable extension with incompatible-version rejection. |
| Standalone installers | GameEngine / Hub | Build a host-native installable package and validate on a fresh test environment. |
| Mobile deployment | GameEngine / Renderer | Build and run one supported mobile target with input and packaging smoke. |
| Web deployment | GameEngine / Renderer | Browser-compatible sample build with rendering and input smoke. |
| Release certification | GameEngine | Exact pinned workspace builds, tests and native packaging gates pass with saved artifacts. |
| RPG demonstration | Samples / Player | Complete short playable quest loop with dialogue, inventory and completion. |
| Platformer demonstration | Samples / Player | Playable start-to-finish level with jumps, hazards, respawn and goal. |
| Strategy demonstration | Samples / Player | Playable select/order/resource/goal loop using reusable engine systems. |

## Execution order

1. Runtime fundamentals: reusable quest/dialogue, IK/retargeting, plugin and network contracts.
2. Physics/environment: soft bodies, fluids, destruction, weather, water, effects.
3. Authoring: UI designer, accessibility and licensed asset catalog.
4. Multiplayer: transport, authoritative client/server, synchronization, local matchmaking.
5. Examples: RPG, platformer and strategy vertical slices.
6. Distribution: host installers, mobile/web experiments, exact-head certification.

Work may proceed independently where dependencies permit. Merge and pin
component revisions only after observed CI/host evidence. Platform gates
require the actual platform; an unsupported host is a blocker, not a pass.
Every change updates the devlog and study order, with code comments and tests
in the same PR. Update this queue with measured evidence, not optimistic
percentages.

## Candidate progress: quest/dialogue

The reusable rules engine, branching terminal example and regression fixtures are present on the candidate branch. M50 remains open until runtime sample integration, disk persistence and relevant exact-head tests pass.

## Candidate progress: two-bone IK

Analytic kernel, CLI pose lab and portable tests exist. 10,000 seeded cases passed under both GCC and Clang. Rendered pose integration remains open; no 50% promotion until the demonstration gate is met.

## Candidate progress: quest persistence

Disk save/load and cross-process terminal completion now pass locally under the documented narrow CPU scope. Native graphical authored sample and accepted workspace integration remain open.

## Candidate progress: animation retargeting

Compatible named skeleton CPU mapping and sampled synthetic clip demo exist; 5,000 seeded poses pass per compiler. Imported source/target fixture and graphical Player integration remain open. No claim of arbitrary humanoid compatibility.

## Candidate progress: network transport and authority

Real two-process loopback exchange and 10,000 seeded authority ticks per compiler passed on Linux. This covers the narrow local transport scenario, but accepted workspace CI, graphical game integration, reconciliation under delay/loss and matchmaking remain open. No WAN or multiplayer readiness claim.

## Candidate progress: network synchronization

Client prediction and reconciliation now run in the real loopback sample and pass 64,000 impaired-snapshot ticks per compiler. The impairment model drops, delays and reorders snapshots while inputs reach the authority reliably and in order. Full game state, real network loss and multi-actor authority remain open.

## Candidate progress: weather

Seeded weather timeline and renderer-neutral light modulation pass portable tests. The streamed Craft Player now opts into actual lighting changes with --weather. Native acceptance and visible rain remain open; no 50% promotion yet.

Visible precipitation draw submission and a native capture/removal gate are now implemented on the candidate branch. Portable geometry tests pass; native smoke remains pending, so no accepted 50% promotion.

## Candidate progress: water flow

Bounded depth-only grid and seeded conservation tests pass on two compilers. A terrain basin terminal sample is present. Actual rendered water, waves, momentum and authored integration are not established; the water/ocean and fluid gates remain open.

## Candidate progress: soft bodies

XPBD tetrahedral edge/volume constraints and ground projection pass 24,000 seeded drop steps per compiler. Reusable CPU kernel is present; real mesh presentation, multi-element contact acceptance and native integration remain open. No 50% promotion.

## Candidate progress: native water and soft body presentation

A dedicated native renderer lab now submits water depth columns and a deformed tetrahedral surface, with numerical and image-change smoke gates. Native CI has not yet run; 50% acceptance remains pending. This is not an ocean shader or general soft-body authoring system.

## Candidate progress: destruction

Predetermined rigid cube fracture, bounded fragments and ground response passed 1,000 seeded fracture events plus 240,000 body steps per compiler. Native lab and smoke registered but not run; authored physics contact and arbitrary mesh support remain open.

## Candidate progress: authored UI

Canonical button document and shipping bridge loading implemented, with 1,000 CPU roundtrips per compiler. Sample and headless validation registered. Native interaction, visual labels, scaling, and graphical designer still open; no 50% promotion.

Native menu host candidate now supports pointer/keyboard input, focus, bounded labels and a larger-text toggle through existing RuntimeUIScene and Vulkan/Metal overlay APIs. Portable focus tests pass; native execution, graphical editor designer and OS accessibility integration remain open.

Local licensed asset catalog, search, staging, checksums, retained license evidence and recipe export implemented and locally tested. Generated model passes existing recipe candidate's GLB validation. Full native import and graphical catalog browser remain open; no hosted marketplace claim.

Native plugin v1 manifest, shared-library ABI, explicitly trusted CLI extension and incompatible-version rejection pass real Linux dynamic loading tests. Editor/runtime extension lifecycle, macOS/Windows acceptance and full workspace CI remain open. Narrow plugin lab is not a complete ecosystem or sandbox.

## Candidate progress: local matchmaking

Local create/list/join/leave/close, expiry and capacity checks pass, including a 24-process race and actual discovery-to-native-connection demo. Narrow local functional gate demonstrated; accepted workspace CI and server-side token enforcement remain open. No remote service claim.

## Candidate progress: platformer

Complete playable terminal route with movement, jump, hazard, respawn, collectibles and goal. Both compilers passed end-to-end and randomized tests. Native presentation and workspace acceptance pending; do not confuse this fixed-grid demo with a general 2D editor.

## Candidate progress: strategy

Playable select/move/gather/deposit loop reaches six-resource goal through a wall-and-gap map. Portable tests pass both compilers. Native rendering, editor-authored maps, general pathfinding integration and workspace CI remain open.

## Candidate progress: native RPG, platformer and strategy host

Common native 2D overlay host, controls and full objective smoke gates added. Pixel layout tested; native build/capture and full workspace acceptance still pending. No 50% promotion without that evidence.

## Candidate progress: independent portable certification

Separate Linux dual-compiler acceptance gate with exact source fingerprints, regression runs and retained evidence added. This narrows the feedback loop while full workspace CI is pending; it does not certify the renderer, Editor or deployment platforms.

## Candidate progress: Linux installer

Actual Debian archive generation and relocated native ELF fixture execution pass. Wrapper accepts the existing Kairo release package format; full native player bundle on a clean Linux environment, dependencies and cross-platform distribution remain open. No accepted 50% promotion yet.

## Candidate progress: web deployment

Actual freestanding C++ WASM build and 20,279 native/WASM state/map comparisons passed. Static responsive Canvas host and browser acceptance fixture added. Browser execution was blocked/timed out locally, and the native GPU renderer/general exporter/mobile-native deployment are outside this slice. No accepted 50% promotion pending browser and workspace evidence.

## Post-merge integration status (2026-11-26)

The previous implementation stack is merged into main. Follow-up fixes isolate modal gameplay input, use identical pixel hit/draw bounds, test native sample rectangle generation, repair rain mesh teardown, and reuse bounded dynamic mesh publication. [Expanded portable evidence](evidence/m50-portable-integration-2026-11-26.json) proves 28 execution gates per compiler at 58 exact source blobs. The [native acceptance runner](M50_NATIVE_ACCEPTANCE.md) requires six actual Vulkan/Metal sample gates and rejects skips; native CI execution is still pending. This is not a percentage promotion. Mobile native packaging, graphical editor authoring, native animation integration, real GPU capture and full release acceptance remain open.

## Candidate progress: native Android deployment

Single shared-core Android game, native JNI, touch/lifecycle and APK/emulator gate implemented; actual Linux JVM/JNI tests pass both compilers. Device/APK acceptance pending; iOS and full mobile engine renderer/exporter remain open. No accepted 50% promotion.

## Candidate progress: graphical menu authoring

Standalone Tk graphical designer consumes the exact native C++ parser/serializer and writes the existing menu format. GUI drag/edit/undo/add/delete/save and conflict tests passed on real Tk/Xvfb with both compiled validators. Main-editor integration and native runtime acceptance remain separate.

## Candidate progress: RPG item loop

The graphical RPG fixture now owns a real crystal and gold inventory with transactional dialogue rewards, persisted counts and legacy save migration. Both portable compilers passed 50,000 item transactions and 20,000 combined quest transitions each; updated native smoke and painter tests registered, pending execution. No promotion until relevant accepted-head gates pass.

## Candidate progress: animated skeleton diagnostic

Local rotation adapter and FK reconstruct 20,000 solved 3D chains per compiler; native animation mode now presents source, retargeted and IK-corrected poses, with 5,000 portable motion/layout tests per compiler and seventh required native capture gate. Native verification and imported skinned rigs remain open. No accepted promotion yet.

## Candidate progress: catalog release acceptance

Exact payload and provenance verifier now integrates the recipe output, release packaging, relocated launch and extracted Debian launcher. Four local real archive/ELF fixture tests pass; actual generated-game Vulkan release CI is registered and pending. Fresh OS dependency/install certification remains open.

## Candidate progress: authoritative two-client co-op

Both local compilers passed 95,825 randomized simultaneous turns each and an actual three-process shared victory plus four failure-injection cases. Graphical integration, accepted-head CI and admission enforcement remain open. Not an Internet multiplayer service.

## Candidate progress: graphical co-op

Two-client asynchronous worker, keyboard/overlay host and independent native three-process capture gate added. Both portable compilers passed real worker network/cancellation/failure tests and 5,000 viewport cases each. Native CI and accepted-head gate pending.

## Candidate progress: imported character IK

Imported glTF node correction now feeds the real shared skin palette extraction and runtime controls. Both local compilers passed 10,000 structural adapter matrix reconstructions each; actual artifact/module regression registered, awaiting full workspace CI. No imported native GPU capture or accepted promotion yet.

## Candidate progress: file-backed native skin acceptance

Original self-contained GLB fixture and actual imported skinned mesh/clip/IK capture/pick gate registered as ninth strict native sample. Local byte tests passed; actual native importer/build/render acceptance remains pending. No promotion until accepted-head native results.

## Candidate progress: protected co-op admission

Optional distinct 256-bit seat tickets bind protected local connections to their assigned seats rather than connection order. Wrong, malformed, truncated and replayed credentials are rejected before a seat is consumed. Portable process and native graphical gates now exercise protected admission. CI execution is required before promotion; this is not a hosted service, encrypted transport, or automatic matchmaking-directory token validation.

## Candidate progress: graphical licensed catalog

A standalone Tk catalog browser now reuses the canonical local manifest, search, checksum, license evidence and recipe staging code. It displays reviewed metadata, filters selectable assets, and stages asynchronously into a new directory; metadata changes between review and staging reject. Actual Tk/Xvfb tests are registered for search, selection, provenance, overwrite refusal and stale metadata. Main-editor import, actual native model preview, and CI execution remain open.

## Candidate progress: protected directory-to-game workflow

The SQLite directory now issues the actual protected server tickets atomically, never recycles a claimed seat within a match, and exposes a host/list/join launcher for terminal or graphical clients. The server allows a bounded interactive lobby window. Portable tests exercise 24-process contention, expiry, direct directory-to-native connection, and the actual launcher command flow. CI and native graphical execution are pending; no external matchmaking or percentage promotion.

## Candidate progress: catalog-to-Editor project creation

The graphical catalog and CLI now share a direct new-project operation using isolated verified staging and the canonical recipe compiler. The browser can open the generated Project.kproject in an explicitly configured KairoEditorApp. Tk tests cover project output, exact model/license evidence, destination refusal and the editor argument boundary; native release CI uses the same creation operation. Actual native Editor execution and in-project asset import remain open.

## Candidate progress: generic collectible objectives

Restacked older #108 on current main. The ordinary Player recognizes opt-in static visual-only leaf collectibles, reports score/time/completion, and hides collected entities. HUD publication is deferred during modal menu frames and refreshed on resume. Exact tested core/test Git blobs `f5d09f452e08680bb0d00eba33f7be632a011601` and `369db57ab97862a1ff0e479d61f8220540b5a73b` passed GCC, Clang and Clang ASan/UBSan, each with 1,000 seeded runs and a 4,096-goal batch. Native CI validates the six-goal project, checks one goal collected at spawn and a nonuniform native capture, and rejects a tagged rigid-body player. The native gate is pending, not evidence of a complete playthrough. No moving collectible, arbitrary trigger graph, persistence or percentage promotion claim.

## Candidate progress: authored proximity triggers

Added bounded data-only sphere rules with deterministic ascending-ID enter/exit/once events and explicit visual show/hide effects. Generic Player validates opted-in static root visual leaves at load time, excludes incompatible component roles, and pauses sampling during modal menu frames. Trigger Arena demonstrates reversible visual interactions. Exact tested Git blobs `e13c402650d77971bdc11ebf974155b4545f1493` (core) and `f330a410a29be6e0564429d222c103f6d7fbe73d` (tests) passed strict C++23 GCC, Clang and Clang ASan/UBSan, each with 50,000 randomized samples and rejection cases. Actual native Player entry/capture/invalid-target CI gate registered, not run locally. No physics door, swept collision, trigger persistence, arbitrary scripting or accepted percentage promotion claim.

## Candidate progress: persistent objective and trigger checkpoint

Explicit `--progress-file` stores collected IDs, elapsed time, trigger inside/fired flags and visual enable states outside the authored project. Restore validates the authored descriptor/scene/trigger byte fingerprint, exact IDs and bounded payload into copies before publishing. Reuses the existing atomic local file replacement helper. Local strict GCC, Clang and Clang ASan/UBSan each passed three separate OS process phases (save, restore, reject), including final-state cold reload, source fingerprint change and blocked writer preserving bytes. Tested blobs: `5a5911a39ff947af80b18a90815b256526c77dd7`, `11418bb569c4b6e36b98f14368fbab0268400595`, `975c86ddb8b7aec350ffe3ebc5aa50b0e4accab2`, `e4cb2b90cdb3c6b50694dbd8e71dbcc62d6b93a4` and existing file helper `27c83cd0a478a99326c355ad816833e0ea9a4919`. Two-launch actual native Player gate registered, not executed locally. This is objective progress, not whole-world physics, autosave, cryptographic authentication, fsync durability or a percentage promotion.

## Candidate progress: physics-backed sliding doors

Versioned door rules bind opted-in kinematic box mesh roots to the generic Player fixed-step fanout. Vertical movement follows horizontal primary capsule-player proximity and requires a conservative activation radius. Existing physics bridge updates scene and body poses, retaining the same body identity. Exact portable Git blobs `2811b8978a642ffdf7d0caa4df3009a9d2b1ad9d` (controller), `3c120f1bbc13c324ae3f055ea8e704fae166c0a0` (tests) and dependency `11418bb569c4b6e36b98f14368fbab0268400595` passed strict GCC, Clang and Clang ASan/UBSan, each with 50,000 randomized motion samples. Door Arena and actual CTest raycast/body/scene round-trip registered; full native workspace execution pending. No swept collision, arbitrary moving-body obstruction detection, crush-safety, persisted door motion or accepted percentage promotion claim.

## Candidate progress: health, hazards and respawn

A versioned opt-in health document provides bounded integer health, fixed-tick damage cooldowns, sampled sphere hazards, fall death and checkpoint selection. The generic Player samples after character movement, resets motor state through its existing Teleport API, and composes health/objective bars into one modal-safe HUD. Exact portable Git blobs `db8b2320f85edd30f49168b74959424ebbeb185b` and `8a7370497bcd2f265a923304199388dffe990292` passed strict GCC, Clang and Clang ASan/UBSan, each with 50,000 seeded state samples plus invalid-input cases. Health Arena, actual motor/body/scene CTest and native Vulkan capture registered; full workspace execution pending. Health and checkpoint selection are session-local; no swept zone crossing, solid-geometry clearance proof, persistent health, death animation or accepted percentage promotion claim.

## Candidate progress: raycast combat

Opt-in authored combat rules define damage, fixed-tick cooldown, range and health for static destructible roots. The generic Player latches an Attack edge, retains movement aim, uses actual physics raycasts with self-collider exclusion, ignores trigger hits and removes a destroyed target's active physics body and visual state. Exact Git blobs `74fc02c34ee07dded2dc6c0c32c7d515396999b9` and `b2f9b8bdb1a1d43c72c6a46e2584471547019876` passed strict GCC, Clang and Clang ASan/UBSan each with 50,000 seeded samples. Actual physics CTest registered, not executed locally. No enemy AI, projectiles, animation, full-world save or native acceptance claim.

## Candidate progress: travelling projectile attacks

Optional projectile speed extends authored combat without changing default hitscan rules. Bounded point flights trace the entire travelled segment with existing physics raycasts, apply impacts in stable order, expire by range and clear on respawn. Portable flight tests (10,000 seeded journeys), prior combat regression (50,000 seeded samples), and combined delayed-damage/parser tests passed strict GCC, Clang and Clang ASan/UBSan. Exact blobs: `d784f0b9eda8dc2615e31b2f9c632012df7dd9d5`, `4b34812fd1a2450d0e28efd55de572340cc285a7`, `1699b6db0d69fd41dcd15396c6b8d49821f820ee`, `b2f9b8bdb1a1d43c72c6a46e2584471547019876`, `d36b602a3816888c8222fed7d8667b5a8c800786`. Actual physics bridge CTest registered, not executed locally. Console launch/impact feedback only; no rendered mesh/trail, ballistics, moving-target sweep guarantee or accepted overall percentage promotion.
