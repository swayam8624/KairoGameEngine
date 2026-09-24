#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_ROOT="${KAIRO_WORKSPACE_ROOT:-$(cd "${ENGINE_ROOT}/.." && pwd)}"
LOCK_FILE="${ENGINE_ROOT}/workspace.lock"

mismatches=0
while read -r repo expected extra; do
    [[ -z "${repo}" || "${repo}" == #* ]] && continue
    if [[ -n "${extra:-}" ]]; then
        echo "ERROR: malformed workspace.lock row for ${repo}" >&2
        exit 2
    fi
    path="${WORKSPACE_ROOT}/${repo}"
    if [[ ! -e "${path}/.git" ]]; then
        echo "MISSING   ${repo} expected ${expected}"
        mismatches=$((mismatches + 1))
        continue
    fi
    actual="$(git -C "${path}" rev-parse HEAD)"
    if [[ "${actual}" == "${expected}" ]]; then
        echo "OK        ${repo} ${actual}"
    else
        echo "DIFF      ${repo}"
        echo "          expected ${expected}"
        echo "          actual   ${actual}"
        mismatches=$((mismatches + 1))
    fi
done < "${LOCK_FILE}"

if [[ ${mismatches} -ne 0 ]]; then
    echo
    echo "Workspace differs from the recorded integration snapshot in ${mismatches} repo(s)." >&2
    echo "That is valid during development; do not call it the locked integration state." >&2
    exit 1
fi

echo
echo "Workspace exactly matches workspace.lock."
