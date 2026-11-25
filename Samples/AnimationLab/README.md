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
