# Teaching-first documentation and licensing — change ledger

**Snapshot:** 2026-11-21. **Scope:** 27 Kairo repositories plus NESEmulator, MAVEB, Vulkax and MLLibrary (31 repos). All changes are on `docs/teaching-source-map-2026-11` branches and open pull requests; nothing in this curriculum is described as merged until independently verified. No functional source changes were intended.

## Progress at this snapshot

- 31 repository pull requests prepared; each has `READING_ORDER.md` with source links and API/interactions learning notes, plus README entry-point links.
- 314 source/test/build entrypoints have received teaching comments or a teaching header out of 1801 indexed/source paths counted for this snapshot. **These totals mix C++/Swift/Python source with selected build/scripts in the large research catalogs; they are a navigation metric, not a comparable function-coverage percentage.**
- Even a repo with every file header-annotated still needs a second **function-by-function** pedagogy and accuracy review. A header is NOT proof that each method, foreign API or performance trade-off is explained fully.
- New MIT licenses are proposed where the owner's original code lacked a root license. Existing licensing was not replaced: KairoBlender remains GPL-3.0-or-later, MAVEB/Vulkax retain Apache-2.0 and applicable research/asset terms; NES apparent OneLoneCoder derivatives retain OLC-3 with a separate scope for original MIT additions.

## Repository-by-repository proposed changes

| PR / repository | Source files commented / cataloged | Licensing |
|---|---:|---|
| [NESEmulator](https://github.com/swayam8624/NESEmulator/pull/1) | 17/17 | Mixed: own MIT additions, upstream OLC-3 |
| [Maveb](https://github.com/swayam8624/Maveb/pull/125) | 6/655 | Apache-2.0 software; research licensing retained |
| [Vulkax](https://github.com/swayam8624/Vulkax/pull/102) | 5/455 | Apache-2.0 retained |
| [KairoGameEngine](https://github.com/swayam8624/KairoGameEngine/pull/40) | 5/92 | Existing MIT |
| [KairoRenderer](https://github.com/swayam8624/KairoRenderer/pull/19) | 5/51 | New MIT proposed |
| [KairoPhysicsEngine](https://github.com/swayam8624/KairoPhysicsEngine/pull/14) | 6/31 | New MIT proposed |
| [KairoEditor](https://github.com/swayam8624/KairoEditor/pull/19) | 6/79 | New MIT proposed |
| [KairoMath](https://github.com/swayam8624/KairoMath/pull/2) | 29/29 | New MIT proposed |
| [KairoGeometry](https://github.com/swayam8624/KairoGeometry/pull/3) | 18/18 | New MIT proposed |
| [KairoPhysicsMath](https://github.com/swayam8624/KairoPhysicsMath/pull/4) | 12/12 | New MIT proposed |
| [KairoSpatial](https://github.com/swayam8624/KairoSpatial/pull/6) | 22/22 | New MIT proposed |
| [KairoEngineCore](https://github.com/swayam8624/KairoEngineCore/pull/24) | 8/69 | New MIT proposed |
| [KairoAssets](https://github.com/swayam8624/KairoAssets/pull/14) | 16/50 | New MIT proposed |
| [KairoECS](https://github.com/swayam8624/KairoECS/pull/4) | 6/6 | New MIT proposed |
| [KairoReflection](https://github.com/swayam8624/KairoReflection/pull/2) | 4/4 | New MIT proposed |
| [KairoRayTracer](https://github.com/swayam8624/KairoRayTracer/pull/4) | 32/32 | New MIT proposed |
| [KairoHub](https://github.com/swayam8624/KairoHub/pull/6) | 11/11 | New MIT proposed |
| [KairoGPU](https://github.com/swayam8624/KairoGPU/pull/2) | 4/4 | New MIT proposed |
| [KairoScheduler](https://github.com/swayam8624/KairoScheduler/pull/2) | 3/3 | New MIT proposed |
| [KairoSIMD](https://github.com/swayam8624/KairoSIMD/pull/3) | 4/4 | New MIT proposed |
| [KairoONNX](https://github.com/swayam8624/KairoONNX/pull/3) | 5/5 | New MIT proposed |
| [KairoTransformers](https://github.com/swayam8624/KairoTransformers/pull/3) | 8/8 | New MIT proposed |
| [KairoMacPerception](https://github.com/swayam8624/KairoMacPerception/pull/2) | 7/7 | New MIT proposed |
| [KairoAI](https://github.com/swayam8624/KairoAI/pull/4) | 14/14 | Existing MIT |
| [KairoPipelineCore](https://github.com/swayam8624/KairoPipelineCore/pull/5) | 15/15 | Existing MIT |
| [KairoBlender](https://github.com/swayam8624/KairoBlender/pull/2) | 12/12 | GPL-3.0-or-later retained |
| [KairoProductionTools](https://github.com/swayam8624/KairoProductionTools/pull/1) | 2/2 | Existing MIT |
| [KairoHoudini](https://github.com/swayam8624/KairoHoudini/pull/1) | 8/8 | Existing MIT |
| [KairoMaya](https://github.com/swayam8624/KairoMaya/pull/1) | 8/8 | Existing MIT |
| [KairoNuke](https://github.com/swayam8624/KairoNuke/pull/1) | 9/9 | Existing MIT |
| [MLLibrary](https://github.com/swayam8624/MLLibrary/pull/11) | 7/69 | New MIT proposed |

## What has been completed

1. Generated ordered, clickable reading maps, with subsystem call paths and key APIs, for all scoped repositories.
2. Clarified README entry points and license boundaries.
3. Annotated major Kairo foundation and pipeline source files as well as the entire NES source, prioritizing ownership and state flow, native/portable APIs, validation boundaries, and tests.
4. Added extensive research source catalogs for Maveb and Vulkax without altering frozen results.

## Remaining work, explicitly not claimed done

- Deep per-function comments for *every* source file, especially large Maveb/Vulkax research trees; explain invariants, complexity, threading, synchronization, failure modes, APIs, types, units, and actual caller/callee lines—not just file-level summaries.
- Finish uncovered source files in KairoEngineCore, KairoAssets, KairoRenderer, KairoEditor, KairoPhysicsEngine, KairoGameEngine, and MLLibrary. Give tests and examples the same care as runtime source.
- Audit existing README and ancillary docs for stale build commands, broken file links, outdated benchmarks, duplicate claims and mismatches with the actual source.
- Audit third-party license attribution and bundled assets before legal release; repository-wide MIT text must never relabel external assets.
- Execute formatting/build/test/link checks on the exact documentation branches. CI results, native Metal/Windows/Blender/Maya/Houdini/Nuke gates, and scientific benchmark reproduction must remain separately recorded.
- Review/merge approved PRs in a dependency-aware order; re-run Kairo's exact-head acceptance rather than implying a 95% completion score from documentation.

## Suggested technical review protocol

Each function teaching note should state **intent**, **input/output**, **preconditions/invariants**, **ownership/lifetime**, **calls into other files**, **errors/fallback**, **numerics/threading/performance where applicable**, and **the matching test**. Preserve testability and compiler syntax, including C++23 global module fragments, Python `__future__` imports, Swift `// swift-tools-version:`, and TypeScript triple-slash directives.

Do not merge a PR merely because it has a source file catalog. Verify no executable behavior changed and that commentary is accurate before acceptance.
