# Mesh Particle Arena

Open Project.kproject in the existing Kairo editor or Player. The scene uses ordinary authored entities and mesh/material assets. Move with WASD, jump with Space, and quit with Escape.

## Authoring

- Tag exactly one entity `kairo.particle-emitter`. Its world-space translation is the emission origin; disabling it drains existing particles. The emitter can be a child of a moving entity.
- Add 1–128 disabled root mesh entities tagged `kairo.particle-marker`. They must have positive authored scales, no children, physics, logic, lights, cameras, audio, imported scene instances or other gameplay ownership tags.
- Change the mesh, material, or scale of the markers with the ordinary scene authoring controls. Slots are used in ascending entity-ID order. The sample contains 24 sphere markers.
- The generic native Player validates the pool before graphical startup and before packaging. The simulation runs at 60 Hz, emits every third tick, applies gravity, and shrinks particles over 48 ticks. Saturation drops new particles. Paused menu frames do not advance particles.

This first 3D effect uses existing scene extraction and ordinary mesh draws, without per-particle native resource allocation. It is not a billboard or GPU compute particle engine, has no particle collisions, supports one emitter, has fixed emission parameters and provides no live editor particle preview or dedicated material graph. Moving emitter origins are sampled once per game frame.

Run the registered `KairoPlayerParticles3DTests`, `KairoPlayerParticles3DBridgeTests` and `KairoPlayerMeshParticleArenaValidate` CTests to qualify the portable simulation and native scene adapter.
