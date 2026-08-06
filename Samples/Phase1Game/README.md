# Kairo Phase 1 Game

This is the first end-to-end gameplay vertical slice built on the reusable
`KairoPlayerRuntime` library. It intentionally uses only systems that exist:
a tagged authored scene, named input actions, fixed-step rigid-body physics,
trigger contacts, deterministic game state, save/load, Vulkan rendering, and
runtime packaging.

## Controls

| Action | Keyboard | Gamepad |
| --- | --- | --- |
| Start | Enter | Start |
| Move | WASD | Left stick |
| Jump | Space | A |
| Pause | P | Back |
| Reset | R | Y |
| Save | F5 | X |
| Load | F9 | B |
| Quit | Escape | — |

Collect all three objects, then touch the goal. Touching the hazard or falling
out of the arena loses the run. The window title is the current minimal runtime
status surface; a complete in-game UI system belongs to a later engine phase.

## Run

```bash
./build/dev-clang/Samples/Phase1Game/KairoPhase1Game
```

The executable defaults to the checked-in project. A relocated project may be
passed explicitly:

```bash
./build/dev-clang/Samples/Phase1Game/KairoPhase1Game \
  Samples/Phase1Game/Project/Phase1.kproject
```

## Validate and smoke test

```bash
./build/dev-clang/Runtime/KairoPlayer/KairoPlayer \
  Samples/Phase1Game/Project/Phase1.kproject --validate

./build/dev-clang/Samples/Phase1Game/KairoPhase1Game --smoke
```

## Package

```bash
./build/dev-clang/Samples/Phase1Game/KairoPhase1Game \
  Samples/Phase1Game/Project/Phase1.kproject \
  --package Release --replace
```

The generated launcher contains the Phase 1 executable rather than the generic
player, so the packaged artifact retains the sample's gameplay state machine.
