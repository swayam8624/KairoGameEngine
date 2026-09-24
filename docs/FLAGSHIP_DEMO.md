# KAIRO v1 Flagship Demo Runbook

This is the reproducible evidence sequence for the flagship
**“I Built an Entire Game Engine From Scratch — KAIRO”** demonstration.

The runbook uses checked-in deterministic projects instead of downloading a
last-minute binary demo dependency.

## 0. Lock and acceptance

```bash
cd KairoGameEngine
bash scripts/sync_workspace.sh
bash scripts/verify_workspace_lock.sh
bash scripts/run_portfolio_acceptance.sh
bash scripts/verify_portfolio_95.sh
```

Capture the zero-warning pass and final CTest summary.

## 1. Imported-content interoperability

Open the shared imported-content project:

```bash
./build/dev-clang/KairoEditor/KairoEditorApp \
  --project Samples/SharedContentShowcase/Project.kproject
```

Evidence to capture:

- glTF/GLB hierarchy and imported meshes;
- texture/material bindings;
- lights/environment and camera;
- viewport selection/debug modes;
- the same project in KairoPlayer.

Run a deterministic native smoke/readback:

```bash
./build/dev-clang/Runtime/KairoPlayer/KairoPlayer \
  Samples/SharedContentShowcase/Project.kproject --smoke
```

## 2. Unified profiler

In the Editor switch to **Profiling** workspace and show **Statistics**.

Capture, in one shot:

- frame milliseconds and UI FPS;
- render-graph total time;
- per-pass renderer timings;
- PhysicsPreview total/broadphase/narrowphase/solver timing;
- active body/collider counts.

The panel consumes completed renderer and physics snapshots; it does not own
those systems.

## 3. Complete game slice

Run Phase1 Arena:

```bash
./build/dev-clang/Samples/Phase1Game/KairoPhase1Game
```

Capture:

- project boot;
- mapped movement/jump input;
- physics interaction;
- collectibles and hazards;
- pause/reset;
- save/load;
- win/loss state;
- shared native renderer path.

Native smoke:

```bash
./build/dev-clang/Samples/Phase1Game/KairoPhase1Game --smoke
```

## 4. Package and relocate

```bash
./build/dev-clang/Samples/Phase1Game/KairoPhase1Game \
  --package Release --replace
```

Show the package manifest and execute the relocated launcher. The package step
must be atomic and validation must happen before publication.

## 5. Reference/offline rendering

From KairoRayTracer:

```bash
./build/KairoRayTracerCLI scenes/cornell.kairo \
  --mode path --passes 32 --output outputs/flagship-path.png
./build/KairoRayTracerCLI scenes/cornell.kairo \
  --mode accel_diff --output outputs/flagship-accel-diff.png
```

Use the beauty/reference image and differential-debug image to explain R6
without claiming the research hypothesis is already proven.

## 6. Production pipeline

Show one bounded diagnostic/publish path from each relevant DCC repository:

- Blender asset preflight + publish;
- Maya SceneDoctor diagnostics;
- Houdini CacheGuard missing/stale range report;
- Nuke ShotDoctor plate/output validation;
- KairoPipelineCore immutable publish manifest and hashes;
- KairoAssets downstream verification/registration.

Native host gates that are unavailable on the recording machine must be labeled
platform-gated rather than simulated.

## 7. Compute/AI

Use machine-readable benchmark output from SIMD/Scheduler/GPU/Transformers and
show KairoAI's plan/approval/receipt boundary. Do not present R5/R7 research
claims as established results until their registered baselines have been run.

## 8. Final portfolio shot

End on:

```bash
bash scripts/verify_portfolio_95.sh
```

and the 27-repository target/acceptance matrix in `docs/V1_95_COMPLETION.md`.

The flagship video should distinguish three labels on screen:

- **TARGET 95 / ACCEPTED ONLY WITH MATCHING EVIDENCE**
- **VERIFIED ON THIS HOST**
- **PLATFORM-GATED / POST-v1**

That keeps the demonstration technically impressive without overstating
evidence.
