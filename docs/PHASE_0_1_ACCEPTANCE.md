# Phase 0 and Phase 1 Acceptance

This document records the release gates implemented by the Phase 0 stabilization
and Phase 1 gameplay vertical-slice pull request.

## Phase 0: stabilized integration baseline

The integration branch must pass:

- Ubuntu Clang full superbuild, including KairoEditor;
- macOS Clang full superbuild, including KairoEditor;
- Ubuntu AddressSanitizer and UndefinedBehaviorSanitizer runtime build and tests;
- Windows MSVC runtime build and tests;
- recursive submodule checkout and exact component pins;
- source-package generation on Ubuntu.

Windows currently validates the runtime stack rather than KairoEditor because
MSVC 19.44 encounters an internal compiler error in an editor module. The same
editor revision remains compiled and tested by the Ubuntu and macOS Clang lanes.

The Phase 0 component fixes cover native Windows checkpoint paths, portable
tensor-shape comparisons, exact vector equality across C++ module boundaries,
and direct standard-library declarations in the viewport controller.

## Phase 1: core gameplay vertical slice

`Samples/Phase1Game` must provide one complete authored-project-to-package path:

- project descriptor, asset manifest, input map, and Scene V2 loading;
- keyboard and standardized gamepad movement;
- fixed-step rigid-body movement and jumping;
- tagged collectibles, goal, hazard, reset, win, and loss states;
- menu and pause states that freeze simulation;
- versioned atomic save/load;
- Vulkan render submission and nonblank native-frame smoke capture;
- deterministic game-state tests;
- project validation;
- isolated packaging and relocated-manifest validation.

The phase deliberately does not claim runtime audio or a complete game UI. The
native window title is the temporary status surface until those later engine
systems exist.

## Registered acceptance tests

- `KairoPhase1.GameState`
- `KairoPhase1.ProjectValidation`
- `KairoPhase1.Package`
- `KairoPhase1.NativeSmoke`

A native smoke test may return CTest skip code `77` only when the renderer raises
the typed presentation-unavailable condition. Compilation, initialization,
rendering, capture, project, or gameplay failures remain test failures.
