# Kairo Physics Playground

Create **2D Physics Playground** in the Hub, or open this project's Project.kproject in the World2D editor. Run Player with the supported Vulkan or Metal renderer. Use WASD/arrows, P to pause, R to restart, N for new game, Escape to quit. Two physics-enabled props fall, slide and collide with foreground tiles; collecting coins and reaching the goal still use the existing adventure controller.

## Physics authoring

Choose Physics in the World2D editor. Enabling the configuration writes a version-3 world. Existing version-1 and version-2 worlds remain unchanged unless explicitly enabled. The native validator checks every body and joint before the configuration is applied. Save the document after applying; undo and redo work as for other scene edits.

```text
physics 0 180
bodies 2
6 dynamic box 2 0.3 0.7 4 3 0 0 0
8 dynamic box 1 0.2 0.5 4 3 0 30 0
joints 0
```

A body record contains actor ID, motion (static/dynamic/kinematic), shape (box/circle), mass, restitution, friction, layer, collision mask, sensor flag, initial X/Y velocity. Coordinates are pixels, velocities pixels/s, accelerations pixels/s². Collision layers are bit masks: map = 1, player = 2, authored prop = 4 in this example. To make the props also collide with one another, change both masks from 3 to 7. A distance joint is two actor IDs and rest length in pixels; at least one endpoint must be dynamic. Circles require square decor bounds. Solid actors must be boxes with map/player collisions enabled; they cannot be sensors or kinematic, since the legacy player controller still treats solid actor bounds as obstacles. Kinematic decor can be given a velocity. Physics sensors report transitions in the C++ core, but are not yet exposed to authored scripts. There is no angular dynamics or general convex polygon support. Eight bounded substeps at 60Hz limit integrated motion; this is not general continuous time-of-impact collision detection. The player remains a separate kinematic controller; it does not directly apply pushing impulses.

The native parser caps authored bodies at 128, distance joints at 64, and the complete collision world including tile runs and boundary walls at 1024 bodies. A dense map may need simpler collision geometry to fit that bound. Clear the Physics panel and validate/apply to disable physics. The original game remains version 2 when saved.

## Custom game UI

Choose **Game UI designer** in the World2D editor to edit the project-wide Config/UI.kui. The sample contains 15 widgets across play, pause and completion pages. The canvas supports anchored positioning, drag-to-move, aspect previews (16:9, 4:3, 9:16 and 21:9), widget paint order, property editing, color selection and undo/redo. Save UI runs the native validator before atomic publication; a stale or invalid edit leaves the old file intact. Preview typography uses Tk's font while the Player uses a bounded 5x7 bitmap alphabet, so it is not a pixel-perfect GPU preview.

Use the mouse on buttons, or Tab/Shift+Tab and Enter on pause and completion pages. Counters bind to coins, deaths and campaign level; the coin bar reads the actual objective progress. The campaign title binding reads the current project title. Buttons invoke existing campaign operations (pause, resume, restart, new game and quit). The document is optional: without it the old interface remains. Cosmetic edits do not invalidate saved campaign progress. UI is flat rather than a nested layout hierarchy; no general script callbacks, health/inventory model beyond coins, text input, arbitrary fonts or Unicode yet.

## Audio Studio

Open **Audio studio** in the World2D editor. Config/Audio.kaudio is optional and project-wide. Master, effects, music and ambient volume controls update the document; save validates every cue, actor, level and decoded clip. The sample ships original synthesized pickup/damage/victory sounds and an actor-attached hum. No third-party soundtrack is bundled.

Import a mono/stereo WAV under a new lowercase clip ID; the actual shared WAV importer preflights staged bytes before exclusive publication to Assets/Audio/<id>.wav. Set a music cue to that ID for looping background playback, or map it to an event. Preview auditions up to three seconds through the native audio device, at fixed preview gain; it is an isolated audition, not the complete spatial scene mix. Imported files are retained if you discard the unsaved audio document.

```text
kairo-audio2d 1
mix 70 80 35 35
clips 4
pickup tone 660 90
damage tone 160 160
victory tone 880 250
hum tone 220 1000
collect pickup 75
death damage 85
complete victory 80
music - 70
emitters 1
main 8 hum 35 96
```

The four mix percentages are master/effects/music/ambient. Clip records are ID wav or ID tone frequency milliseconds. Cues use ID and gain percentage; "-" disables an event/music cue. Emitter records are level name, actor ID, clip ID, gain, radius in world pixels. Emitters follow active actors and stop when the actor disappears or the level changes. The player is the listener. Pausing freezes music and ambient cursors and clears pending effects (previously queued PCM may drain). Older projects without the audio document keep their original synthesized event sounds. Audio changes do not invalidate gameplay checkpoints.

The native bank supports up to 32 clips, 16 emitters, 32 simultaneous voices, 32 MiB per WAV source, 64 MiB aggregate WAV bytes, 128 MiB decoded sample storage and 120 seconds per WAV. WAV PCM 8/16/24/32-bit and IEEE float32 mono/stereo use the existing KairoAssets decoder; compressed/extensible WAV, MP3, Ogg, FLAC, streaming music, audio DSP and custom bus graphs are not supported by this authoring path. Native playback depends on available Audio Queue, ALSA or WinMM output; the headless mixer remains usable without a device. Runtime source files are packaged with the project; the World2D bank uses its own file naming contract, not a 3D asset registry or derived cache.

## Gameplay logic

Open **Gameplay logic** in the World2D editor. Config/Logic.klogic2d is optional, level-scoped, native-validated and compiled into shared EngineCore VM commands where applicable. This sample uses:

```text
kairo-logic2d 1
rules 7
main begin print "Physics playground ready"
main timer 60 add 0 1
main collect add 0 10
main death add 1 1
main input Jump impulse 6 0 -60
main ui pause add 2 1
main complete sound 2
```

Each rule has a level, event, optional event selector, and one command. The events are begin, tick, collect, death, complete, input ACTION (press only), sensor_begin ID, sensor_end ID, ui BUTTON_ID, timer TICKS. Timers run on fixed simulation ticks, not wall clock; 60 ticks = one second. Collection signals once per tick if collection increased. Sensor transitions are coalesced by sensor actor and begin/end kind within each tick, without other-actor identity. Begin runs on level entry, restart and death reset. Commands are print quoted message, position DECOR_ID X Y, impulse DYNAMIC_ID X Y, add SLOT INTEGER, set SLOT INTEGER, sound EVENT_INDEX (0 collect, 1 death, 2 complete). Sound commands invoke existing event cues, and may deliberately retrigger built-in event audio.

There are 64 rules maximum, 16 signed integer slots, saturated to ±1,000,000. Values survive level transitions and death/restart, are saved when the campaign checkpoint is written, and clear on New Game. No mid-level autosave. Use the UI counter binding "script" to display slot 0. Position is limited to in-bounds non-physics decor; impulses target authored dynamic bodies. The engine validates input names, button IDs, sensor and actor references before launch or packaging. A changed script invalidates a stale checkpoint revision. The editor provides a text rule authoring panel, not a 2D visual graph. Run Player with --world2d-logic-trace to emit bounded rule-level console trace; this is not the 3D live debugger or a full instruction trace. The adapter reuses EngineCore LogicInstance for print/position/impulse; the World2D scheduler owns events, counters and sound commands. There are no general control-flow constructs, arbitrary user code, spawning, custom native plugins or scripting API parity with 3D yet.
