# One authored Entity through ECS, physics, animation and rendered presentation

This is an end-to-end laboratory for the actual Kairo integration source. Open [RuntimeProject.cppm](../Runtime/KairoPlayer/RuntimeProject.cppm), [KairoSceneECSBridge.cppm](../Runtime/KairoSceneECSBridge.cppm), [RuntimePhysicsBridge.cppm](../Runtime/KairoPlayer/RuntimePhysicsBridge.cppm), [RuntimeEntityLifecycleBridge.cppm](../Runtime/KairoPlayer/RuntimeEntityLifecycleBridge.cppm), [RuntimeRenderBridge.cppm](../Runtime/KairoPlayer/RuntimeRenderBridge.cppm), [RealtimeSceneBridge.cppm](../Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm) and [KairoPlayer/main.cpp](../Runtime/KairoPlayer/main.cpp).

The point is to understand where *one stable authored object* acquires temporary runtime identities—and where those identities must be invalidated during removal, replay or a project reload. The superbuild connects independently versioned repositories; the source SHA in workspace.lock must be accounted for when reproducing a run.

## 1. The project descriptor starts the trust boundary

A player receives a .kproject file. RuntimeProject resolves the actual descriptor path with weakly_canonical, verifies the extension and regular-file status, then loads the shared EngineCore ProjectDescriptor.

The descriptor identifies portable relative paths for the asset manifest, startup scene and input map. RuntimeProject::RequireProjectFile normalizes each path, canonicalizes it against the project root and compares filesystem **components** for containment. Comparing only string prefixes would be unsafe: a root /game must not accept /game-evil/file as a child. Following symlinks before the containment test matters when a seemingly innocent relative path points outside the directory.

The **asset manifest must load before the scene**, because typed mesh/material/texture references in the authored scene must resolve to known persistent AssetIDs.

**Exercise:** make two project roots, A and B, then a symlink inside A pointing to a scene in B. Reason through why a canonicalized containment check should reject it. Repeat with a valid child path that contains subdirectories and identical textual prefixes.

## 2. Identity exists at several levels

Consider authored entity E=42, a dynamic character whose MeshRenderer references a stable MeshAsset ID and whose Collider/RigidBody describe physics.

| Layer | Example identity | What it means | Lifetime |
| --- | --- | --- | --- |
| EngineCore Scene | Entity.Value=42 | authored persistent object | saved .kscene / project |
| KairoECS | Entity(Index,Generation) | process-local component slot | Registry lifetime |
| PhysicsWorld | BodyID | simulation rigid body and solver state | PhysicsWorld active body lifetime |
| Renderer | MeshHandle | uploaded native mesh | RendererRuntime lifetime |
| Picking | ObjectID=42 | maps hit pixel back to authored Entity | current frame/scene |

Do not serialize ECS generations, PhysicsWorld body-array indices or Renderer MeshHandle numbers as authored object identity. The adapters explicitly maintain mappings.

SceneECSMirror::Extract creates ECS entries for all authored scene entities first, attaching AuthoredEntityIdentity, RuntimeName, RuntimeTransform and RuntimeSceneState. A second pass resolves parent links into the new ECS handles. This handles forward references, because a parent need not appear before a child.

After extraction, the bootstrap structural journal is cleared. RefreshAuthoredState later updates only authoring-owned values on **existing** ECS handles; adding/deleting entire entities needs another topology reconciliation path.

**Exercise:** create three authored entities with IDs 42, 7 and 99, where 42 is child of 99. Trace extraction's two passes and compare stored parent runtime handle against the independent original stable parent ID.

## 3. Why the physics bridge owns two maps

RuntimePhysicsBridge constructs PhysicsWorld from active authored collider/rigidbody descriptors. An entity with only an authored Collider is represented as a static body; dynamic mass/inertia depends on primitive shape, scale and density. PhysicsWorld still owns solver events, impulses, broadphase proxies and caches.

For every created body, the bridge maintains **Entity.Value -> BodyID** and **BodyID -> EngineCore Entity**, plus previous/current pose history. A contact callback from PhysicsWorld contains BodyIDs; CaptureContact translates them to Scene Entity IDs before gameplay sees the event.

These mappings are bidirectional because gameplay starts with authored identities but the solver reports internal IDs. A missing reverse mapping is a lifecycle invariant violation, not a safe default to Entity 0.

**Exercise:** create two dynamic bodies and one static floor, then record authored IDs, BodyIDs and collision Begin/Stay/End event identities. Deactivate one streamed object and verify its stale contact events cannot leak into gameplay.

## 4. Derive the fixed-step accumulator and interpolation

The rendering loop supplies a variable elapsedSeconds. RuntimePhysicsBridge::Advance clamps one frame to MaximumFrameDeltaSeconds, adds it to an accumulator and runs as many FixedDeltaSeconds steps as fit, up to MaximumSubsteps.

For each substep, it saves Previous pose, invokes optional BeforePhysicsStep listener, calls PhysicsWorld::Step(dt), then synchronizes Current poses. It subtracts dt from the accumulator.

**Worked example:** physics dt=1/60 second (~16.667 ms), one render-frame delta=40 ms, cap=4. Two physics steps execute (~33.333 ms) and roughly 6.667 ms remains, so interpolation alpha = remaining/dt ≈ 0.4.

The bridge publishes an interpolated pose between Previous and Current. This does **not** perform another physics solve or modify PhysicsWorld's authoritative velocity/contacts. It updates render-facing Scene local transforms, processing hierarchy parent-first so world-to-local conversion sees the updated parent.

If the substep cap is reached with at least one full step remaining, it keeps only the fractional remainder and reports DroppedExcessTime. This bounded behavior avoids a spiral of death but **drops simulated elapsed time**, so physics time need not keep up with wall-clock time under prolonged overload.

**Exercise:** simulate frame deltas [8,8,24,40] milliseconds using the same dt/cap and manually compute step count and alpha after each call. Compare against RuntimePhysicsBridge output. Then repeat with a 1-second debugger pause and inspect cap behavior.

## 5. Physics topology changes require transactional rollback

World streaming can activate or deactivate entity physics after bridge construction. ActivateEntities and DeactivateEntities snapshot the world state **and** copies of both identity maps, pose history and entity contact events.

If any operation throws, RestoreTopologyTransaction restores all copies and reinstalls the contact callback. Rolling back only PhysicsWorld while leaving Entity-to-BodyID mappings changed would silently misroute later collision events. Likewise, restoring only maps without world state could make stored BodyIDs invalid.

RestoreSnapshot validates an incoming PhysicsWorld snapshot against the **current running topology** before publishing it. It resets interpolation Previous=Current after loading, avoiding a visible lerp from a pre-save position to the restored location.

**Exercise:** append two entities where one collider is invalid, attempt ActivateEntities, then check entire world snapshot, mappings and pending contact event collection against the pre-call state.

## 6. Spawning a fragment is a dependency-ordered transaction

RuntimeEntityLifecycleBridge coordinates extra per-entity runtime services. Its profile permits character motor, navigation, crowd and locomotion animation under strict dependencies: navigation needs a motor, crowd needs navigation, animation needs crowd.

Activation order is **Scene -> Physics -> CharacterMotor -> Navigation -> Crowd -> Animation**. Failure rolls back successfully registered services in reverse order. Physics is deactivated only if this particular bridge owns its activation; startup or other streaming subsystems may own a body instead.

SpawnFragment first appends EngineCore SceneComposition records with new stable destination Entity IDs and a source->destination mapping, then activates physics and selected gameplay profiles. The returned fragment token represents session-local ownership of the appended group, not a persisted Entity ID. If a later service registration fails, it tries to remove the partial services, physics and appended Scene fragment.

DestroyFragment validates removal on a **candidate Scene copy** first, then performs transactional physics deactivation, unregisters services in reverse and publishes the candidate Scene. Fragment tokens are invalidated by cold save-game restore; surviving authored entities can be adopted into new ownership later.

**Exercise:** spawn an NPC with motor, navigation and crowd. Attempt another profile that requests crowd without navigation; it must fail before allocating resources. Then remove the NPC fragment and inspect active Scene, Physics and service counts for stale state.

## 7. Renderer asset handles are not stable AssetIDs

RuntimeRenderBridge owns the actual RendererRuntime mesh and texture handles it creates. Its RenderAssetBindings maps persistent typed KairoAssets IDs to these temporary native handles. If LoadAssets throws, the bridge releases already-created handles. Its destructor releases them in reverse order; RendererRuntime must outlive the bridge.

A texture's interpretation is part of its import configuration. The same stored image bytes may be used as sRGB base color or linear normal/roughness data. EnsureTexture checks that one AssetID is not bound with incompatible color-space or normal-map semantics under one handle.

LoadAssets resolves material textures first, then builtin/imported meshes and glTF scene primitives. The derived-data cache stores CPU import artifacts, **not GPU allocations**. RendererRuntime::CreateMesh and CreateTexture perform native allocation.

**Exercise:** register one texture as both base-color sRGB and normal-map linear for the same asset ID. The bridge should reject incompatible semantics rather than silently reuse a wrong GPU upload.

## 8. Extract one draw object

RealtimeSceneBridge::BuildRenderScene traverses active, visible and layer-matching authored entities. For a MeshRenderer it resolves typed mesh/material bindings, composes Scene::WorldTransform into a model matrix and assigns ObjectID=EngineCore Entity.Value. This is why an Editor picking result can recover authored selection identity without treating MeshHandle as Entity ID.

For a multi-primitive SceneInstance, each primitive has an asset-local transform and possibly glTF node/skin metadata. An unskinned animated primitive uses the outer authored world transform **times the evaluated glTF node world matrix**.

A skinned primitive uses outer authored world as Model and places jointWorld*inverseBind matrices in an asset-space skin palette. Multiplying animated mesh node world a second time would double transformations and distort the character. The code caches one palette per SkinIndex during extraction.

The same bridge converts authored photometric units into backend-neutral RenderLight intensity so Editor and Player do not disagree: directional source intensity uses 10,000 as scale; local light sources use 100. These are engine calibration factors, not a proof of absolute physical radiometry.

**Exercise:** construct a two-node glTF character with one skinned mesh. Compute the rest pose and one animated pose, then compare the expected world-space vertex transform using skin palette and Model. Confirm the node world transform was not applied twice.

## 9. Animation time belongs to gameplay

RuntimeRenderBridge::SetAnimation validates clip index and playback rate. Choosing the already active clip preserves its clock unless restart is explicitly requested. Repeatedly selecting walk every frame therefore does not restart the cycle.

StepAnimations advances explicit gameplay-owned clocks or a compatibility loop of clip zero. It then publishes playback overrides; the shared RealtimeSceneBridge handles animation pose/skin sampling. If an authored scene instance disappears, stale animation states are cleared. An instance without glTF animation cannot be given an arbitrary clip.

**Exercise:** set walk at t=0, advance 0.4 seconds, then select the same walk clip without restart. Confirm time continues from 0.4. Repeat with restart=true and then with a clip change. Check looping and clamping at clip end.

## 10. Three independent acceptance gates

Pure EngineCore or ECS tests can prove authored mapping and mutation semantics. PhysicsWorld tests can prove shapes, contacts and solver stability. Native Editor/Player graphics tests prove actual GPU allocation, image output, readbacks, material/skin behavior and clean teardown. A passing one is not proof of all three.

Begin with [SceneECSBridgeTests.cpp](../tests/SceneECSBridgeTests.cpp), [RuntimePhysicsTopologyTests.cpp](../Runtime/KairoPlayer/tests/RuntimePhysicsTopologyTests.cpp), [RuntimeEntityLifecycleBridgeTests.cpp](../Runtime/KairoPlayer/tests/RuntimeEntityLifecycleBridgeTests.cpp), [SharedContentContractTests.cpp](../tests/SharedContentContractTests.cpp), and the graphics/Player tests indexed in [SOURCE_READING_ORDER.md](SOURCE_READING_ORDER.md).

For reproducibility capture the component SHA tuple from workspace.lock, host/toolchain, runtime backend, exact fixture and test commands, number of fixed steps, dropped backlog, contact oracle, asset import settings, and visual evidence. The portfolio's “95%” label is a *target*, not an acceptance result.

*Documentation-only lesson. No new C++ compiler run, native GPU test or portfolio acceptance campaign was executed as part of this pass.*
