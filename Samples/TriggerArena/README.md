# Trigger Arena

An ordinary Kairo game project demonstrating proximity-driven visual interactions. The player starts within the authored sphere: the left beacon disappears and the right beacon appears. Walk outside the sphere to reverse the effects; return to repeat. No separate game executable or custom runtime script is required.

`Config/Triggers.ktrigger` uses `kairo-triggers 1`, followed by `trigger ID X Y Z RADIUS EDGE TARGET ACTION`. EDGE is `enter`, `exit`, or `once`; ACTION is `show` or `hide`. Rules run in ascending numeric ID order. Coordinates are world-space sphere centers and radius. Targets must be explicitly tagged `kairo.trigger-visual`, root visual leaves without physics, camera, light, environment, audio, attached logic or collectible roles. Only the primary player's sampled position drives rules; thin-zone crossings between samples are not swept. Menus pause sampling, and one-shot state is session-local.

Run `build/game-clang/Runtime/KairoPlayer/KairoPlayer Samples/TriggerArena/Project.kproject`. Native acceptance: `python3 Samples/TriggerArena/run_native_acceptance.py build/game-clang/Runtime/KairoPlayer/KairoPlayer Samples/TriggerArena metal` (or `vulkan` with display support). The native gate checks entry effects and nonuniform capture, not visual pixel identity or an interactive exit/re-entry journey.
