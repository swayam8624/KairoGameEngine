# Kairo Development Map

Status date: 2026-09-24.

KAIRO v1 is now frozen at **95% source-complete across all 27 repositories**.
This map distinguishes that engineering/source milestone from exact-head native
execution. The source milestone does not imply that every platform-gated smoke
has run on every supported host.

## Evidence levels

| State | Meaning |
| --- | --- |
| Source-complete 95 | Frozen v1 implementation/contract/tests/docs are complete |
| Verified | The relevant exact-head gate executed successfully on a supported host |
| Platform-gated | Source exists but requires another native host/license/device |
| Post-v1 | Explicitly excluded from the frozen v1 score |

The machine-readable authority is each repo's `STATUS.yaml`. Validate the
complete portfolio with `scripts/verify_portfolio_95.sh`.

## Waves

| Wave | Repositories | Source state |
| --- | --- | --- |
| A | Math, Geometry, PhysicsMath, PipelineCore, Blender | 95 |
| B | ECS, Reflection, Scheduler, GPU | 95 |
| C | Spatial, PhysicsEngine, Assets, Renderer, EngineCore, RayTracer | 95 |
| D | Editor, ProductionTools, Hub, Houdini, Maya, Nuke | 95 |
| E | SIMD, ONNX, Transformers, AI, MacPerception | 95 |
| F | GameEngine integration/release evidence | 95 |

See `docs/V1_95_COMPLETION.md` for the exact scope and post-v1 rule.

## Integrated v1 acceptance

KairoGameEngine owns the cross-repo evidence path:

- sibling-repository workspace with no duplicate component worktrees;
- exact component revisions in `workspace.lock`;
- zero-warning fail-fast configure/build/test flow;
- shared asset/scene semantics between Editor and Player;
- imported glTF/material/texture evidence in `SharedContentShowcase`;
- complete gameplay/package evidence in `Phase1Game`;
- live Editor profiling of frame time, render-graph passes and physics phases;
- project validation, native smoke, viewport readback/screenshot and package checks;
- portfolio-wide source-status verification;
- optional compute/ML standalone gates;
- host-neutral production-pipeline tests and native DCC gates when available.

Run the available-host campaign with:

```bash
bash scripts/run_portfolio_acceptance.sh
```

A skipped native gate remains platform-gated; it is never promoted to verified.

## Research

Research is not used to inflate engineering completion. R1-R7 remain
falsifiable, unproven research programs with explicit baselines and metrics.
See `docs/RESEARCH_TRACKS.md`.

## Public demonstration

The v1 public story uses two deterministic samples instead of introducing a
last-minute external binary dependency:

- `Samples/SharedContentShowcase`: imported glTF/GLB hierarchy, materials,
  textures and shared rendering semantics;
- `Samples/Phase1Game`: input, physics, gameplay state, collectibles, hazards,
  save/load, win/loss, rendering, smoke and package acceptance.

The Editor's Profiling workspace provides the live systems HUD used by the
flagship demonstration.

## Post-v1 programs

These do not reopen the v1 score:

- console/mobile shipping;
- production networking/live services;
- platform signing/notarization/installers;
- full ONNX/operator/dtype parity;
- distributed/large-model inference;
- arbitrary OS automation;
- cloth/fluid/destruction feature families;
- Unreal/Unity/DCC feature parity;
- advanced renderer scheduling/shadow features beyond the frozen contract.

The maker/tutorial rollout is fixed in `docs/MAKER_VIDEO_PLAYLIST.md`.
