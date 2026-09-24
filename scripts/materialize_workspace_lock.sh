#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_ROOT="${KAIRO_WORKSPACE_ROOT:-$(cd "${ENGINE_ROOT}/.." && pwd)}"
LOCK_FILE="${ENGINE_ROOT}/workspace.lock"

if [[ ! -f "${LOCK_FILE}" ]]; then
    echo "ERROR: workspace lock is missing: ${LOCK_FILE}" >&2
    exit 2
fi

if (( $# > 0 )); then
    repos=("$@")
else
    repos=()
    while read -r repo sha extra; do
        [[ -z "${repo}" || "${repo}" == #* ]] && continue
        if [[ -n "${extra:-}" || ! "${sha:-}" =~ ^[0-9a-f]{40}$ ]]; then
            echo "ERROR: malformed workspace.lock row for ${repo}" >&2
            exit 2
        fi
        repos+=("${repo}")
    done < "${LOCK_FILE}"
fi

locked_sha() {
    local repo="$1"
    awk -v wanted="${repo}" '
        $1 == wanted {
            if (found++) exit 3
            print $2
        }
        END {
            if (!found) exit 2
        }
    ' "${LOCK_FILE}"
}

for repo in "${repos[@]}"; do
    sha="$(locked_sha "${repo}")" || {
        echo "ERROR: ${repo} is missing or duplicated in workspace.lock." >&2
        exit 2
    }
    if [[ ! "${sha}" =~ ^[0-9a-f]{40}$ ]]; then
        echo "ERROR: invalid locked SHA for ${repo}: ${sha}" >&2
        exit 2
    fi

    target="${WORKSPACE_ROOT}/${repo}"
    if [[ -e "${target}" && ! -e "${target}/.git" ]]; then
        echo "ERROR: ${target} exists but is not a Git checkout." >&2
        exit 2
    fi

    if [[ ! -e "${target}/.git" ]]; then
        echo "clone    ${repo}"
        git clone --filter=blob:none --no-checkout             "https://github.com/swayam8624/${repo}.git" "${target}"
    else
        status_output="$(git -C "${target}" status --porcelain --untracked-files=all)"
        meaningful_status="$(printf '%s\n' "${status_output}" |
            awk 'NF && !($1 == "??" && ($2 == ".DS_Store" || $2 ~ /\/.DS_Store$/))')"
        if [[ -n "${meaningful_status}" ]]; then
            echo "ERROR: refusing to materialize locked ${repo} over local changes." >&2
            printf '%s\n' "${meaningful_status}" >&2
            exit 2
        fi
    fi

    # Fetch branch refs first. If the exact commit is historical or otherwise
    # not already present, ask the remote for that reachable commit explicitly.
    git -C "${target}" fetch --prune --no-tags origin >/dev/null
    if ! git -C "${target}" cat-file -e "${sha}^{commit}" 2>/dev/null; then
        git -C "${target}" fetch --no-tags origin "${sha}" >/dev/null
    fi
    if ! git -C "${target}" cat-file -e "${sha}^{commit}" 2>/dev/null; then
        echo "ERROR: locked commit ${sha} for ${repo} is unavailable from origin." >&2
        exit 2
    fi

    git -C "${target}" checkout --detach --force "${sha}" >/dev/null
    actual="$(git -C "${target}" rev-parse HEAD)"
    if [[ "${actual}" != "${sha}" ]]; then
        echo "ERROR: ${repo} materialized at ${actual}, expected ${sha}." >&2
        exit 2
    fi
    printf '%-24s %s\n' "${repo}" "${sha}"
done

echo
echo "Locked workspace materialization: PASS"
