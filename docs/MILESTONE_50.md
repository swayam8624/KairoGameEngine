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
