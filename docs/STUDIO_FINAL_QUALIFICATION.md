# Cumulative Studio qualification

PR #206 merge commit 38f0026de9f9807fd5b3797fe2c1c460bc43658a, workflow 46063866390: Linux Clang 276/276 (job 140724375143), Windows MSVC 229/229 (140724375093), Linux ASan/UBSan 245/245 (140724375049), integrated compute passed. Signal Vault verified archive artifacts 15232311760 (Linux, Vulkan smoke) and 15229903781 (Windows, validation), both with native import inventories. Linux and Windows licenses/lock raw hashes differ across checkout line endings; the candidate now pins LF for workspace.lock and Samples/SignalVault. Requalify the new checkout before comparing receipts.

macOS job 140724375162 was cancelled during build at 1136/1792 steps, approximately 45 minutes into the job. There is no compiler error in that log. The macOS job budget is now 120 minutes; the native test configuration has not been reduced. It must complete before qualification.

## Component license evidence

Exact pinned LICENSE blobs retrieved for KairoMath, KairoEngineCore, KairoRenderer and KairoEditor: f745f2afcc7c868e25c04be0d8e43350bc57d2e6 (MIT). KairoAI: 5c7c6f3c3ed53872c9e9b9196acb1ab58d8cdb82 (MIT). Exact root LICENSE retrieval returned 404 for KairoGeometry, KairoSpatial, KairoPhysicsMath, KairoPhysicsEngine, KairoAssets, KairoECS, KairoReflection, KairoRayTracer. This does not prove absence of licensing; alternate paths and pinned source history require checking. Do not infer license from current main or silently apply a license to third-party code.

The repository MIT notice is present in Signal Vault. Static/transitive third-party licensing, dependency bundling, signing and clean-machine installation remain unverified. No main promotion or public release claim. Prior qualification applies to the recorded merge commit, not automatically to subsequent changes.
