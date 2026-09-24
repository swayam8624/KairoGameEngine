# KAIRO Maker Recording Runbook

Use this only after the current revision passes `bash scripts/run_flagship_campaign.sh`.

## Flagship episode

Record in this order so the viewer sees a working product before architecture diagrams.

1. **Cold open — finished game/runtime.** Launch Phase1 and show movement, physics, hazards, save/load and the native renderer.
2. **Editor proof.** Open the shared-content project, move through hierarchy/viewport/debug views, then run the same scene in Player.
3. **External content proof.** Use the Hub external-import flow on a GLB, then validate and smoke the generated Kairo project.
4. **Engine internals.** Show render-graph pass profile, physics profile, spatial debug structures, streaming state and resource statistics.
5. **Compute stack.** Put the JSON from `build/wave-e-compute/summary.json` on screen; show measured SIMD/GPU/transformer values without extrapolating across hardware.
6. **Production path.** Run Blender validation/publish and show the immutable publish result.
7. **Packaging.** Package Phase1 Release, relocate it, and launch the packaged result.
8. **Architecture.** Only now show the repository/system diagram and explain how the pieces connect.
9. **Research.** Present R1–R7 as hypotheses unless a registered campaign has produced comparative evidence.
10. **End proof.** Show `verify_portfolio_95.sh` plus `build/flagship-evidence/manifest.json`.

## Capture set

Keep these assets for editing:

- clean gameplay footage;
- editor viewport + profiler;
- external import before/after;
- render graph and spatial/physics debug captures;
- Blender publish result;
- packaged runtime launch;
- terminal clips of Wave C/D/E/F PASS lines;
- machine-readable JSON result screens;
- architecture graphic;
- failure-case clip for at least one subsystem.

## Per-repository videos

Each repo episode follows the same evidence structure:

**problem → implementation → one failure/edge case → benchmark or correctness oracle → integration point → exact revision**

Do not spend a video enumerating classes. Demonstrate a behavior and then explain the minimum architecture needed to understand it.

## Claims on screen

Use:
- **Measured on this machine** for benchmark values.
- **Verified on this host** for executed native gates.
- **Platform-gated** for unexecuted native platforms.
- **Research hypothesis** until baseline/ablation/falsification evidence exists.

Never turn a benchmark from one host into a universal performance claim.
