#!/usr/bin/env bash
set -Eeuo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="${ENGINE_ROOT}/Samples/KairoRacingGame/Project/Racing.kproject"
GAME="${ENGINE_ROOT}/build/dev-clang/Samples/KairoRacingGame/KairoRacingGame"
PLAYER="${ENGINE_ROOT}/build/dev-clang/Runtime/KairoPlayer/KairoPlayer"
EDITOR="${ENGINE_ROOT}/build/dev-clang/components/KairoEditor/KairoEditorApp"
LOG_DIR="${ENGINE_ROOT}/build/kairo-racing-local"
LOG="${LOG_DIR}/run.log"

mkdir -p "${LOG_DIR}"
: > "${LOG}"
exec > >(tee -a "${LOG}") 2>&1

trap 'status=$?; echo; echo "============================================================"; echo "KAIRO RACING FAILED"; echo "line: ${LINENO}"; echo "command: ${BASH_COMMAND}"; echo "status: ${status}"; echo "log: ${LOG}"; echo "============================================================"; exit ${status}' ERR

echo "============================================================"
echo "KAIRO RACING — NATIVE ENGINE RUN"
echo "============================================================"
echo "engine:  ${ENGINE_ROOT}"
echo "project: ${PROJECT}"
echo "log:     ${LOG}"
echo

cd "${ENGINE_ROOT}"

echo "[1/7] Verifying locked KAIRO workspace..."
bash scripts/verify_workspace_lock.sh

echo
echo "[2/7] Preparing upstream racing assets through Blender..."
bash Samples/KairoRacingGame/tools/prepare_assets.sh

echo
echo "[3/7] Configuring and compiling KairoRacingGame..."
cmake --preset dev-clang
cmake --build --preset dev-clang --target KairoRacingGame --parallel

test -x "${GAME}"
test -x "${PLAYER}"
test -x "${EDITOR}"

echo
echo "[4/7] Validating the external-game project through KairoPlayer..."
"${PLAYER}" "${PROJECT}" --validate

echo
echo "[5/7] Running native Metal smoke + registered racing tests..."
"${GAME}" "${PROJECT}" --renderer metal --smoke
ctest --test-dir build/dev-clang -R '^KairoRacing\.' --output-on-failure

echo
echo "[6/7] Packaging the native KAIRO racing game..."
"${GAME}" "${PROJECT}" --package Release --replace
find Samples/KairoRacingGame/Project/Build/Release -maxdepth 3 -type f -print 2>/dev/null || true

echo
echo "[7/7] Opening KAIRO Editor and launching the actual game..."
"${EDITOR}" --project "${PROJECT}" > "${LOG_DIR}/editor.log" 2>&1 &
EDITOR_PID=$!
echo "KAIRO Editor PID: ${EDITOR_PID}"
echo "Editor log: ${LOG_DIR}/editor.log"

sleep 2

echo
echo "============================================================"
echo "PLAY"
echo "W/S   throttle/reverse"
echo "A/D   steer"
echo "SPACE brake"
echo "R     reset"
echo "ESC   quit"
echo "============================================================"
echo
echo "The following process is KairoRacingGame using KAIRO Renderer/Physics/Input."
echo "Closing the game returns you to this terminal."
echo

exec "${GAME}" "${PROJECT}" --renderer metal
