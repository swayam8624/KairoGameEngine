# Teaching source map — KairoGameEngine

> A guided source-reading itinerary. It is not a test result or claim that every file is documented yet.

## How to use this guide

KairoGameEngine is a sibling-repository CMake superbuild. Read integration contracts first; each component implementation lives in another repository.

Read each phase in order. Every link goes to a source file or directory in this repository. For a directory, inspect its public interface and then its implementation files. After each phase, identify the caller, the state it owns, the inputs it validates, and its observable outputs.

## 1. Trust and bootstrap

1. [`README.md`](../README.md)
2. [`STATUS.md`](../STATUS.md)
3. [`STATUS.yaml`](../STATUS.yaml)
4. [`workspace.lock`](../workspace.lock)
5. [`CMakeLists.txt`](../CMakeLists.txt)
6. [`CMakePresets.json`](../CMakePresets.json)
7. [`scripts/bootstrap_workspace.sh`](../scripts/bootstrap_workspace.sh)

## 2. Architecture and acceptance

1. [`docs/DEVELOPMENT_MAP.md`](../docs/DEVELOPMENT_MAP.md)
2. [`docs/V1_95_COMPLETION.md`](../docs/V1_95_COMPLETION.md)
3. [`docs/status/exact-head-ci-audit.md`](../docs/status/exact-head-ci-audit.md)
4. [`scripts/run_portfolio_acceptance.sh`](../scripts/run_portfolio_acceptance.sh)
5. [`scripts/verify_portfolio_95.sh`](../scripts/verify_portfolio_95.sh)

## 3. Runtime and gameplay

1. [`Runtime`](../Runtime)
2. [`Samples/SharedContentShowcase`](../Samples/SharedContentShowcase)
3. [`Samples/Phase1Game`](../Samples/Phase1Game)
4. [`Samples/PlayableVerticalSlice`](../Samples/PlayableVerticalSlice)
5. [`Samples/KairoRacingGame`](../Samples/KairoRacingGame)

## 4. Integration tests and CI

1. [`tests/SharedContentContractTests.cpp`](../tests/SharedContentContractTests.cpp)
2. [`tests/SceneECSBridgeTests.cpp`](../tests/SceneECSBridgeTests.cpp)
3. [`tests/CompatibilityContentTests.cpp`](../tests/CompatibilityContentTests.cpp)
4. [`tests/ComputeStackIntegration.cpp`](../tests/ComputeStackIntegration.cpp)
5. [`.github/workflows/build-test.yml`](../.github/workflows/build-test.yml)
6. [`.github/workflows/portfolio-ci-audit.yml`](../.github/workflows/portfolio-ci-audit.yml)

## End-to-end interaction map

`workspace.lock` selects exact component SHAs; CMake wires sibling targets. Assets/Scene descriptors are read by EngineCore and adapted to KairoRenderer and KairoPhysicsEngine. KairoEditor authors the same contract the Player runs. KairoHub launches validated projects. Cross-repository changes must be tested at their locked heads, not merely at independent main branches.

## APIs and code-reading questions

For each function, distinguish **contract** (preconditions, postconditions, failure modes), **implementation** (state/algorithms), **collaborators** (imports, calls and consumers), **lifetime/ownership**, and **verification** (test or evidence that checks it). For C++ modules, `export module` names the public compiled interface and `import` expresses module dependencies; an implementation unit's `module ...;` contributes definitions without exporting them.

## Boundaries and honesty

The 95% figure is an **unverified target**, not completion. Weekly CI audit without `--strict` does not certify the portfolio. Native host and packaging gates remain separate.

## Documentation rules

- Teaching comments describe *why*, invariants, edge cases, API ownership and cross-file effects; avoid narrating obvious assignments.
- Preserve behavior during documentation-only edits; keep correctness changes in separate reviewed commits.
- Link function names and tests where possible; mark claims as source-present, CI-observed, host-verified or research-evidenced.
- License grants apply only to material for which the repository owner holds rights; third-party source, models, media and datasets keep their own notices.
