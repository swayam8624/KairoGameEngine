# KairoBridge — Cross-Engine Migration Architecture

KairoBridge exists so an existing Unity, Unreal Engine, or Godot production can move into Kairo incrementally instead of being rewritten from zero.

The migration contract is intentionally stronger than asset import. A successful migration preserves source identity, hierarchy, references, authored semantics, and enough behavior for the project to become playable quickly. Engine-specific behavior that cannot yet become native Kairo code is represented explicitly through compatibility layers rather than silently discarded.

## Pipeline

```text
Unity / Unreal / Godot project
            |
            v
     source adapter
            |
            v
 Kairo Canonical Project IR
            |
            v
 semantic compatibility resolver
      /           |           \
 native       compatibility   attention
      \           |           /
            v
       Kairo project
            |
            v
 incremental native conversion
```

The canonical IR is the architectural boundary. Kairo must not build a web of direct Unity->Kairo, Unreal->Kairo, and Godot->Kairo one-off converters. Each source adapter translates its engine into stable engine-neutral concepts; Kairo consumers translate those concepts into native runtime/editor systems.

## Stable source identities

Every imported object carries a `SourceIdentity` containing the source engine, a durable engine identifier where available, and project-relative provenance.

Expected durable identifiers include:

- Unity `.meta` GUIDs.
- Unreal package/object paths and other stable package identities.
- Godot resource UIDs.

KairoBridge owns a stable `CanonicalID` for each source identity. The migration manifest then maps that canonical identity to the resulting Kairo asset/entity identity. This mapping must survive reimport so changing one texture, prefab, material, or scene does not invalidate references throughout the migrated project.

## Migration dispositions

Every translated concept is classified as one of four states:

- **Native** — represented directly by a Kairo subsystem.
- **Compatibility** — playable through a source-engine compatibility facade, with a future conversion path to native Kairo.
- **NeedsAttention** — information is preserved but a developer decision is required before runtime parity is guaranteed.
- **Unsupported** — the source behavior is understood but Kairo does not currently provide an executable representation.

A migration report should expose both runnable coverage (`Native + Compatibility`) and native-Kairo coverage. The long-term workflow is to get a project running first, then monotonically increase its native percentage.

## Canonical semantic surface

The first IR schema reserves concepts for projects, worlds, streaming cells, scenes, entities, prefabs, meshes, materials, textures, shaders, skeletons, animation clips/graphs, cameras, lights, colliders, rigid bodies, joints, character controllers, audio, navigation, terrain, particles, scripts, visual scripting, UI, input actions, data tables, gameplay tags, and other source concepts.

Not every reserved kind is already a complete Kairo runtime feature. Preserving an unsupported concept in the IR is preferable to destroying it during import.

## Adapter sequence

### Unity

First production adapter targets:

1. project/version metadata and package manifest;
2. `.meta` GUID graph;
3. scenes and prefabs;
4. meshes, textures, standard/URP/HDRP materials;
5. transforms, cameras, lights, rigid bodies and colliders;
6. animation clips/controllers;
7. input actions, NavMesh and audio;
8. MonoBehaviour metadata plus a .NET/Unity API compatibility strategy;
9. Shader Graph/VFX Graph translation where structurally possible.

### Unreal Engine

First production adapter targets:

1. `.uproject` and plugin descriptors;
2. package/asset registry metadata;
3. maps, actors and component hierarchies;
4. static/skeletal meshes, textures and materials;
5. animation assets/state machines;
6. physics, navigation, audio and input;
7. Blueprint graph translation into Kairo graph IR;
8. World Partition/data-layer/HLOD intent;
9. Unreal C++ classification into portable C++, compatibility API, or manual migration.

Binary `.uasset` support must be version-aware and must never pretend an unknown serialization version was converted safely.

### Godot

First production adapter targets:

1. `project.godot` and project settings;
2. `.tscn` / `.tres` text resources;
3. node/scene/resource identity and inheritance;
4. meshes, materials, animation, physics, navigation, audio, input and UI;
5. GDScript/C# metadata and compatibility/conversion paths.

## Validation strategy

KairoBridge will be tested against two classes of projects.

**Synthetic contract fixtures** stay small and deterministic. They deliberately isolate difficult cases such as cyclic prefab references, missing source assets, nested scenes, custom materials, animation graphs, plugin declarations, world streaming, and incompatible scripts.

**Real open-source games** exercise complete production graphs. Every external corpus entry must record upstream URL/revision, source-engine version, license, migration adapter version, expected coverage, and a reproducible validation command. Test infrastructure should fetch or reference those projects rather than copying content into Kairo when the upstream license does not permit redistribution.

A real migration is not accepted merely because it parses. For each corpus project we progressively require:

1. deterministic discovery;
2. deterministic canonical IR;
3. reference-complete import;
4. editor load;
5. Player boot;
6. behavior/render/physics checkpoints;
7. save/load consistency;
8. packaged-game smoke run;
9. performance comparison against the original engine where a reproducible baseline exists.

## Long-term compatibility principle

Compatibility layers are an entry ramp, not the permanent architecture. A migrated game should be able to run early through compatibility facades and then convert subsystem by subsystem to native Kairo. KairoEditor should eventually expose conversion actions and a live project score such as:

```text
Runnable migration coverage: 98.7%
Native Kairo coverage:       73.4%
Compatibility:               25.3%
Needs attention:              1.0%
Unsupported:                  0.3%
```

The end goal is that switching engines becomes a controlled engineering migration rather than a multi-year rewrite.
