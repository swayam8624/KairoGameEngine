# Teaching annotation standard

The goal is a repository a learner can **understand, reproduce, extend and challenge**. The standard applies to C++23 modules, C++, Swift, Rust, Python, shaders, CMake, tests and research scripts.

## Before writing a comment

Read the public interface, its implementation, callers, downstream consumers, test cases and current documentation. Identify the algorithm's invariant, failure boundary, lifetime model, representation conventions and observed rather than planned capabilities.

Explain in paragraphs, not just keywords. A meaningful teaching comment states:

1. **Purpose** — what architectural problem this type/function solves.
2. **Input contract** — units, shape, lifetime, valid ranges, expected order and failure behavior.
3. **Ownership and effects** — who allocates, mutates, synchronizes or persists state.
4. **Collaborators** — specific files, modules, imported APIs, callers and consumers.
5. **Mechanism** — why the algorithm works, especially nonobvious mathematical/graphics/physics steps.
6. **Evidence** — test, oracle, benchmark or native gate that validates it; state what is not tested.

Not every trivial getter needs six paragraphs; keep details proportional to conceptual difficulty.

## Example: C++23 renderer façade

```cpp
/// Converts a backend-neutral scene into a request for the selected GPU runtime.
///
/// Input: a RenderScene containing stable mesh/material handles produced by the
/// asset-to-render bridge. It does not own source glTF/texture bytes.
/// Effect: dispatches through RendererRuntime to Vulkan/Metal/OpenGL/D3D12;
/// the native backend owns GPU allocations, synchronization, and submission.
/// Consumer: KairoPlayer and KairoEditor. Verify: RendererTests and native
/// backend smoke tests for the actual platform.
void SubmitRenderScene(const RenderScene& scene);
```

Keep `export module`, `import`, global module fragments and `#if` platform guards semantically correct. A comment may be added before a declaration, but never between an expression and its required operand/initializer, where readability and macro behavior could suffer.

## Algorithms and numerical APIs

Document coordinate systems, handedness, row/column storage, vector-multiply convention, precision, condition numbers, tolerances and expected complexity. In physics, distinguish broadphase candidates from genuine contacts and discrete simulation from continuous physical truth. In graphics, distinguish a CPU render-graph stage profile from GPU timestamp evidence and a theoretical work estimate from measured end-to-end runtime.

For randomized ML, explain RNG/seed scope, data leakage boundaries, fit/transform semantics, optimizer state and reproducibility limits. For research, preserve exact dataset split, blinding, uncertainty and failure cases.

## Ownership and language ecosystems

- **C++:** spell out references, smart-pointer ownership, iterator invalidation, allocator arenas, RAII and thread synchronization.
- **Swift/macOS:** explain `Sendable`, actor isolation, host permissions, ScreenCaptureKit/Vision API contracts and main-thread UI boundaries.
- **Rust/Tauri:** distinguish serialization contracts, trust boundaries, path canonicalization, command authorization and subprocess invocation.
- **Python/DCC:** separate portable core logic from Blender/Houdini/Maya/Nuke host APIs and native-only tests.
- **GPU shaders:** explain binding layouts, resource hazards, color space, coordinate space, sampling/filtering and synchronization assumptions.
- **Build/test files:** explain targets, platform dependencies, integration gates, reproducible fixtures and why a skipped test cannot support a claim.

## Style and accuracy

Use short, self-contained explanations at file top for the big picture and focused detailed comments at the API/algorithm. Avoid paraphrasing identifiers, speculative completion claims, generic "handles data" statements, or massive repetitive comment banners. Add navigable links in Markdown guides; reference stable filenames in code comments.

Prefer **what the code actually does**, including limitations, over what the README hopes it does. If a defect is spotted, record it; fix behavior separately and test it. Never alter third-party source or presume redistribution rights simply to add comments. Existing research freezes must remain scientifically untouched.

## Review checklist

After edits, compare the source diff to ensure comments have not altered tokens or semantics, run formatting/lint checks where supported, run CTest/cargo test/swift test/pytest/native smoke on the proper host, and update the coverage ledger only after evidence exists.
