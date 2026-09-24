#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_ROOT="${KAIRO_WORKSPACE_ROOT:-$(cd "${ENGINE_ROOT}/.." && pwd)}"

repos=(
  KairoMath KairoGeometry KairoSpatial KairoPhysicsMath KairoPhysicsEngine
  KairoAssets KairoAI KairoECS KairoReflection KairoEngineCore KairoRenderer
  KairoEditor KairoRayTracer KairoGPU KairoSIMD KairoScheduler KairoONNX
  KairoTransformers KairoHub KairoMacPerception
)

default_branch_for_repo() {
    local path="$1"
    local remote_head

    # Prefer the local origin/HEAD symbolic ref after refreshing it.
    git -C "${path}" remote set-head origin -a >/dev/null 2>&1 || true
    remote_head="$(git -C "${path}" symbolic-ref --quiet --short refs/remotes/origin/HEAD 2>/dev/null || true)"
    if [[ -n "${remote_head}" ]]; then
        printf '%s\n' "${remote_head#origin/}"
        return 0
    fi

    # Fall back to the remote HEAD symref without assuming main vs master.
    remote_head="$(git -C "${path}" ls-remote --symref origin HEAD 2>/dev/null | awk '/^ref:/ {sub("refs/heads/","",$2); print $2; exit}')"
    if [[ -n "${remote_head}" ]]; then
        printf '%s\n' "${remote_head}"
        return 0
    fi

    return 1
}

# First pass: ensure every sibling is a clean Git checkout.  Fetching remote
# refs is safe and lets us resolve detached historical checkouts without losing
# local work.
for repo in "${repos[@]}"; do
    path="${WORKSPACE_ROOT}/${repo}"
    if [[ ! -e "${path}/.git" ]]; then
        echo "ERROR: missing Git checkout: ${path}" >&2
        exit 1
    fi
    if [[ -n "$(git -C "${path}" status --porcelain --untracked-files=all)" ]]; then
        echo "ERROR: ${repo} has local changes; commit/stash them before workspace sync." >&2
        git -C "${path}" status --short >&2
        exit 1
    fi

    printf '%-24s ' "${repo}"
    git -C "${path}" fetch --prune origin >/dev/null
    echo "fetched"
done

# Second pass: repair clean detached checkouts and verify tracking branches.
for repo in "${repos[@]}"; do
    path="${WORKSPACE_ROOT}/${repo}"
    branch="$(git -C "${path}" symbolic-ref --quiet --short HEAD || true)"

    if [[ -z "${branch}" ]]; then
        detached_head="$(git -C "${path}" rev-parse HEAD)"

        # Never abandon a detached commit that exists only locally.
        if ! git -C "${path}" branch -r --contains "${detached_head}" |
             sed 's/^[* ]*//' |
             grep -q '^origin/'; then
            echo "ERROR: ${repo} is detached at ${detached_head}, and that commit is not reachable from any origin branch." >&2
            echo "       Refusing to switch branches because that could orphan local work." >&2
            exit 1
        fi

        default_branch="$(default_branch_for_repo "${path}")" || {
            echo "ERROR: could not determine origin's default branch for ${repo}." >&2
            exit 1
        }

        echo "${repo}: detached at ${detached_head}; restoring tracking branch '${default_branch}'."
        if git -C "${path}" show-ref --verify --quiet "refs/heads/${default_branch}"; then
            git -C "${path}" switch "${default_branch}"
        else
            git -C "${path}" switch --track -c "${default_branch}" "origin/${default_branch}"
        fi
        branch="${default_branch}"
    fi

    if ! git -C "${path}" rev-parse --abbrev-ref --symbolic-full-name "@{u}" >/dev/null 2>&1; then
        if git -C "${path}" show-ref --verify --quiet "refs/remotes/origin/${branch}"; then
            git -C "${path}" branch --set-upstream-to="origin/${branch}" "${branch}" >/dev/null
        else
            echo "ERROR: ${repo} branch '${branch}' has no upstream and origin/${branch} does not exist." >&2
            exit 1
        fi
    fi
done

# Third pass: fast-forward every repository.  --ff-only protects local history.
for repo in "${repos[@]}"; do
    path="${WORKSPACE_ROOT}/${repo}"
    printf '%-24s ' "${repo}"
    git -C "${path}" pull --ff-only
done

echo
echo "All sibling repositories are on tracking branches and fast-forward synchronized."
