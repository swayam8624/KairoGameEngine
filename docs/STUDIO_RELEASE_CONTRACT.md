# Kairo Studio production release contract

This is the bounded first public engine release scope, not a claim of parity with every feature of a commercial engine. Do not label the release 100% until each gate has actual integrated evidence. Do not require the owner to iterate manually on incomplete native builds; the final owner-run Mac check follows the release candidate gate.

| Gate | Required acceptance | Present evidence / outstanding work |
| --- | --- | --- |
| Project hub | Open/create 2D, 3D and sample projects; recent projects, safe paths, missing-project recovery, engine/version information, no shell command construction from names | Existing project session, recipe compiler and lifecycle tools; dedicated startup hub and template workflow remain |
| Editor interaction | Selection never frames or moves camera; F frames explicitly; Q/W/E/R tools outside RMB; orbit/pan/dolly/free-look on mouse and trackpad; no clicks on toolbar treated as scene picks; undoable local/world transforms, parent hierarchy, cancel and redo | Native shell changes for gesture ownership and explicit framing; local geometry test; full mouse interaction, transformed children, retained selection and Retina picking acceptance pending |
| Selection and visual design | Consistent theme/typography, dock persistence, legible inspector and real asset preview; selected mesh silhouette, nonmesh icons, stable selection while editing | Studio shell and cobalt draw bounds; bounds are not a silhouette, skinned bounds are bind-pose approximations, thumbnails currently type glyphs |
| Scripting | Create source or visual script, edit with meaningful diagnostics and autocomplete/navigation, attach to entity, compile, run, debug, save/reopen; hot reload with defined state policy | Existing logic document compiler, graph/code authoring and native gameplay registration; end-to-end scripting UX and debugger acceptance remain |
| Scene and assets | Create/import/edit objects, lights/cameras/materials/audio, prefabs and scenes; asset import errors and dependency safety; undo/redo; missing-file recovery | Existing project/asset/scene services; representative actual end-to-end projects and graphical workflow validation remain |
| Gameplay/runtime | Input, collision, character movement, AI navigation, combat, UI, audio, animation, save/load and project lifecycle; coherent runtime/editor behavior and documented supported formats | Previous native Linux/Windows candidate regressions, Mac co-op failures outstanding; revalidate final pinned revisions |
| Release and performance | Incremental application build, documented clean build, distributable launchable app, validated package with licenses, no critical crashes/leaks, regression suite and real captures across declared platforms | Target-only build script; native Studio compile/render, Linux Vulkan leak diagnosis, packaging and macOS final run outstanding |

## Evidence rules

Keep actual commands, candidate commit and workspace lock, completed results, logs and captured frames. A portable test cannot certify GPU interaction. A source edit cannot certify native behavior. Missing acceptance is not a percentage. Distinguish static checks, local executed tests, CI and owner device tests. Preserve unmerged work and build caches until verified promotion.

## Pass 1: viewport interaction

Removed implicit camera framing after entity creation/asset placement. Normal selection remains a scene-state operation. RMB captured flight owns movement keys; without RMB, Q/W/E/R select transform tools. Orientation buttons are outside the scene image. Explicit F framing exits the authored camera lens and layer mask; ordinary navigation restores all render layers. Visible mesh selection uses actual submitted draw object IDs, cached mesh-local bounds and each draw model matrix. No per-frame vertex scan or GPU readback. Portable bounds topology test passed 50,000 seeded cases each under GCC, Clang and Clang ASan/UBSan; exact tested blobs in KairoEditor: 1a2fdf95f1ebcd7b38d6e59f3521e0de877aedc7 and 04620d481ec2ed46d83a114fe9c70751112749a5. Native interaction and highlight rendering still require acceptance. A full silhouette, correct skinned deformation bounds, and parent-aware transform gizmos are not claimed.

Next: validate and improve viewport ownership and actual selection on all supported GPU backends; project hub/template workflow; scripting lifecycle and editor integration; then asset, animation, packaging and consolidated acceptance.

## Pass 2: startup Project Hub

Implemented an actual pre-editor graphical entry point, with a consistent graphite/cobalt theme, recent project open/remove/missing state, safe recovery of corrupt recent history, explicit built-engine directory and graphics backend, asynchronous project creation, editor child-process diagnostics, and a launcher that never implicitly compiles or installs dependencies.

Four templates are wired to real formats: 3D Starter and 3D Arena use the existing recipe generator; Collectible Game and Adventure Starter copy the authored BeaconRun and KeyGateArena projects, preserving license files and excluding build/cache output. Existing destinations are never overwritten. A dedicated native 2D project template is explicitly unavailable; the standalone platformer lab is not an editable 2D project. No claims of a distributable Hub app bundle, real native 2D authoring, or graphical validation before CI.

Local execution: five focused core fixture suites passed for portable names, bounded and deduplicated recents, missing descriptors, safe sample duplication, nested-path and symlink rejection, overwrite prevention, runtime-output exclusion and unavailable template rejection. Core Git blob e552550bbb75f203f3111c45661f99486b7da19d verified. Final core adds explicit recovery; current exact blob ad8a1fde50a48adc551edd0daa5bd9c2343013da was tested for malformed-byte backup and recreation. Graphical Hub test and real repository-template tests are registered in Xvfb CI; a native CI gate compiles and validates all four generated projects and opens the starter Editor and arena Player. These new CI checks have not yet completed.

Entry point for future release acceptance: `bash scripts/launch_kairo.sh`. Owner Mac run remains deferred until final candidate acceptance.
