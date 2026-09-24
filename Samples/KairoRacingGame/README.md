# KAIRO Racing — pmndrs/racing-game port

This is a native KAIRO gameplay workload based on the visual assets and gameplay
reference from `pmndrs/racing-game`.

The original React / react-three-fiber / Cannon runtime is not executed by this
sample. Runtime ownership is KAIRO:

- KairoAssets imports the track, chassis, and wheel scenes.
- KairoEngineCore owns project, scene, transform, tags, input, and authored physics.
- KairoPhysicsEngine owns runtime collision and motion.
- KairoPlayerRuntime owns project/bootstrap bridges.
- KairoRenderer owns presentation and the selected graphics backend.
- `KairoRacingGame` owns arcade vehicle control and lap/checkpoint state.

## Prepare local third-party assets

The generated GLBs are intentionally not committed. With the upstream repository
cloned beside the KAIRO workspace at `ExternalGames/racing-game`:

```bash
bash Samples/KairoRacingGame/tools/prepare_assets.sh
```

Override locations with `KAIRO_RACING_UPSTREAM` or `KAIRO_BLENDER` if needed.

## Controls

- W/S — throttle/reverse
- A/D — steer
- Space — brake
- R — reset
- Escape — quit

## Run

```bash
./build/dev-clang/Samples/KairoRacingGame/KairoRacingGame \
  Samples/KairoRacingGame/Project/Racing.kproject --renderer metal
```
