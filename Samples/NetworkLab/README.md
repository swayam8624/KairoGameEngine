# Local authoritative networking lab

Build and run from the integration root (Linux/macOS):

    c++ -std=c++23 -Wall -Wextra -Werror Samples/NetworkLab/NetworkLab.cpp -o /tmp/kairo-net
    python3 Samples/NetworkLab/run_pair.py /tmp/kairo-net

Windows uses Winsock and needs ws2_32 (CMake target links it). The Linux
GCC/Clang evidence does not verify Windows or macOS. The server chooses an
ephemeral IPv4 loopback port and accepts one client, exchanges ten frames,
and exits. The client sends unit-axis input, not positions. Server validates
sequence numbers, applies movement, and sends authoritative snapshots.

Read RuntimeNetworkProtocol.hpp before RuntimeLoopbackTcp.hpp, then the
protocol tests and this sample. The versioned 20-byte big-endian frame is
independent of sockets. TCP partial reads/writes are handled explicitly.
This is a narrow local one-client prototype, not a distributed game,
authentication, encryption, NAT traversal or loss/reconciliation solution.


The client also predicts its movement locally and reconciles the returned server snapshot. The separate RuntimeNetworkPredictionTests.cpp tests delayed, dropped and reordered snapshots, corrections and bounded journal backpressure; this does not claim UDP reliability.
