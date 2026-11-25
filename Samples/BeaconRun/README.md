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

## Release verification

    python3 scripts/verify_project_release.py build/game-clang/Runtime/KairoPlayer/KairoPlayer Samples/BeaconRun/Project.kproject

The gate packages a temporary copy, relocates its release directory, removes
the original source copy, verifies every project file and the player binary
by SHA-256, then launches the package's own launcher with --validate.
On Linux under a display server, --smoke also checks the native Vulkan
viewport. This verifies relocation on the current build host, not
redistribution to a clean OS without installed graphics/system libraries.
