# Conservative water grid lab

From the repository root:

    c++ -std=c++23 -Wall -Wextra -Werror Samples/WaterLab/WaterLab.cpp -o /tmp/kairo-water
    /tmp/kairo-water

Read RuntimeWaterGrid.hpp and its regression first. The sample prints depth
in a closed basin at six snapshots. The raised ridge blocks water below its
crest; one gap connects both sides. The CPU model transports water volume
down free-surface gradients and limits total donor outflow. It is a
depth-only relaxation model, not a momentum-conserving shallow-water solver
or ocean renderer. A real-time mesh/material adapter is a remaining gate.
