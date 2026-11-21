# Cross-repository source-learning curriculum

This is the **portfolio-level** reading order. Each repository has a finer-grained, clickable `docs/SOURCE_READING_ORDER.md`. This index is an educational itinerary, **not** a claim that every function is annotated, that every project compiles, or that every research result reproduces.

## Foundations: math and data

1. [KairoMath](https://github.com/swayam8624/KairoMath/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
2. [KairoGeometry](https://github.com/swayam8624/KairoGeometry/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
3. [KairoPhysicsMath](https://github.com/swayam8624/KairoPhysicsMath/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
4. [KairoECS](https://github.com/swayam8624/KairoECS/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
5. [KairoReflection](https://github.com/swayam8624/KairoReflection/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
6. [KairoScheduler](https://github.com/swayam8624/KairoScheduler/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
7. [KairoSIMD](https://github.com/swayam8624/KairoSIMD/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions

## Spatial and scene representation

1. [KairoSpatial](https://github.com/swayam8624/KairoSpatial/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
2. [KairoAssets](https://github.com/swayam8624/KairoAssets/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
3. [KairoEngineCore](https://github.com/swayam8624/KairoEngineCore/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions

## Real-time and offline graphics

1. [KairoRayTracer](https://github.com/swayam8624/KairoRayTracer/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
2. [KairoRenderer](https://github.com/swayam8624/KairoRenderer/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions

## Simulation and interactive editor

1. [KairoPhysicsEngine](https://github.com/swayam8624/KairoPhysicsEngine/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
2. [KairoEditor](https://github.com/swayam8624/KairoEditor/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
3. [KairoHub](https://github.com/swayam8624/KairoHub/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
4. [KairoGameEngine](https://github.com/swayam8624/KairoGameEngine/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions

## ML/compute extensions

1. [MLLibrary](https://github.com/swayam8624/MLLibrary/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
2. [KairoGPU](https://github.com/swayam8624/KairoGPU/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
3. [KairoONNX](https://github.com/swayam8624/KairoONNX/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
4. [KairoTransformers](https://github.com/swayam8624/KairoTransformers/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
5. [KairoAI](https://github.com/swayam8624/KairoAI/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
6. [KairoMacPerception](https://github.com/swayam8624/KairoMacPerception/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions

## Production pipelines

1. [KairoPipelineCore](https://github.com/swayam8624/KairoPipelineCore/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
2. [KairoBlender](https://github.com/swayam8624/KairoBlender/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
3. [KairoHoudini](https://github.com/swayam8624/KairoHoudini/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
4. [KairoMaya](https://github.com/swayam8624/KairoMaya/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
5. [KairoNuke](https://github.com/swayam8624/KairoNuke/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
6. [KairoProductionTools](https://github.com/swayam8624/KairoProductionTools/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions

## Independent systems and graphics research

1. [NESEmulator](https://github.com/swayam8624/NESEmulator/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
2. [Maveb](https://github.com/swayam8624/Maveb/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions
3. [Vulkax](https://github.com/swayam8624/Vulkax/blob/docs/teaching-walkthrough-20261121/docs/SOURCE_READING_ORDER.md) — source map and verification questions

## Read one subsystem as a connected story

Start with the public interface and build configuration, trace the data path into implementation and runtime adapters, then read the tests and recorded CI/evidence. Keep three independent questions open: **what is implemented**, **what is observed to pass**, and **what is scientifically established**.

For a math module, track coordinate/storage conventions and numerical error to geometry, spatial and physics consumers. For graphics, follow the asset importer into authored scenes and backend-specific resources. For simulation, distinguish candidate broadphase contacts, actual narrowphase contacts and solver results. For editor/Hub, track user intent through validated documents, explicit host permissions and scene/runtime state. For research projects, follow frozen experiment protocols before interpreting headline metrics.

## Annotation progress (21 November 2026)

This branch wave creates source maps and README navigation in all 31 linked repositories and adds missing permissive licenses to repositories where original work was not already licensed. It also annotates central entrypoint/source files and a larger NES source cohort. **It is not yet a complete per-function annotation pass.** The next file-level work remains: walk every source/target/test/script, write function-level contracts, connect caller/test links, review upstream attribution and run native/CI checks.

## Licensing and scientific honesty

Keep existing Apache-2.0 or MIT licenses in their respective projects. Newly added MIT grants apply to original owner-controlled contributions; do not treat them as third-party sublicenses. NESEmulator explicitly needs an upstream provenance audit. Frozen MAVEB/VULKAX experimental claims and Kairo's unverified 95% target are not changed by documentation.

## Verification gate

No native compile/test campaign has been run as part of this documentation branch wave. Before merge, review diffs and source comments, run repository CI and the Kairo exact-head acceptance suite, and inspect LICENSE/NOTICE and bundled assets individually.
