# Native game lab

Build KairoGameLab with the integration workspace. Run using a backend with the pinned renderer's native viewport overlay (Vulkan or Metal):

    KairoGameLab --renderer metal
    KairoGameLab --renderer metal --strategy
    KairoGameLab --renderer metal --rpg

Platformer: A/D, Space, R reset. Strategy: 1/2 select, arrows move cursor, M move, G gather. RPG: 1/2 choose, S save, L load (optional --save path). Escape closes each sample. The same portable game kernels own state. Add --smoke to any invocation to require a completed objective and different before/after native captures. The native host is an unverified integration candidate at authoring time. These are small 2D game lab scenes, not a full editor-authored 3D RPG or RTS. OpenGL/D3D12 lack the current overlay capability.

## Integration acceptance

The native host consumes [RuntimeGameLabView.hpp](../../Runtime/KairoPlayer/RuntimeGameLabView.hpp), now tested against a CPU painter oracle and resized viewport bounds. The CTest label m50-native includes the three actual native objective/image-change smoke gates on Linux Vulkan and macOS Metal. Run ctest --preset game-clang -L m50-native --output-on-failure (use Xvfb on Linux). CPU rectangles are not native screenshots; the native gates remain unverified until executed. The presentation is intended for these small authored fixtures, not arbitrary maximum-sized levels or general GUI rendering.

## RPG inventory and reward

The keeper's quest now grants an actual crystal item, consumes it on return and awards ten gold. Revisit the cave after picking up the crystal: the duplicate pickup is disabled. The inventory is visible in the HUD. Save/load writes KRL1, with migration of the earlier fixed-lab KQS1 saves. Invalid inventory/progress combinations reject without changing the current session. This is a small authored quest, not a full RPG inventory editor or equipment economy.

## Animation laboratory

Run KairoGameLab --animation --renderer metal (or vulkan). The left panel is the sampled source chain; the middle preserves target proportions while retargeting local rotations; the right applies the two-bone IK local rotation adapter and evaluates FK. The target marker turns green when reached, red when clamped. Space pauses; R resets phase. The native smoke requires clamped-to-reached state and a changed actual capture. The fixture is a procedural two-bone skeleton diagnostic, not imported glTF skinning or general clip authoring.
