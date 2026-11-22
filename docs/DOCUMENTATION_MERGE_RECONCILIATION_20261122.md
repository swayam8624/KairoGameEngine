# Source-teaching documentation merge reconciliation — 2026-11-22

This is a **dated status snapshot** of the cross-repository teaching project, not a claim that all branches were merged. The original 2026-11-21 teaching branch [historical ledger](https://github.com/swayam8624/KairoGameEngine/blob/docs/teaching-source-map-2026-11/docs/TEACHING_DOCUMENTATION_STATUS.md) scoped **31 repositories** and approximately **1,801 first-party source/build files**, with mixed annotation depth. Some of those earlier notes were made on PR branches rather than active default branches.

## Meaning of merged in this project

A documentation file is counted as **integrated** only after its contents are present on the repository's active default branch and a fetch confirms it. A diverged Git history, an open PR, a matching file name, or a completed source review does not establish that an entire PR was merged. Do not apply a blanket MIT license to third-party material. Runtime code changes, test execution and scientific validation are separate steps.

At this snapshot, the 31 historic `docs/teaching-source-map-2026-11` PRs remained **open** with `mergeable=false` reported by GitHub. This can reflect conflicts or other mergeability conditions; consult fresh PR state before taking action. Many active branches separately received more recent documentation, so old PRs should not be merged wholesale without a file-by-file comparison.

## Original PR reconciliation queue

| Repository | Historical documentation PR | Active default branch | Status at audit |
| --- | --- | --- | --- |
| NESEmulator | [#1](https://github.com/swayam8624/NESEmulator/pull/1) | main | Open; reconcile versus newer source teaching |
| Maveb | [#125](https://github.com/swayam8624/Maveb/pull/125) | main | Open; protected branch, see PR #129 |
| Vulkax | [#102](https://github.com/swayam8624/Vulkax/pull/102) | main | Open; historical catalog/tutorial forwarded |
| KairoGameEngine | [#40](https://github.com/swayam8624/KairoGameEngine/pull/40) | main | Open; historical catalog/tutorial forwarded |
| KairoRenderer | [#19](https://github.com/swayam8624/KairoRenderer/pull/19) | main | Open; compare to existing source map |
| KairoPhysicsEngine | [#14](https://github.com/swayam8624/KairoPhysicsEngine/pull/14) | main | Open; compare to existing source map |
| KairoEditor | [#19](https://github.com/swayam8624/KairoEditor/pull/19) | main | Open; compare to existing source map |
| KairoMath | [#2](https://github.com/swayam8624/KairoMath/pull/2) | master | Open; default uses master, not main |
| KairoGeometry | [#3](https://github.com/swayam8624/KairoGeometry/pull/3) | master | Open; default uses master, not main |
| KairoPhysicsMath | [#4](https://github.com/swayam8624/KairoPhysicsMath/pull/4) | main | Open; compare to existing source map |
| KairoSpatial | [#6](https://github.com/swayam8624/KairoSpatial/pull/6) | main | Open; compare to existing source map |
| KairoEngineCore | [#24](https://github.com/swayam8624/KairoEngineCore/pull/24) | main | Open; compare to existing source map |
| KairoAssets | [#14](https://github.com/swayam8624/KairoAssets/pull/14) | main | Open; compare to existing source map |
| KairoECS | [#4](https://github.com/swayam8624/KairoECS/pull/4) | main | Open; compare to existing source map |
| KairoReflection | [#2](https://github.com/swayam8624/KairoReflection/pull/2) | main | Open; compare to existing source map |
| KairoRayTracer | [#4](https://github.com/swayam8624/KairoRayTracer/pull/4) | main | Open; compare to existing source map |
| KairoHub | [#6](https://github.com/swayam8624/KairoHub/pull/6) | main | Open; compare to existing source map |
| KairoGPU | [#2](https://github.com/swayam8624/KairoGPU/pull/2) | main | Open; newer GPU teaching already on main |
| KairoScheduler | [#2](https://github.com/swayam8624/KairoScheduler/pull/2) | main | Open; newer concurrency teaching already on main |
| KairoSIMD | [#3](https://github.com/swayam8624/KairoSIMD/pull/3) | main | Open; newer SIMD teaching already on main |
| KairoONNX | [#3](https://github.com/swayam8624/KairoONNX/pull/3) | main | Open; fixture notice, reading order and deeper comments forwarded |
| KairoTransformers | [#3](https://github.com/swayam8624/KairoTransformers/pull/3) | main | Open; guided reading order and advanced functions forwarded |
| KairoMacPerception | [#2](https://github.com/swayam8624/KairoMacPerception/pull/2) | main | Open; compare to existing source map |
| KairoAI | [#4](https://github.com/swayam8624/KairoAI/pull/4) | main | Open; compare to existing source map |
| KairoPipelineCore | [#5](https://github.com/swayam8624/KairoPipelineCore/pull/5) | main | Open; compare to existing source map |
| KairoBlender | [#2](https://github.com/swayam8624/KairoBlender/pull/2) | main | Open; preserve GPL-3.0-or-later |
| KairoProductionTools | [#1](https://github.com/swayam8624/KairoProductionTools/pull/1) | main | Open; compare to existing source map |
| KairoHoudini | [#1](https://github.com/swayam8624/KairoHoudini/pull/1) | main | Open; compare to existing source map |
| KairoMaya | [#1](https://github.com/swayam8624/KairoMaya/pull/1) | main | Open; compare to existing source map |
| KairoNuke | [#1](https://github.com/swayam8624/KairoNuke/pull/1) | main | Open; compare to existing source map |
| MLLibrary | [#11](https://github.com/swayam8624/MLLibrary/pull/11) | main | Open; newer C++23/source/test training materials already on main |

## Verified mainline integrations during this reconciliation

- **KairoONNX/main:** restored `THIRD_PARTY_NOTICES.md`, enriched `docs/SOURCE_READING_ORDER.md`, and added function-level parser/runtime/optimizer explanations; preserved more recent mainline API docs.
- **KairoTransformers/main:** enriched the source learning order and first-party function comments for cached generation, attention, packed weights, training, and two distinct checkpoint mechanisms.
- **Vulkax/main:** forwarded [455-source catalog](SOURCE_CATALOG.md) to its own repository and restored `SIMULATION_TO_VERIFIED_WORLD_TUTORIAL.md`. The catalog is explicitly identified as a historical snapshot.
- **KairoGameEngine/main:** restored this repository's [92-file source catalog](SOURCE_CATALOG.md) and [KairoPlayer launch/frame walkthrough](PLAYER_EXECUTION_EXPLAINED.md).
- **Maveb:** [PR #129](https://github.com/swayam8624/Maveb/pull/129) contains historical 655-source catalog, TSDF/local world-edit tutorial, and updated reading order. **Not merged**: main requires five successful checks, and auto-merge is disabled.
- Current Kairo foundation library licenses were checked on their active default branches. Existing MIT files were retained, not replaced with stale branch copies.

## Practical next merge-and-teach passes

For each open historic PR, compare the file list and **current text** of the head branch with the active default branch. If an older annotation is already present or superseded, leave the richer mainline version untouched. If a file is genuinely absent, port it with correct live links, dated inventory labels, and the appropriate license boundary. If source comments conflict, preserve executable statements and reconcile the pedagogy line-by-line rather than merging the entire stale branch.

After that, run a build/test check on the exact resulting commit. Use native Metal/Swift/Xcode, Blender/Maya/Houdini/Nuke, and scientific/research-validation hosts only where available and applicable. Do not claim a check passed merely because source documents now link it.

**Verification note:** this 2026-11-22 ledger records source/PR metadata and successful GitHub content commits. It does not certify compiled code, all PR merges, platform CI, scientific benchmark reproduction, or correctness of every linked historical path.
