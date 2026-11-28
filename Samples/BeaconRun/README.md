# Beacon Run

A small playable objective game authored entirely with ordinary Kairo scene
entities and the generic player. Move using WASD or left stick, jump with
Space or A. Collect all six beacons by walking near them. Each disappears on
collection, the HUD fills where the native backend supports overlays, and the
console reports completion time. Escape quits. Relaunch for a new run.

Run after building the game profile:

    build/game-clang/Runtime/KairoPlayer/KairoPlayer Samples/BeaconRun/Project.kproject

No separate sample game executable or source asset downloads are needed.
The collectible tag opts in visual-only leaf scene entities. The objective
is session-local; no leaderboard, level progression or save persistence.

The current integration is restacked on the newer modal menu host: gameplay score/HUD updates pause while the menu owns the frame, and the HUD is restored on resume. Use `python3 Samples/BeaconRun/run_native_acceptance.py /path/to/KairoPlayer Samples/BeaconRun metal` (or `vulkan`, with an appropriate display) for the native smoke gate. It validates the ordinary six-goal sample, relocates one goal to spawn in an isolated copy, checks collection and nonuniform frame capture, and rejects a nonvisual/physics collectible. It does not drive the entire six-goal journey interactively. Collectibles must remain static and visual-only; runtime persistence is out of scope.

### Optional persistent progress

Run `KairoPlayer Samples/BeaconRun/Project.kproject --progress-file /path/to/existing/save-directory/beacon.kprogress` (use your built Player path). The file must be outside the project tree. The next launch using the same path restores collected beacons, objective time, trigger edge history and visual enable states. The player starts at the authored spawn; first gameplay sampling reconciles trigger edges with that location. Changed descriptor, startup-scene or trigger bytes reject the checkpoint. Use a new filename after editing the authored project. Saves are written on successful shutdown, not periodically; concurrent sessions using one file are unsupported, and the fingerprint is not a security mechanism. The existing full-world save bridge remains separate.
