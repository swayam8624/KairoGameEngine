#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_ROOT="${KAIRO_WORKSPACE_ROOT:-$(cd "${ENGINE_ROOT}/.." && pwd)}"
LOCK_FILE="${ENGINE_ROOT}/workspace.lock"
requested=("$@")

should_check_repo() {
    local candidate="$1"
    if (( ${#requested[@]} == 0 )); then
        return 0
    fi
    local wanted
    for wanted in "${requested[@]}"; do
        if [[ "${candidate}" == "${wanted}" ]]; then
            return 0
        fi
    done
    return 1
}

mismatches=0
checked=0
while read -r repo expected extra; do
    [[ -z "${repo}" || "${repo}" == \#* ]] && continue
    should_check_repo "${repo}" || continue
    checked=$((checked + 1))
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
    status_output="$(git -C "${path}" status --porcelain --untracked-files=all)"
    meaningful_status="$(printf '%s\n' "${status_output}" |
        awk 'NF && !($1 == "??" && ($2 == ".DS_Store" || $2 ~ /\/.DS_Store$/))')"

    if [[ "${actual}" != "${expected}" ]]; then
        echo "DIFF      ${repo}"
        echo "          expected ${expected}"
        echo "          actual   ${actual}"
        mismatches=$((mismatches + 1))
        continue
    fi
    if [[ -n "${meaningful_status}" ]]; then
        echo "DIRTY     ${repo} ${actual}"
        printf '%s\n' "${meaningful_status}" | sed 's/^/          /'
        mismatches=$((mismatches + 1))
        continue
    fi
    echo "OK        ${repo} ${actual}"
done < "${LOCK_FILE}"

if (( ${#requested[@]} > 0 && checked != ${#requested[@]} )); then
    echo "ERROR: requested ${#requested[@]} lock entries but matched ${checked}; check repository names." >&2
    exit 2
fi

if [[ ${mismatches} -ne 0 ]]; then
    echo
    echo "Workspace differs from the recorded integration snapshot in ${mismatches} repo(s)." >&2
    echo "That is valid during development; do not call it the locked integration state." >&2
    exit 1
fi

echo
echo "Workspace exactly matches workspace.lock and all locked trees are clean."
