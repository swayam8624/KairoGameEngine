#!/usr/bin/env bash
# Start the product entry point, not the native editor with an arbitrary scene.
# No implicit builds, dependency installs, project mutation or shell evaluation.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON="${KAIRO_PYTHON:-python3}"
if ! "$PYTHON" -c 'import tkinter' >/dev/null 2>&1; then
    printf '%s\n' 'Kairo Hub requires a Python installation with Tk support.' >&2
    exit 1
fi
exec "$PYTHON" "$ROOT/scripts/kairo_hub.py" "$@"
