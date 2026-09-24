#!/usr/bin/env bash
set -euo pipefail

ENGINE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE_ROOT="${KAIRO_WORKSPACE_ROOT:-$(cd "${ENGINE_ROOT}/.." && pwd)}"

repos=(
  KairoMath KairoGeometry KairoSpatial KairoPhysicsMath KairoPhysicsEngine
  KairoAssets KairoAI KairoECS KairoReflection KairoEngineCore KairoRenderer
  KairoEditor KairoRayTracer KairoGPU KairoSIMD KairoScheduler KairoONNX
  KairoTransformers KairoHub KairoMacPerception
  KairoPipelineCore KairoBlender KairoProductionTools KairoHoudini KairoMaya KairoNuke
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

# First pass: preflight the complete workspace before touching any remote refs.
# A dirty repo must fail immediately without leaving a confusing half-fetched
# transcript that looks like some repositories synchronized and others did not.
for repo in "${repos[@]}"; do
    path="${WORKSPACE_ROOT}/${repo}"
    if [[ ! -e "${path}/.git" ]]; then
        echo "ERROR: missing Git checkout: ${path}" >&2
        exit 1
    fi

    status_output="$(git -C "${path}" status --porcelain --untracked-files=all)"
    # Finder metadata is disposable host noise. Ignore only untracked
    # .DS_Store entries; tracked edits and every other untracked file remain
    # a hard stop so sync can never silently destroy real work.
    meaningful_status="$(printf '%s\n' "${status_output}" |
        awk 'NF && !($1 == "??" && ($2 == ".DS_Store" || $2 ~ /\/.DS_Store$/))')"

    if [[ -n "${meaningful_status}" ]]; then
        echo "ERROR: ${repo} has meaningful local changes; workspace sync is read-only until they are resolved." >&2
        printf '%s\n' "${meaningful_status}" >&2

        if [[ "${repo}" == "KairoBlender" ]] &&
           [[ "${meaningful_status}" == " M docs/images/blender-asset-result.png" ||
              "${meaningful_status}" == "M  docs/images/blender-asset-result.png" ]]; then
            echo >&2
            echo "This path is the reproducible Blender documentation render." >&2
            echo "If you did not intentionally edit it, restore only that generated artifact with:" >&2
            echo "  bash \"${ENGINE_ROOT}/scripts/repair_legacy_generated_artifacts.sh\"" >&2
            echo "The helper is allow-listed to that generated PNG only; it cannot discard source edits." >&2
            echo "The portfolio-scene generator has been fixed so normal runs no longer rewrite it." >&2
        fi
        exit 1
    fi
done

echo "Workspace cleanliness preflight: PASS"

# Second pass: fetch every remote only after the entire workspace is known clean.
for repo in "${repos[@]}"; do
    path="${WORKSPACE_ROOT}/${repo}"
    printf '%-24s ' "${repo}"
    git -C "${path}" fetch --prune origin >/dev/null
    echo "fetched"
done

# Third pass: repair clean detached checkouts and verify tracking branches.
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

# Fourth pass: fast-forward every repository.  --ff-only protects local history.
for repo in "${repos[@]}"; do
    path="${WORKSPACE_ROOT}/${repo}"
    printf '%-24s ' "${repo}"
    git -C "${path}" pull --ff-only
done

echo
echo "All sibling repositories are on tracking branches and fast-forward synchronized."
