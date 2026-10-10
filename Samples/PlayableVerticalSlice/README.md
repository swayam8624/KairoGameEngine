# Kairo Playable Vertical Slice

This project is intentionally launched by the normal `KairoPlayer` executable rather than a sample-specific game host. It is the first integration target for the playable-world pass.

Controls: **WASD / left stick** move the kinematic capsule, **Space / gamepad A** jumps, and **Escape** quits. The scene opts into Kairo's built-in player controller with the `kairo.player-controller` entity tag and otherwise uses ordinary EngineCore scene, physics, camera, lighting, environment, asset, and input data.

From an umbrella build directory:

```sh
./Runtime/KairoPlayer/KairoPlayer ../Samples/PlayableVerticalSlice/Project.kproject
```

A successful vertical-slice smoke test proves that the same generic player path can load the authored startup scene, construct PhysicsWorld, bind the character motor, consume project input actions, render from the authored primary camera, and run the normal logic/native/production/audio update fanout without the old `Phase1Game` executable owning gameplay integration.
