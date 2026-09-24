#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_ROOT="${KAIRO_WORKSPACE_ROOT:-$(cd "${ENGINE_ROOT}/.." && pwd)}"
EVIDENCE_DIR="${ENGINE_ROOT}/build/wave-e-compute"
mkdir -p "${EVIDENCE_DIR}"

resolve_clang() {
    if [[ -n "${KAIRO_CXX_COMPILER:-}" && -x "${KAIRO_CXX_COMPILER}" ]]; then
        printf '%s\n' "${KAIRO_CXX_COMPILER}"
        return
    fi
    if command -v brew >/dev/null 2>&1; then
        local prefix
        prefix="$(brew --prefix llvm 2>/dev/null || true)"
        if [[ -n "${prefix}" && -x "${prefix}/bin/clang++" ]]; then
            printf '%s\n' "${prefix}/bin/clang++"
            return
        fi
    fi
    command -v clang++ || true
}

run() {
    echo
    echo "==> $*"
    "$@"
}

CXX_COMPILER="$(resolve_clang)"
if [[ -z "${CXX_COMPILER}" ]]; then
    echo "ERROR: clang++ is required for the compute campaign." >&2
    exit 4
fi

run cmake --preset compute-stack-clang
run cmake --build --preset compute-stack-clang --target KairoComputeStackIntegration --parallel
run "${ENGINE_ROOT}/build/compute-stack-clang/KairoComputeStackIntegration"     "${EVIDENCE_DIR}/integration.json"

standalone() {
    local repo="$1"
    local target="$2"
    local build="${WORKSPACE_ROOT}/${repo}/build/wave-e"
    run cmake -S "${WORKSPACE_ROOT}/${repo}" -B "${build}" -G Ninja         -DCMAKE_BUILD_TYPE=Release         -DCMAKE_CXX_COMPILER="${CXX_COMPILER}"
    run cmake --build "${build}" --target "${target}" --parallel
}

standalone KairoSIMD KairoSIMDBenchmark
run "${WORKSPACE_ROOT}/KairoSIMD/build/wave-e/KairoSIMDBenchmark"     "${EVIDENCE_DIR}/simd.json"

standalone KairoScheduler KairoSchedulerBenchmark
"${WORKSPACE_ROOT}/KairoScheduler/build/wave-e/KairoSchedulerBenchmark"     > "${EVIDENCE_DIR}/scheduler.json"

standalone KairoGPU KairoGPUBenchmark
"${WORKSPACE_ROOT}/KairoGPU/build/wave-e/KairoGPUBenchmark"     > "${EVIDENCE_DIR}/gpu.json"

standalone KairoONNX KairoONNXSmoke
run "${WORKSPACE_ROOT}/KairoONNX/build/wave-e/KairoONNXSmoke"
printf '%s\n' '{"schema":"kairo.onnx.smoke.v1","passed":true}'     > "${EVIDENCE_DIR}/onnx.json"

standalone KairoTransformers KairoTransformerBenchmark
run "${WORKSPACE_ROOT}/KairoTransformers/build/wave-e/KairoTransformerBenchmark"     "${EVIDENCE_DIR}/transformer.json"

python3 - "${EVIDENCE_DIR}" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
names = ["integration", "simd", "scheduler", "gpu", "onnx", "transformer"]
records = {}
for name in names:
    path = root / f"{name}.json"
    if not path.is_file():
        raise SystemExit(f"missing compute evidence: {path}")
    records[name] = json.loads(path.read_text())

if records["integration"].get("schema") != "kairo.compute.integration.v1":
    raise SystemExit("invalid integrated compute evidence schema")
if not records["integration"].get("correct"):
    raise SystemExit("integrated compute workload reported incorrect output")
if records["simd"].get("maximum_absolute_error") != 0:
    raise SystemExit("SIMD benchmark reported numerical error")
if records["gpu"].get("backend") != "unavailable" and not records["gpu"].get("correct"):
    raise SystemExit("GPU benchmark reported incorrect output")
if not records["onnx"].get("passed"):
    raise SystemExit("ONNX smoke did not pass")
if records["transformer"].get("cached_full_max_abs_error", 1.0) > 1e-5:
    raise SystemExit("transformer cached/full equivalence exceeded tolerance")

summary = {
    "schema": "kairo.compute.campaign.v1",
    "components": names,
    "integration_correct": True,
    "simd_backend": records["simd"].get("backend"),
    "gpu_backend": records["gpu"].get("backend"),
    "scheduler_workers": records["scheduler"].get("workers"),
    "transformer_tokens_per_second": records["transformer"].get("tokens_per_second"),
}
(root / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
print(json.dumps(summary, indent=2))
PY

echo
echo "KAIRO Wave E compute campaign: PASS"
echo "Evidence: ${EVIDENCE_DIR}/summary.json"
