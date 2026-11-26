# Native game lab

Build KairoGameLab with the integration workspace. Run using a backend with the pinned renderer's native viewport overlay (Vulkan or Metal):

    KairoGameLab --renderer metal
    KairoGameLab --renderer metal --strategy
    KairoGameLab --renderer metal --rpg

Platformer: A/D, Space, R reset. Strategy: 1/2 select, arrows move cursor, M move, G gather. RPG: 1/2 choose, S save, L load (optional --save path). Escape closes each sample. The same portable game kernels own state. Add --smoke to any invocation to require a completed objective and different before/after native captures. The native host is an unverified integration candidate at authoring time. These are small 2D game lab scenes, not a full editor-authored 3D RPG or RTS. OpenGL/D3D12 lack the current overlay capability.

## Integration acceptance

The native host consumes [RuntimeGameLabView.hpp](../../Runtime/KairoPlayer/RuntimeGameLabView.hpp), now tested against a CPU painter oracle and resized viewport bounds. The CTest label m50-native includes the three actual native objective/image-change smoke gates on Linux Vulkan and macOS Metal. Run ctest --preset game-clang -L m50-native --output-on-failure (use Xvfb on Linux). CPU rectangles are not native screenshots; the native gates remain unverified until executed. The presentation is intended for these small authored fixtures, not arbitrary maximum-sized levels or general GUI rendering.
