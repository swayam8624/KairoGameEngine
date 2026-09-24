# KAIRO Research Tracks

Research progress is deliberately separate from the 95% engineering score.
A repository can be a complete v1 engineering artifact while a research
hypothesis remains unproven.

| Track | Owners | Question | Required evidence | Claim state |
| --- | --- | --- | --- | --- |
| R1 Telemetry-adaptive render graph | KairoRenderer | Can bounded online pass/resource telemetry improve frame-budget behavior over fixed graph policy? | fixed-policy baselines, trace replay, p50/p95/p99 frame cost, memory/transient pressure | unproven |
| R2 MORPH-ECS | KairoECS + KairoEngineCore | Can runtime query/churn telemetry choose sparse/archetype layouts better than one fixed representation? | sparse/archetype/fixed-hybrid/oracle baselines, migration cost, memory, adversarial phase changes | unproven |
| R3 Transactional heterogeneous worlds | KairoGameEngine + KairoEngineCore + KairoAssets | Can authored worlds update runtime subsystems transactionally while retaining stable identity and bounded rollback? | world-delta fixtures, failure injection, state fingerprints, rollback/locality metrics | unproven |
| R4 Adaptive collision pipeline | KairoPhysicsEngine + KairoSpatial | Can scene/shape/churn statistics choose broadphase/narrowphase policies under a fixed simulation budget? | fixed-policy baselines, collision correctness oracle, p95 step time, pair/contact counts | unproven |
| R5 Reversible AI-native authoring | KairoEditor + KairoAI + KairoMacPerception | Can plan/preview/approve/verify/undo make AI authoring measurably safer without destroying task throughput? | typed task suite, exact-call audit, rollback success, correction rate, completion time | unproven |
| R6 Cross-backend semantic differential rendering | KairoRenderer + KairoRayTracer | Can backend-neutral scene semantics plus reference rendering localize rendering divergence automatically? | canonical scenes, image/semantic buffers, backend/reference deltas, fault-injection localization | unproven |
| R7 Frame-budget-aware heterogeneous inference | KairoScheduler + KairoSIMD + KairoGPU + KairoONNX + KairoTransformers | Can online CPU/GPU selection with transfer/residency cost reduce missed interactive deadlines? | CPU-only/GPU-only/static/oracle baselines, latency tails, transfers, residency, deadline misses | unproven |

## Rules

- No research track is allowed to manufacture novelty from infrastructure.
- Baselines and falsification criteria are fixed before headline claims.
- Raw measurements remain machine-readable; figures are generated from them.
- Dataset/workload manifests record source, version, hash, license and conversion.
- Negative results remain documented rather than being deleted from the research record.
- Research claims never gate the v1 engineering completion score.
