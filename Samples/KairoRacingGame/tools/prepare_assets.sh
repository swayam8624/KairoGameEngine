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
    test -f "${UPSTREAM}/public/models/${asset}-draco.glb"
done
test -x "${BLENDER}"

mkdir -p "${OUT}"
for asset in track chassis wheel; do
    echo "[KAIRO Racing] decoding exact upstream runtime asset ${asset}-draco.glb"
    "${BLENDER}" --background --factory-startup         --python "${SCRIPT_DIR}/export_glb.py" --         "${UPSTREAM}/public/models/${asset}-draco.glb"         "${OUT}/${asset}.glb"
done

echo
echo "[KAIRO Racing] runtime assets ready:"
ls -lh "${OUT}"/*.glb

echo
echo "[KAIRO Racing] validating imported runtime-space bounds"
python3 - "${OUT}" <<'PY'
import sys
from pathlib import Path

root = Path(sys.argv[1])

def read_bounds(name: str):
    path = root / f"{name}.bounds.txt"
    rows = {}
    for line in path.read_text().splitlines():
        key, *values = line.split()
        rows[key] = values
    minimum = tuple(float(v) for v in rows["min"])
    maximum = tuple(float(v) for v in rows["max"])
    return minimum, maximum, int(rows["mesh_count"][0])

track_min, track_max, track_meshes = read_bounds("track")
chassis_min, chassis_max, chassis_meshes = read_bounds("chassis")
wheel_min, wheel_max, wheel_meshes = read_bounds("wheel")

start_x, start_z = -110.0, 220.0
if not (track_min[0] <= start_x <= track_max[0] and
        track_min[2] <= start_z <= track_max[2]):
    raise SystemExit(
        "KAIRO Racing asset-space mismatch: upstream car start "
        f"({start_x}, {start_z}) lies outside track X/Z bounds "
        f"x=[{track_min[0]}, {track_max[0]}], "
        f"z=[{track_min[2]}, {track_max[2]}]"
    )

def extent(minimum, maximum):
    return tuple(b - a for a, b in zip(minimum, maximum))

chassis_extent = extent(chassis_min, chassis_max)
wheel_extent = extent(wheel_min, wheel_max)
if max(chassis_extent) < 1.0 or max(chassis_extent) > 20.0:
    raise SystemExit(f"Unexpected chassis scale after conversion: {chassis_extent}")
if max(wheel_extent) < 0.1 or max(wheel_extent) > 5.0:
    raise SystemExit(f"Unexpected wheel scale after conversion: {wheel_extent}")
if track_meshes < 5:
    raise SystemExit(f"Track import unexpectedly contains only {track_meshes} mesh objects")

print(
    "KAIRO Racing asset contract PASS\n"
    f"  track meshes:   {track_meshes}\n"
    f"  track bounds:   min={track_min} max={track_max}\n"
    f"  chassis meshes: {chassis_meshes} extent={chassis_extent}\n"
    f"  wheel meshes:   {wheel_meshes} extent={wheel_extent}"
)
PY
