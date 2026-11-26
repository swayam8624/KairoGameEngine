# Native M50 sample acceptance

Configure a clean exact-lock workspace with the existing game-player-clang
preset and native renderer dependencies. Build and run the eight native
sample gates together:

    # macOS (Metal)
    python3 scripts/run_m50_native_acceptance.py --build build/game-player-clang --evidence build/m50-native-first
    # Linux (Vulkan software or hardware device and working display)
    xvfb-run -a python3 scripts/run_m50_native_acceptance.py --build build/game-player-clang --evidence build/m50-native-first

The runner binds the verification to the configured CMake source and sibling
roots, verifies the workspace.lock revisions, builds KairoGameLab, KairoSimulationLab, KairoCoopLab and KairoCoopClient, requires exactly eight named CTest gates, and rejects
missing, failed or skipped tests. It retains build output, discovery JSON,
CTest log and JUnit XML. A success summary records engine commit, lock digest,
binary hashes and gate times. Use a new evidence directory per run.

The locked procedural terrain workflows invoke this runner on Vulkan and
Metal. CI also checks Vulkan validation diagnostics. This is sample-level
native acceptance, not complete engine release certification. Use a fresh
build directory for certification; an incremental build is not equivalent
to an isolated clean-machine package test. CPU-only fixture tests verify
the report checker, not the GPU.

Local status: expanded portable suite passed 56/56 commands per compiler,
28 execution gates each and 58 matching source hashes. Native report checker
passed four actual CMake/CTest fixture tests. Native renderer and full
workspace builds were not performed locally because the locked sibling
sources and platform toolchains are unavailable here.
