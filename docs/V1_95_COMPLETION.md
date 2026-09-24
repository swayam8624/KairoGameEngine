# KAIRO v1 — 95% Source-Complete Portfolio

Status date: 2026-09-24.

This document is the final v1 scope contract. **95% means the repository's
frozen v1 implementation, integration contract, tests/diagnostics surface and
documentation are source-complete.** It does not mean that every native host has
executed the exact head. Native execution remains a separate release-evidence
field in each repository's `STATUS.yaml`.

## Waves

| Wave | Repositories | Frozen v1 result |
| --- | --- | --- |
| A Foundation / DCC base | KairoMath, KairoGeometry, KairoPhysicsMath, KairoPipelineCore, KairoBlender | 95% |
| B Runtime infrastructure | KairoECS, KairoReflection, KairoScheduler, KairoGPU | 95% |
| C Engine systems | KairoSpatial, KairoPhysicsEngine, KairoAssets, KairoRenderer, KairoEngineCore, KairoRayTracer | 95% |
| D Authoring / production | KairoEditor, KairoProductionTools, KairoHub, KairoHoudini, KairoMaya, KairoNuke | 95% |
| E Compute / AI | KairoSIMD, KairoONNX, KairoTransformers, KairoAI, KairoMacPerception | 95% |
| F Integration / release evidence | KairoGameEngine | 95% |

Machine-check the complete portfolio with:

```bash
bash scripts/verify_portfolio_95.sh
```

Run the host-available verification campaign with:

```bash
bash scripts/run_portfolio_acceptance.sh
```

## Completion definition

A repo may be 95 only when all of the following are true for its frozen v1:

- the primary public contract exists and is bounded;
- known unsafe or unsupported behavior fails explicitly rather than silently;
- deterministic/host-neutral tests exist where the repository can support them;
- native-only behavior has an explicit native release gate;
- diagnostics or machine-readable benchmark evidence exist for performance-sensitive code;
- documentation states what is v1 and what is intentionally post-v1;
- no future roadmap item is counted as missing v1 work merely because it could be useful later.

The remaining five percent is release evidence, platform breadth, long-run
performance history and post-v1 expansion. It is **not** a hidden feature list.

## Wave F evidence

KairoGameEngine owns the integrated acceptance story:

1. one physical checkout per repository through the sibling workspace;
2. exact integration revisions through `workspace.lock`;
3. fail-fast zero-warning configure/build before CTest;
4. `SharedContentShowcase` as the imported glTF/material/texture interoperability fixture;
5. `Phase1Game` as the complete gameplay/package fixture: input, physics,
   collectibles, hazards, save/load, win/loss and native rendering;
6. the Editor Profiling workspace as a unified live telemetry surface for frame,
   renderer pass and physics phase timing;
7. project validation, native smoke, screenshot/readback and runtime package tests;
8. source-status verification across all 27 repositories;
9. research ownership and falsification rules in `docs/RESEARCH_TRACKS.md`;
10. the public maker/tutorial sequence in `docs/MAKER_VIDEO_PLAYLIST.md`.

## Post-v1 rule

Consoles/mobile shipping, production networking, full ONNX compatibility,
arbitrary OS automation, Unreal/Unity/DCC feature parity, distributed rendering,
large-model serving, full cloth/fluid/destruction stacks, and platform
signing/installers are post-v1 programs. They must not reopen the v1 completion
score.

Individual repositories list their exact exclusions in `STATUS.yaml`.
