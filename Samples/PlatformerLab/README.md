# Platformer lab

Compile at the root:

    c++ -std=c++23 -Wall -Wextra -Werror -IRuntime/KairoPlayer Samples/PlatformerLab/PlatformerDemo.cpp -o /tmp/kairo-platformer
    /tmp/kairo-platformer

Controls are printed on startup. Move right, jump the hazard, collect both coins, and reach the exit. For a deterministic winning route use r 31, j 17, r 80 on separate lines. Death resets the level's coins. A solid border bounds the integer tile world. This is a playable terminal sample and reusable CPU kernel; native visual adapter and external level editor remain open. It is not using PhysicsEngine rigid bodies: the deliberately narrow grid collision model is standalone.
