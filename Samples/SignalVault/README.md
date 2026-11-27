# Signal Vault

A short three-stage, authored 2D adventure. Recover two signal cores per stage, reach the exit, and unlock the vault.

| Stage | Objective |
| --- | --- |
| Signal outpost | Find the two signals and navigate the wall passage. |
| Relay bridge | Collect two signals and jump over the hazard. |
| Signal vault | Find the narrow gateway, recover the last signals, and reach the vault. |

Open Project.kproject with KairoPlayer or the existing World2D authoring workflow. WASD or arrows move, Space jumps on the platform stage, P pauses, R restarts the stage, N starts a new game, Escape quits. Gamepad bindings are also authored. Score increases by 100 per signal and 50 per stage completion (750 on a clean run). Checkpoints persist at campaign boundaries; the score is cumulative across attempts within a stage, not a competitive high-score system. A hazard resets the current stage.

The project includes original indexed sprite frames and palette, authored responsive play/pause/completion UI, tone audio with positional emitters, fixed-tick particle effects, and shared VM-backed rule dispatch. The platform level layout is adapted from the existing World2D sample. No game-specific native gameplay source is required.

## Native acceptance

Run the CTest targets KairoPlayerSignalVaultTests, KairoPlayerSignalVaultValidate, KairoPlayerSignalVaultNativeSmoke (supported native graphics hosts), and KairoPlayerSignalVaultPackage (Python-enabled configurations). The replay exercises the real gameplay and VM core, not GUI automation; the separate native smoke checks the first stage and authored overlays. Package relocation checks source-independent content and replays all three stages. It does not qualify installer signing or execution on a machine without engine build dependencies.

Package with KairoPlayer Project.kproject --package Release. The generated bundle lives at Build/Release. Existing native platform launchers are produced by the engine packager.
