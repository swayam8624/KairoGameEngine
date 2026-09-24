#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EVIDENCE_DIR="${ENGINE_ROOT}/build/wave-c-scale"
mkdir -p "${EVIDENCE_DIR}"

run() {
    echo
    echo "==> $*"
    "$@"
}

run bash "${ENGINE_ROOT}/scripts/verify_workspace_lock.sh"
run cmake --preset dev-clang
run cmake --build --preset dev-clang --parallel

TEST_REGEX='SpatialTests|KairoPhysicsEngineTests|KairoAssetsTests|KairoRendererTests|KairoEngineCoreTests'
START_NS="$(python3 - <<'PY'
import time
print(time.time_ns())
PY
)"
run ctest --test-dir "${ENGINE_ROOT}/build/dev-clang"     -R "${TEST_REGEX}" --output-on-failure
run "${ENGINE_ROOT}/build/dev-clang/KairoWaveCScaleBenchmark" \
    "${EVIDENCE_DIR}/benchmark.json"
END_NS="$(python3 - <<'PY'
import time
print(time.time_ns())
PY
)"

ENGINE_SHA="$(git -C "${ENGINE_ROOT}" rev-parse HEAD)"
LOCK_SHA="$(shasum -a 256 "${ENGINE_ROOT}/workspace.lock" | awk '{print $1}')"
python3 - "${EVIDENCE_DIR}/summary.json" "${ENGINE_SHA}" "${LOCK_SHA}" "${START_NS}" "${END_NS}" <<'PY'
import json
import pathlib
import sys

path, engine_sha, lock_sha, start_ns, end_ns = sys.argv[1:]
elapsed_ns = int(end_ns) - int(start_ns)
record = {
    "schema": "kairo.wave-c.scale.v1",
    "engine_sha": engine_sha,
    "workspace_lock_sha256": lock_sha,
    "gates": [
        "SpatialTests",
        "KairoPhysicsEngineTests",
        "KairoAssetsTests",
        "KairoRendererTests",
        "KairoEngineCoreTests",
    ],
    "correctness": "PASS",
    "aggregate_test_wall_ns": elapsed_ns,
    "note": "Wall time is diagnostic evidence, not a cross-machine performance claim.",
}
pathlib.Path(path).write_text(json.dumps(record, indent=2) + "\n")
print(json.dumps(record, indent=2))
PY

echo
echo "KAIRO Wave C scale campaign: PASS"
echo "Evidence: ${EVIDENCE_DIR}/summary.json"
