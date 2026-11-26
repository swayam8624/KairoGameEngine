# Animation lab — two-bone IK

Build from the integration root:

    c++ -std=c++23 -Wall -Wextra -Werror Samples/AnimationLab/TwoBoneIKDemo.cpp -o /tmp/kairo-ik
    /tmp/kairo-ik

Enter three coordinates per target. The two segments preserve their lengths;
unreachable goals clamp to the inner or outer shell. Try 1 1 0, 10 0 0, and 0 0 0.
The solver returns positions rather than local joint rotations. Read the
glTF animation runtime and skin palette contract before integrating into a
rendered rig; the lab is not a skinning acceptance result.

Read [the solver](../../Runtime/KairoPlayer/RuntimeTwoBoneIK.hpp), then
[its geometry tests](../../Runtime/KairoPlayer/tests/RuntimeTwoBoneIKTests.cpp),
then the demo. The triangle law of cosines determines the distance along the
target ray; the pole chooses the perpendicular direction for the joint.

## Compatible skeleton retargeting

    c++ -std=c++23 -Wall -Wextra -Werror Samples/AnimationLab/RetargetDemo.cpp -o /tmp/kairo-retarget
    /tmp/kairo-retarget

The sample plays five CPU poses of a synthetic clip on a longer target skeleton. Root displacement scales by two; the target arm stays length two. The algorithm applies local rest-relative quaternion deltas by name and parent identity. Compatible axes, single-root parent-first skeletons only; imported glTF rig adapters, different bone axes and native skin presentation remain open.

## Imported glTF skin correction

RuntimeRenderBridge::SetTwoBoneIK(entity, SceneTwoBoneIK{{root,middle,tip},{targetX,targetY,targetZ},{poleX,poleY,poleZ}}) applies a correction after clip sampling. Call ClearTwoBoneIK to restore ordinary playback. The same shared extraction can accept SceneAnimationOverrides::SetTwoBoneIK(entity,assetID,request) from an editor host. Nodes must form a direct glTF TRS chain, with unit sampled scale, and root/middle must belong to the same skin. The target/pole are in root-parent space, not entity/world space. The original pose and shared binding remain immutable. No automatic bone mapping or world-space control yet; imported scaled chains are rejected. Actual shared-module tests registered; the portable adapter passed both compilers with structural fixtures, not the full glTF importer/GPU.

## Native imported skin acceptance

After building KairoImportedIKSmoke in a locked native workspace, run:

    python3 Samples/AnimationLab/run_imported_ik_native.py build/game-player-clang/Runtime/KairoPlayer/KairoImportedIKSmoke metal

Use vulkan under xvfb-run on Linux. The Python driver generates a fresh original embedded GLB (six vertices, three joints, four triangles, one translation clip), with no external files or downloads. Actual importer, derived artifact, mesh upload, clip sampling, post-clip IK, palette, native pixels and object picking are required; clearing IK must restore the first capture. The independent Python byte tests passed locally. The native binary/actual asset importer has not run locally. This is a native test fixture, not a general character editor.
