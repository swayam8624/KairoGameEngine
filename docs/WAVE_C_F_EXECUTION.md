# KAIRO Waves C–F Execution

This document replaces percentage-only progress with executable evidence.

## Wave C — scale and failure resistance

Wave C is accepted only when the normal component CI includes and passes:

- KairoSpatial: thousands of primitives, accelerated queries checked against brute force, dynamic-tree update validity.
- KairoPhysicsEngine: repeated stress simulation produces finite state and deterministic final transforms.
- KairoAssets: hundreds of immutable content-addressed DDC entries survive write/read validation.
- KairoRenderer: a long transient render-graph chain demonstrates bounded alias-slot capacity and executes the full schedule.
- KairoEngineCore: thousands of streaming cells remain within committed-cell/byte budgets and repeat deterministically.

These are regression gates. Performance claims require benchmark evidence rather than wall-clock assertions inside correctness tests.

## Wave D — end-to-end authoring

Command:

```bash
bash scripts/run_authoring_workflow.sh
```

The campaign proves the checked-out workspace can:

1. validate the starter project;
2. run the shared-content project;
3. run the Phase1 native game smoke;
4. build and test KairoHub;
5. import a real external GLB into a generated Kairo project;
6. validate and smoke the generated project;
7. package the Phase1 release artifact; and
8. run the real Blender publish tests on macOS.

Evidence is written to `build/wave-d-authoring/accepted.env`.

## Wave E — integrated compute

Command:

```bash
bash scripts/run_compute_stack_campaign.sh
```

The umbrella integration workload feeds deterministic data through Scheduler → SIMD → optional Metal GPU → ONNX and exercises Transformer numerical primitives in the same process. The campaign also runs each subsystem's deeper benchmark and writes JSON evidence to `build/wave-e-compute/`.

No performance threshold is inferred from another machine. Numerical correctness is mandatory; throughput is recorded for comparison.

## Wave F — flagship proof

Command:

```bash
bash scripts/run_flagship_campaign.sh
```

This is the highest-level KAIRO proof. It first re-establishes exact-head portfolio acceptance, then runs Wave D and Wave E, and finally writes `build/flagship-evidence/manifest.json`.

A maker video should be recorded only from a revision that passes this campaign.

## Platform wording

Use these labels exactly:

- **Verified on this host** — the gate executed successfully here.
- **Platform-gated** — implementation exists but the required native host was not tested here.
- **Measured** — a benchmark JSON exists for the exact revision.
- **Research hypothesis** — not yet established by the registered baseline/ablation campaign.

A green Mac campaign is not a Windows or Linux certification.
