# Kairo Studio — release candidate guide

This is a developer-host desktop candidate, not a signed consumer installer. It contains the project Hub, native Editor and Player, project authoring tools, and eight editable project starters including the complete Signal Vault game. The qualified build artifacts are attached to GitHub Actions runs; do not mix files from different commits or platforms.

## Obtain and verify the candidate

Use the KairoStudio-linux, KairoStudio-macos or KairoStudio-windows-msvc artifact from a successful native qualification run. The downloaded folder must contain KairoStudio.zip, receipt.json, native-imports.json and candidate-verification.json. Keep the files together. From the corresponding trusted source checkout, with Python 3.11 or later:

```sh
python scripts/verify_studio_candidate.py PATH_TO_ARTIFACT --target macos-arm64 --commit EXACT_COMMIT_SHA
```

Use linux-x86_64 or windows-x86_64 instead of macos-arm64 on those hosts. Obtain the exact tested commit from the workflow run, not a mutable branch name. The verifier checks recorded qualification, hashes and every indexed archive file; it cannot establish authenticity without a trusted source for the receipt.

## Launch Studio

Extract KairoStudio.zip into a new directory. Install a matching Python and Tk pair plus native graphics/runtime prerequisites. On Apple Silicon, a Homebrew Python 3.13 and python-tk@3.13 pair is supported by the CI setup; use KAIRO_PYTHON to specify its full interpreter path. The local GLFW and Vulkan loader are still required. Linux builds are qualified against their recorded distribution and libraries, not all Linux releases. Windows builds require compatible native libraries, system runtimes and GPU drivers.

Run launch-studio.sh on Linux/macOS or launch-studio.cmd on Windows. Keep projects outside the installed Studio. Choose a starter in New project, name it and choose a directory, then open the Editor. Use Project tools in the Hub for validation, native smoke, play, gameplay/menu editing and release packaging.

## Create and distribute a game privately

Start with Signal Vault for a complete editable 2D campaign or one of the simpler starters. Use the native project tools to validate after edits. Package Release from the Workbench. Confirm replacement only when rebuilding an existing recognized package, and export each ZIP to a new path. Previous ZIPs and private checkpoint files are preserved.

On Windows and Apple Silicon macOS, use Game installer in the Hub. Select a trusted release ZIP, a new game directory, and an explicit local runtime source (flat directory, supported vcpkg root, or Homebrew prefix on macOS). Inspect runtime reports the candidate library names, architecture and observed hashes. Compare them with independently trusted hashes before entering the pins. Choose an unused external JSON launch-ticket path. Install does not automatically run the game. Use Check, Validate, Smoke or Play explicitly. Retain the original archive and trusted ticket outside the installed game. Static Check does not establish driver support or publisher identity.

Linux uses the normal exported game archive and native runtime prerequisites; the private Windows/macOS installer is not a Linux installer. Do not register the hosted Windows software Vulkan ICD on a user machine. That helper is CI-only.

## Release gate

The exact cumulative revision must pass the native Windows, Apple Silicon and Linux build/creator gates and artifact verification, with no outstanding relevant failure. Hosted software Vulkan rendering is not physical GPU qualification. These artifacts are unsigned, not notarized, and do not establish complete dependency closure, clean physical-host compatibility or third-party binary redistribution rights. Keep those distinctions in release notes. Do not publish the candidate as a generally supported consumer release until the appropriate additional requirements are satisfied.

For engineering evidence and known failures see [STUDIO_FINAL_QUALIFICATION.md](STUDIO_FINAL_QUALIFICATION.md).
