# Portfolio open source and third-party provenance audit

**Scope:** the 31 repositories linked by [the portfolio roadmap](PORTFOLIO_LEARNING_ROADMAP.md), inspected on their teaching branches. This is a file-presence and stated-license audit, **not** a legal review of every contribution or bundled asset. The author can license only material for which they have the necessary rights.

## 1. License files actually present

All 31 selected repositories have a top-level `LICENSE` file on the inspected documentation branches. The permissions are **not uniform**:

| Repositories | Stated top-level license |
| --- | --- |
| KairoMath, KairoGeometry, KairoSpatial, KairoPhysicsMath, KairoPhysicsEngine, KairoRayTracer, KairoRenderer, KairoAssets, KairoEngineCore, KairoEditor, KairoGameEngine, KairoECS, KairoGPU, KairoScheduler, KairoSIMD, KairoONNX, KairoTransformers, KairoAI, KairoReflection, KairoHub, KairoMaya, KairoHoudini, KairoNuke, KairoPipelineCore, KairoProductionTools, KairoMacPerception | MIT |
| [KairoBlender](https://github.com/swayam8624/KairoBlender/blob/docs/teaching-walkthrough-20261121/LICENSE) | GPL-3.0-or-later (the file references the FSF full license URL and carries the SPDX identifier) |
| [Maveb](https://github.com/swayam8624/Maveb/blob/docs/teaching-walkthrough-20261121/LICENSE) and [Vulkax](https://github.com/swayam8624/Vulkax/blob/docs/teaching-walkthrough-20261121/LICENSE) | Apache License 2.0 |
| [NESEmulator](https://github.com/swayam8624/NESEmulator/blob/docs/teaching-walkthrough-20261121/LICENSE) | MIT **for original contributions only**, explicitly not a blanket grant for third-party code or ROMs |
| [desktop-pets](https://github.com/swayam8624/desktop-pets/blob/docs/teaching-guided-source-20261121/LICENSE) | MIT for original code/documentation; dependencies and externally sourced sprites/media keep their own terms |

The grouping above is exactly **26 MIT Kairo repositories + one GPL Kairo repository + two Apache research repositories + one original-contribution-limited MIT NES repository + one MIT desktop-pets repository = 31**. It does not imply identical licensing obligations for all projects.

## 2. Original source code versus bundled dependencies

A project's code-level MIT or Apache grant is meaningful for the author's original source and documentation. It does not replace notices in vendored code such as cgltf, ImGui, windowing libraries, image decoders, tests, game engines or proprietary SDK bindings. A user who redistributes a binary must inspect the included dependency versions, their licenses, copyright notices and applicable obligations.

Blender extension code is explicitly under GPL-3.0-or-later, not MIT. Before distributing it as a packaged extension, confirm the packaged distribution includes the GPL license information required by its stated terms. A short URL notice may not be sufficient for all distribution channels; packaging should be checked separately.

For Apache 2.0 research repositories, preserve upstream copyright/license notices and any applicable NOTICE/attribution material when redistributing modified or bundled third-party work.

## 3. Media, data and research assets require separate permission

Source assets can include meshes, textures, sprites, photography, fonts, audio, pretrained models, ROM images, captured worlds, datasets and third-party benchmark scenes. Each needs its own origin, copyright/license or permission, allowed uses, modification history and distribution decision.

| Asset category | Required provenance questions |
| --- | --- |
| NES ROMs/BIOS/game art | Is the user entitled to distribute the exact commercial game image or assets? The emulator code license does not answer this. |
| Desktop Pets sprites/audio/fonts | Were images created by the author or obtained under a license that permits redistribution and adaptation in an itch.io game? |
| DCC examples and published cache frames | Who authored the original model/texture/scene? Does a publish manifest's SHA-256 identity correspond to content the publisher may redistribute? |
| MAVEB/Vulkax datasets and generated evidence | Which capture/benchmark dataset licenses, acknowledgments and paper conditions apply to raw inputs, derivative imagery and release packages? |
| GitHub vendored libraries and SDK shims | Does the exact included dependency require a license/notice copy in source or binary artifacts? |

A SHA-256 fingerprint proves a byte identity for integrity comparison, **not legal ownership, scientific correctness or permission to redistribute**.

## 4. Suggested provenance register

For every distributable non-original file, keep a machine-readable entry with source URL/repository, upstream author, license and version, file SHA-256, import/transformation history, any attribution text, redistribution scope and where the notice is included in the final package.

For example, the *fields* of a useful record are:

```yaml
artifact_path: "assets/example.png"
origin: "<original source URL or own-work record>"
creator: "<author or organization>"
license: "<SPDX identifier or explicit permission>"
source_version: "<tag/date>"
sha256: "<digest of exact source bytes>"
transformations: []
redistribution_allowed: false # verify, never guess
attribution_location: "THIRD_PARTY_NOTICES.md"
```

This is a documentation template, not a claim that `assets/example.png` exists or is distributable. Avoid adding a false blanket `redistribution_allowed: true` across unknown assets.

## 5. Release gates

Before claiming a repository is ready for public reuse, review the top-level LICENSE, checked-in vendored code, binary dependencies, sample game assets, datasets and download bundles. Retain required upstream notices and verify the project README accurately states which parts are covered by its own license.

Before a game release, confirm itch.io packaging does not include unauthorized commercial ROMs or unlicensed artwork. Before a DCC extension release, check GPL requirements and native application SDK constraints. Before research-data release, verify dataset and paper-specific licenses separately.

**Verification state:** top-level license-file presence and stated top-level terms were inspected on the teaching branches. No complete third-party dependency or media provenance audit, native package review, or legal certification has been performed.
