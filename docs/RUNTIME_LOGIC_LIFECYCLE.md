# Runtime script lifecycle and scheduling

KairoPlayer instantiates one existing LogicInstance per authored enabled logic component, even if the owner starts inactive. Each instance references the source asset's verified compiled artifact. Independent owners may attach the same document; scheduling and first-activation state belong to the instance, not the shared source.

## Lifecycle

- BeginPlay executes once, on the first observed active owner with an enabled matching script component. Calling BeginPlay again does not replay initialization.
- An inactive hierarchy, disabled component, missing owner, or changed document binding suppresses subsequent dispatch. A later observed reactivation of the same prevalidated binding resumes its original instance.
- An observed inactive interval clears partial scheduled tick time. No inactive time or missed input/contact events are replayed.
- Input and collision events use the same activation guard but are not throttled by tick cadence.
- Initially disabled script components, newly created or rebound script components, hot reload and entity-ID reuse are outside this startup binding contract. Changes occurring entirely between dispatch observations are not detected. Existing debugger and source fingerprints remain in effect.

## Authoring a lower-frequency Tick

Optionally add `Config/LogicSchedule.klogic` to a 3D project:

```text
kairo-logic-schedule 1
tick 42 3
tick 57 6
```

Each row identifies an authored entity with an enabled logic component and a cadence of 1–60 physics steps. At 60 Hz the examples deliver Tick at 20 Hz and 10 Hz respectively. The VM receives the sum of elapsed active fixed-step durations. Omitted entities retain the default cadence of one step. The native Player rejects unknown IDs, duplicate rows, invalid intervals and oversized documents during preflight. This is a simulation-step policy, not an independent timer, arbitrary coroutine API, or editor UI.

## Qualification

The standalone KairoPlayerLogicScheduleTests target exercises the parser, timing, first activation and suspension under seeded schedules. The existing native RuntimeProjectTests target exercises the actual compiled VM through the physics/scene host, including delayed activation and cadence across suspension and reactivation. Native CI and package qualification are separate from the portable test.
