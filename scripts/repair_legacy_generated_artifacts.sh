#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_ROOT="${KAIRO_WORKSPACE_ROOT:-$(cd "${ENGINE_ROOT}/.." && pwd)}"

restore_known_generated_file() {
    local repo="$1"
    local relative="$2"
    local path="${WORKSPACE_ROOT}/${repo}"
    local porcelain

    if [[ ! -e "${path}/.git" ]]; then
        echo "ERROR: missing repository checkout: ${path}" >&2
        exit 1
    fi

    porcelain="$(git -C "${path}" status --porcelain --untracked-files=all -- "${relative}")"
    if [[ -z "${porcelain}" ]]; then
        echo "OK        ${repo}/${relative}: clean"
        return
    fi

    # This helper intentionally repairs only repository-owned generated
    # presentation artifacts that older KAIRO tooling could overwrite.
    # It never touches source files or arbitrary untracked work.
    case "${repo}/${relative}" in
        KairoBlender/docs/images/blender-asset-result.png)
            echo "RESTORE   ${repo}/${relative}"
            git -C "${path}" restore --worktree -- "${relative}"
            ;;
        *)
            echo "ERROR: refusing to restore unregistered path: ${repo}/${relative}" >&2
            exit 2
            ;;
    esac
}

restore_known_generated_file KairoBlender docs/images/blender-asset-result.png

echo
echo "Known legacy generated artifacts repaired."
echo "No source files were modified."
