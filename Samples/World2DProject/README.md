# 2D Adventure Starter

Create a 2D Adventure project from the Hub. This sample has a scrolling top-down first level and a second platform level. Collect all items and reach each goal to advance. Progress resumes at the beginning of the last unlocked level, not at the exact player position.

- WASD/arrows or the left gamepad stick: move.
- Space or gamepad A: jump in platform mode.
- P or gamepad Start: pause/resume.
- R or gamepad X: restart current level.
- N or gamepad Y: explicitly start a new campaign.
- Escape or gamepad Back: quit.

The editor has paintable tile layers, actor inspector, palette and indexed animation frames up to 32x32, PNG sheet import, open/clone level actions, campaign/audio settings and input bindings. Add a cloned level name to Config/Campaign.kcampaign in the desired order. The built-in native tools validate edits before publication. Authored campaign levels and controls package with the project; local progress in .kairo/progress is excluded. Changing authored campaign/world bytes invalidates old checkpoints; New Game explicitly replaces incompatible progress.

Event sounds are short synthesized tones with authored frequency, duration, and gain, mixed at 48 kHz. Native playback is optional and uses the platform audio backend. This is not a full audio workstation, arbitrary game scripting, or a textured 2D GPU batcher. Native Player defaults to Metal/Vulkan GPU texture atlas rendering with tile mesh batching, nearest filtering and reusable sprite frame quads. HUD/modal UI uses the existing overlay. Pass --world2d-rectangles for the legacy rectangle path. Individual actors still submit mesh draws; no general dynamic instanced sprite renderer is claimed. Native capture tests compare interior palette colors to the rectangle reference.
