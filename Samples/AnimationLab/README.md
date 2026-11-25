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
