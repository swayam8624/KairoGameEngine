# Independent M50 portable acceptance

From the integration repository root on Linux:

    python3 scripts/run_m50_portable.py --compiler g++ --report m50-gcc.json
    python3 scripts/run_m50_portable.py --compiler clang++ --report m50-clang.json

The gate uses C++23, optimization, and warnings as errors. It builds 16
portable regression executables, a genuine loopback network pair, a
matchmaking connection, good/bad native plugin libraries and their loader
test, three playable terminal demos and both Python test suites. Each
compiler's report includes actual commands, output, exit codes, SHA256
and Git blob fingerprints. Builds and saves are isolated in a temporary
directory, and a missing source or failed test fails the gate.

The separate GitHub workflow runs this without the 26 sibling repositories.
It is an additional acceptance tier, not a replacement for workspace CMake,
native GPU, Editor, physics, packaging, mobile or web platform tests.
Linux only: other operating systems require separate build verification.
