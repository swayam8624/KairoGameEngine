# Strategy lab

    c++ -std=c++23 -Wall -Wextra -Werror -IRuntime/KairoPlayer Samples/StrategyLab/StrategyDemo.cpp -o /tmp/kairo-strategy
    /tmp/kairo-strategy

Select a worker, issue move or gather orders, and advance ticks. Deposit six resources at the base to win. Example: move 2 1; select 1; gather 8 1; tick 100 (each on a separate line). The BFS chooses shortest four-neighbor routes around solid cells and other workers. Worker order resolves contested cells deterministically; temporarily blocked orders wait. A local terminal rules slice, not native RTS presentation, combat, production economy, or general multi-agent crowd avoidance.
