# Game-first integration ledger

Updated 2026-11-24. This is a review/apply queue, not an accepted build stamp.
All listed PRs were opened as drafts. No main/master branch was modified by this cleanup campaign. Do not reset local dirty work.

## Apply order

| Order | Repository | PR | Branch | Review head | Purpose |
|---|---|---|---|---|---|
| 1 | KairoMath | [#4](https://github.com/swayam8624/KairoMath/pull/4) | fix/release-transform-warning | 6dfc0f34a123d191aae5e5110bc686c8ae9d1986 | Release/NDEBUG warning blocking Ubuntu build |
| 2 | KairoAI | [#6](https://github.com/swayam8624/KairoAI/pull/6) | refactor/gameplay-runtime-split | 35d4d4583986d860e54c10c6eff3f56bee12a471 | Gameplay-only target, assistant optional; CI matcher repair |
| 3 | KairoEditor | [#21](https://github.com/swayam8624/KairoEditor/pull/21) | refactor/game-first-scene | 1a0b44c55603040a00a48866b95c983a6befe2f7 | Structural editing component preservation |
| 4 | KairoGameEngine | [#42](https://github.com/swayam8624/KairoGameEngine/pull/42) | refactor/game-first | See PR current head (this ledger is part of it) | Core workspace and strict runtime profile |
| 5 | KairoGameEngine | [#43](https://github.com/swayam8624/KairoGameEngine/pull/43) | refactor/runtime-resource-journal | 1733cd9921c2c1345b69bc1323499d76055069b6 | Linear resource journal and startup snapshot reuse |
| 6 | KairoGameEngine | [#44](https://github.com/swayam8624/KairoGameEngine/pull/44) | fix/compute-integration-variant-header | 397c61526e996628b969831db4d9fe3ea677e87d | Optional compute test missing direct standard-library headers |
| 7 | KairoGameEngine | [#45](https://github.com/swayam8624/KairoGameEngine/pull/45) | refactor/frame-scene-extraction | 63a3d376b3a3e9a0d43835583c49a69041d975d8 | Frame-local hierarchy memoization and shared entity enumeration |

Math, AI and Editor are sibling dependencies; review and test their branches before the strict engine integration. Engine #43, #44 and #45 are independent branches from main; none requires a stacked PR base. Refresh/rebase against the updated main as appropriate. Do not merge just because an older run passed.

## Local Codex handoff

The owner reported that local CI patches and source fixes are saved separately, not pushed. Review those patches alongside this queue; if a Math or compute include fix already exists, apply the equivalent change once. Integrate the CI changes without overwriting the new narrow workflows or the game-first default. No access to the local Mac handoff contents is assumed here.

## Evidence observed

- GameEngine #42 prior head d4d4690fdc0dbb8670f3fe989cd1811553b3ea70: game-first scope and migration tests succeeded; macOS Clang, Windows Runtime MSVC and Ubuntu ASan/UBSan jobs succeeded; Ubuntu Clang failed on Math assert-only eps; integrated compute and Wave E failed at std::get of variant; Wave C and racing compile succeeded. Racing macOS integration was still running at observation. These are historical results, not validation of the final combination.
- AI #6 prior head 99c1d65db3ac09cf1a3dcbe055df14cbbf3f75b7: standard Build and Test and assistant ON matrix passed. OFF matrix built/tested but failed the target-name check; narrowed matcher to exact Ninja target names instead of checkout paths. Rerun required.
- Editor #21 head 1a0b44c55603040a00a48866b95c983a6befe2f7: observed editor workflow cancelled. No complete native regression result.
- Engine #43: local independent Clang C++23 ASan/UBSan harness exercised the exact modified header (verified git blob f95dcdbc65245ce767872974feb2d12dc6202bb6): 8,191 exhaustive failure masks for journal lengths 0–12, 65,536 handles with 17 capacity growths, alternating failure/retry and bind/unbind failure cases. Checked-in CTest regression and native integration still need execution.
- Math #4 and Engine #44: source repairs for observed CI errors, native reruns pending.
- Engine #43: standalone Runtime resource journal Actions run 45429139703 completed successfully; full integration still pending at observation.
- Engine #45: independent Clang C++23 ASan/UBSan harness against verified exact header blob 92f889024b5a24c1618f5a099828084afa5e4709 passed 102,014 randomized reference comparisons and a 100,000-node reverse-ID chain; active state and world transforms evaluated at most once per node, new cache reflected disabled ancestor. Full module tests and native frame timing pending. O(n) transient cache storage and binary-search lookup are tradeoffs; profile both shallow/sparse and deep scenes. Standalone Editor's pinned bridge revision is unchanged until integration is accepted.

## Final integration gate

1. Preserve the local working tree and inspect all six diffs.
2. Apply approved sibling revisions and CI infrastructure; run standalone checks.
3. Refresh engine workspace lock using the actual approved sibling commit SHAs, not branch names or guessed revisions. Preserve historical evidence; do not mark an untested lock accepted.
4. Build game-clang and strict game-player-clang, run CTest with explicit native Metal smoke on Mac and cross-platform CI. Exercise editor undo/duplicate/prefab and the runtime resource journal.
5. Separately run legacy optional compute and portfolio workflows if retaining those components.
6. Record exact revisions, host/compiler, failures and skips. Only then mark the combined game-first build accepted.

Future passes should append entries here; keep the merge queue and exact-head evidence distinct from the roadmap.
