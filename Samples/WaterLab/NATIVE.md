# Native simulation lab

Build the integration workspace and run the target KairoSimulationLab.

    ./build/game-clang/Runtime/KairoPlayer/KairoSimulationLab --renderer metal
    ./build/game-clang/Runtime/KairoPlayer/KairoSimulationLab --renderer metal --soft

Select the renderer supported by your host (Metal on macOS, OpenGL or Vulkan
on Linux, D3D12 on Windows). Add --smoke for a bounded capture comparison.
The water view uses a shared cube with depth-scaled transforms, not a
transparent ocean shader. The soft-body view uploads a four-triangle surface
with recomputed normals. It is a tiny native visualization, not a general
deforming-mesh renderer. Linux OpenGL CTest gates are registered; no native
execution has been observed at authoring time.


Add --destruction for an intact cube that splits into eight subcubes after an authored impact. The fixture demonstrates one-shot fracture, impulse momentum and ground response; no arbitrary mesh cutting or fragment rotation/collision. Its Linux OpenGL smoke is registered but not yet executed.
