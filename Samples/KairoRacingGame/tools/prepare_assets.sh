#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SAMPLE_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
ENGINE_ROOT="$(cd "${SAMPLE_ROOT}/../.." && pwd)"
KAIRO_ROOT="$(cd "${ENGINE_ROOT}/.." && pwd)"
UPSTREAM="${KAIRO_RACING_UPSTREAM:-${KAIRO_ROOT}/ExternalGames/racing-game}"
BLENDER="${KAIRO_BLENDER:-/Applications/Blender.app/Contents/MacOS/Blender}"
OUT="${SAMPLE_ROOT}/Project/Content/Racing"

trap 'status=$?; echo; echo "[KAIRO Racing] FAILED at line ${LINENO}: ${BASH_COMMAND}" >&2; echo "[KAIRO Racing] exit status ${status}" >&2; exit ${status}' ERR

for asset in track chassis wheel; do
    test -f "${UPSTREAM}/assets/${asset}.blend"
done
test -x "${BLENDER}"

mkdir -p "${OUT}"
for asset in track chassis wheel; do
    echo "[KAIRO Racing] exporting ${asset}.blend"
    "${BLENDER}" --background --factory-startup         --python "${SCRIPT_DIR}/export_glb.py" --         "${UPSTREAM}/assets/${asset}.blend"         "${OUT}/${asset}.glb"
done

echo
echo "[KAIRO Racing] runtime assets ready:"
ls -lh "${OUT}"/*.glb
