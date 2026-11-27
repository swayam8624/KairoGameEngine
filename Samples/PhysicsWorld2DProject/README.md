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
