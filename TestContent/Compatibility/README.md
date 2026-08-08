# Kairo Compatibility Content

This directory is a permanent, license-audited input corpus for KairoAssets,
KairoRenderer, KairoEditor, KairoPlayer, and KairoRayTracer. It is test content,
not engine-owned artwork and not a claim that every consumer supports every
feature yet.

## Committed Corpus

| Asset | Purpose | Current acceptance |
| --- | --- | --- |
| Toy Car | hierarchy, multiple primitives/materials, PBR extension fallback | KairoAssets scene import must produce validated nodes, materials, and meshes |
| Metal-Rough Spheres | deterministic metallic/roughness material grid | KairoAssets scene import must preserve material factors and geometry |
| Light Visibility | deliberately newer light/animation extensions | import must either succeed or emit a controlled unsupported-content diagnostic |
| Kloppenheim 06 Pure Sky | linear HDR texture and environment fixture | KairoAssets texture import must produce finite RGBA16F HDR mips |
| Invalid glTF JSON | deterministic malformed-input diagnostic | glTF import must reject the file as invalid JSON without crashing |

Every original binary is accompanied by source, author, license, access date,
byte size, and SHA-256 data in `ASSET_LICENSES.json`. Per-model Khronos license
and metadata files are retained beside the binaries. The corpus checksum test
detects accidental replacement or corruption.

The malformed glTF is a 39-byte Kairo-authored test fixture rather than
third-party artwork. Its provenance and expected failure are still recorded in
the same manifest so every committed corpus input remains auditable.

## Optional Large Content

The CC-BY chess scenery and full CC0 Kenney Nature Kit are intentionally not
stored in Git. Fetch their exact reviewed revisions into the ignored `optional`
directory with:

```bash
cmake -P TestContent/Compatibility/fetch_optional.cmake
```

The script requires the reviewed SHA-256 hash. If an upstream file changes, the
download fails instead of silently accepting new legal or technical content.

## Consumer Matrix

The current phase validates source integrity and KairoAssets decoding. Later
phases must add explicit rows for live rendering, editor instantiation,
packaging, and offline rendering. An unsupported feature is a passing result
only when the diagnostic identifies it; silently dropping authored data is not.
