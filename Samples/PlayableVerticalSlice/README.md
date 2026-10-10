# Kairo Playable Vertical Slice

This project is intentionally launched by the normal `KairoPlayer` executable rather than a sample-specific game host. It is the first integration target for the playable-world pass.

Controls: **WASD / left stick** move the kinematic capsule, **Space / gamepad A** jumps, and **Escape** quits. The scene opts into Kairo's built-in player controller with the `kairo.player-controller` entity tag and otherwise uses ordinary EngineCore scene, physics, camera, lighting, environment, asset, and input data.

From an umbrella build directory:

```sh
./Runtime/KairoPlayer/KairoPlayer ../Samples/PlayableVerticalSlice/Project.kproject
```

The authored Gameplay Camera also has the explicit
`kairo.follow-player-camera` tag. KairoPlayer preserves its original camera
orientation and projection while translating the camera with the moving
kinematic player, so the avatar remains visible during keyboard/gamepad
movement. This is the built-in third-person follow contract, not a replacement
for custom cinematic/scripted camera systems: removing the tag preserves
original camera ownership.

A successful vertical-slice smoke test proves that the same generic player path can load the authored startup scene, construct PhysicsWorld, bind the character motor, consume project input actions, render from the authored primary camera, and run the normal logic/native/production/audio update fanout without the old `Phase1Game` executable owning gameplay integration.
