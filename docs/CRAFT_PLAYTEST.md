# Kairo Craft developer playtest

This documents the review candidate with native Vulkan/Metal viewport HUD,
timed mining, health, equipment and first-person controls. It is not a release
claim. The combined earlier integration candidate passed native Vulkan and Metal;
these later changes still require exact-head acceptance.

## CPU acceptance without the graphics SDK

Run the complete 32-contract C++23 suite on a source checkout:

```bash
CXX=clang++ bash scripts/run_craft_cpu_contracts.sh
```

The script uses strict warnings, ASan/UBSan and a temporary build directory.
To run just the integrated world/inventory/equipment and survival journeys:

```bash
CXX=clang++ bash scripts/run_craft_cpu_contracts.sh CraftInventoryTests CraftSurvivalJourneyTests
```

The full suite is required by the CI ownership job. A selected subset is useful
for iteration, but cannot substitute for native Vulkan/Metal acceptance.

## Build against the locked workspace

Use a clean checkout of the candidate with its exact sibling revisions. Do not
overwrite uncommitted changes in any sibling. Follow the root README for compiler,
Vulkan/MoltenVK and dependency setup; use the strict player preset.

```bash
cmake --preset game-player-clang \
  -DKAIRO_PLAYER_BUILD_PROCEDURAL_TERRAIN=ON \
  -DKAIRO_GAME_ENGINE_BUILD_PHASE1_SAMPLE=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build --preset game-player-clang --parallel 2 \
  --target KairoCraftStream KairoCraftCaves
```

Launch surface survival on a Vulkan-capable machine:

```bash
build/game-player-clang/Runtime/KairoPlayer/KairoCraftStream \
  --renderer vulkan --seed 42 --survival --workers 4 --uploads 2 \
  --save craft-surface.kcraft
```

Launch cave survival on a Mac with Metal:

```bash
build/game-player-clang/Runtime/KairoPlayer/KairoCraftCaves \
  --renderer metal --seed 42 --survival --save craft-caves.kcraft
```

Choose a backend explicitly for reproducible testing. The games use separate
world generation formats; do not use the same save filename for both.

## Relocated developer bundle

After building, run:

```bash
cmake --build --preset game-player-clang --target KairoCraftPlaytestBundle
```

The default output is `build/game-player-clang/craft-playtest`. Configure
`-DKAIRO_CRAFT_PLAYTEST_OUTPUT=/absolute/fresh/path` to choose another location.
The target records the configured source revision and renderer lock. Reconfigure
before staging after a source update; CI supplies the checked-out revision at
staging time. The target refuses an existing output; preserve any saves before removing
or replacing a bundle. Both executables, separate save paths, launchers and
SHA-256/revision manifest are staged. Run `run-surface.sh` or `run-caves.sh`
(or corresponding Windows .cmd) without arguments for survival. Passing
arguments forwards them unchanged, for example `--renderer metal --smoke`.

The Vulkan renderer candidate embeds all twelve compiled shaders; no original
build shader path is intended to be required. Native moved-directory and
relocated executable tests remain pending. The bundle does not collect dynamic
libraries, graphics drivers, code signatures or installer metadata, and must
be tested on a host with the required runtime dependencies.

## Hostile creature prototype

One session-local creature appears near the player in survival if a supported
resident spawn exists. It approaches using the voxel collision solver and
deals two health points at most once per second, with line-of-sight required.
Aim at the creature and click left mouse to deal four points; three hits
defeat it, with a half-second hit cooldown. It respawns after 15 active
seconds. It has a simple red cuboid mesh; the title reports its health. When aimed at an in-range visible sentinel,
the crosshair turns red and an enemy health bar appears above it.
Placement is blocked inside its body. Unknown cells occlude attacks.
The creature also uses bounded same-layer four-neighbor navigation when
direct sight is blocked, replanning at most twice per second. Defeating the stone sentinel immediately awards up to two stone blocks; any excess above the existing stack capacity is discarded. The reward persists through the normal inventory save. There are no ground drops, animation, persistent creature state, vertical navigation or
player/creature rigid-body contact. Creative mode does not spawn enemies.

## Block selection feedback

The aimed block is outlined in gold, or red if mining is blocked by a
protected material, world boundary or full inventory. The face-adjacent
placement preview is green when its preflight checks pass, otherwise red.
The outline uses rebased world coordinates and clears when the aim is lost
or the block is removed. Preflight does not guarantee native upload or edit
journal capacity; actual edits retain the existing transactional checks.

## Surface lighting cycle

Surface play starts at noon and cycles through dusk, night and dawn over ten
minutes of focused simulation. Lighting updates at 240 quantized phases per
cycle, without regenerating the resident terrain meshes. Night retains a dim
ambient floor. The clock pauses when focus is lost and is restored from a survival checkpoint; it does not affect cave lighting, weather or mob spawning.

## Save-file replacement and recovery

When launched with a save path, both games automatically checkpoint dirty
world edits, inventory, selected slot and equipment after 30 seconds of
focused simulation. Repeated edits do not postpone the checkpoint. Failure
leaves state dirty and retries after five active seconds. F5 saves immediately;
a normal exit attempts one last dirty checkpoint and returns a failure status
if it cannot save. A new save slot is dirty initially, even before edits; its first checkpoint creates the file. Without a save path, no file is created. Survival checkpoints now also record grounded-restorable feet position, camera orientation, health, hunger and surface time of day. An unsupported or obstructed saved position uses the normal safe spawn, preserving the saved health and hunger. Creative saves retain any previously loaded survival checkpoint; older saves upgrade on survival play. Velocity, partial hunger/recovery timers and enemy state reset on load.

Both demos reserve a sibling directory named `<save>.write-lock` before
writing a temporary payload. A concurrent cooperating writer is refused.
A closed payload is renamed into place without deleting the prior save first.
If the program crashes, inspect and remove a stale reservation only after
confirming no writer is active; it may contain an incomplete payload. Do not
treat it as a valid save without codec validation. Replacement is not an
fsync-based crash-durability guarantee.

## Controls and progression

| Input | Action |
| --- | --- |
| WASD | Walk |
| Mouse / arrow keys | Look around |
| Space | Jump in survival, ascend in creative |
| Left Shift | Sprint in survival |
| Left Ctrl | Descend in creative |
| Tab | Release or recapture mouse |
| Left mouse hold | Mine in survival (instant in creative) |
| Right mouse | Place selected block |
| 1–4 | Choose block material |
| R | 1 dirt + 2 stone → 2 building blocks |
| T | 4 stone → 3 building blocks |
| F | Craft stone pick (2 dirt + 3 stone) |
| G | Equip/stow existing pick |
| E | Forage: spend 2 grass blocks to restore 6 hunger |
| F5 | Save if a path was supplied |
| Esc | Exit |

The pick doubles speed for stone/building blocks, consumes one use per successful
eligible mine, and breaks after 64 uses. Hand mining stays available. Health
starts at 20; falls over three world units cause damage. Death respawns the player
and preserves inventory, world edits and equipment. Equipment is persisted in the v5 envelope; the v6 checkpoint wraps that envelope with player metadata. Older supported v2/v3 surface and v4 cave saves load with no equipment, while v5 loads with equipment and the normal starting player state.

## Acceptance checklist

1. Start a visible game window, verify the scene and HUD appear, move the mouse
   and test keyboard fallback.
2. Tab away from capture; hold a mouse button while recapturing. Verify no
   placement or mining until release, and no camera jump. Hold E/F/R/F5 while switching focus; no new discrete action may fire until release. The simulation and drawing pause while unfocused, minimized or without a drawable framebuffer. Hold a discrete action or Tab while minimizing and restoring; verify no new action or cursor toggle until release.
3. Mine grass, dirt and stone; interrupt a hold by looking away. Verify progress
   resets and inventory changes only once a block is actually removed.
4. Craft a pick, mine stone, stow/equip it, verify durability and speed. Refill
   an inventory stack; a refused mine must not consume durability.
5. Save, close, reload and check inventory, equipment and edited world. Test
   separate surface/cave files. A malformed file must fail explicitly.
6. Walk and jump across chunk boundaries; check the world does not disappear,
   the player cannot walk into unloaded cells and short jumps cause no damage.
7. Resize portrait and wide; ensure crosshair stays centered, HUD visible and
   world restored at original size.
8. Run the native smoke and full CTest gates; capture exact commit, lock, backend
   and logs. Native renderer offscreen capture is distinct from actual visible
   window presentation.

Nutrition drains one point per 30 seconds at rest, three times faster while sprinting. Sprint is unavailable at hunger four or below. High hunger supports healing; starvation damages health. The forage recipe is a prototype, not a full food system.

Known gaps: expanded combat and NPC behavior, food varieties, item varieties, textured materials,
full UI text and independent game distribution are still outstanding. Native
interactive focus and mouse behavior require a real playtest.

## Experimental weather (M50 candidate)

Add `--weather` to the KairoCraftStream launch command. The seeded cycle progresses from clear to overcast and rain, then clears; the first rain begins around one active minute. Lighting and bounded camera-local rain geometry share the existing terrain submission. Weather time is session-local, and rain has no physics or accumulation. The smoke path now contains a rain visibility/removal gate; it must pass on a native host before acceptance.
