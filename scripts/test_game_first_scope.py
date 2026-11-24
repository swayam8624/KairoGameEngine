#!/usr/bin/env python3
"""Host-neutral regression for game-first workspace and build selection."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"

def run(args, **kwargs):
    return subprocess.run(args, check=True, text=True, capture_output=True, **kwargs)

class GameFirstScope(unittest.TestCase):
    def profile(self, *args):
        result = run(["bash", "-c", 'source "$1"; shift; kairo_select_workspace_profile "$@" && printf "%s\\n" "${repos[@]}"',
                      "scope", str(SCRIPTS / "workspace_profiles.sh"), *args])
        return result.stdout.splitlines()

    def test_profiles_match_lock_and_required_targets(self):
        core = self.profile()
        full = self.profile("--portfolio")
        locked = [line.split()[0] for line in (ROOT / "workspace.lock").read_text().splitlines()
                  if line.strip() and not line.startswith("#")]
        self.assertEqual(len(core), 12)
        self.assertEqual(len(full), 26)
        self.assertEqual(len(set(full)), len(full))
        self.assertEqual(set(full), set(locked))
        cmake = (ROOT / "CMakeLists.txt").read_text()
        required = re.findall(r"^kairo_require_workspace_repo\(\w+ (Kairo\w+)\)", cmake, re.M)
        self.assertEqual(set(core), set(required) | {"KairoEditor"})

    def test_game_preset_explicitly_disables_experiments(self):
        data = json.loads((ROOT / "CMakePresets.json").read_text())
        presets = {p["name"]: p for p in data["configurePresets"]}
        game = presets["game-clang"]
        self.assertTrue(game["binaryDir"].endswith("/game-clang"))
        flags = game["cacheVariables"]
        for name in ("EDITOR", "PLAYER", "PHASE1_SAMPLE"):
            self.assertEqual(flags["KAIRO_GAME_ENGINE_BUILD_" + name], "ON")
        for name in ("RACING_SAMPLE", "RAYTRACER", "PHYSICS_SANDBOX", "AI_CLOUD", "COMPUTE_STACK", "MIGRATION", "BENCHMARKS"):
            self.assertEqual(flags["KAIRO_GAME_ENGINE_BUILD_" + name], "OFF")
        for key in ("buildPresets", "testPresets"):
            self.assertIn("game-clang", [p["name"] for p in data[key]])

    def test_optional_tools_do_not_leak_into_runtime(self):
        root = (ROOT / "CMakeLists.txt").read_text()
        self.assertNotRegex(root, r"target_link_libraries\(KairoGameEngine\s+INTERFACE\s+KairoBridge")
        self.assertRegex(root, r"if\(KAIRO_GAME_ENGINE_BUILD_BENCHMARKS\)\s+add_executable\(KairoWaveCScaleBenchmark")
        player = (ROOT / "Runtime/KairoPlayer/CMakeLists.txt").read_text()
        self.assertIn("if(TARGET Kairo::GameplayAI)", player)
        self.assertIn("Kairo::GameEngine KairoRealtimeRenderBridge ${_kairo_player_gameplay_ai}", player)

    def test_bootstrap_default_then_explicit_portfolio(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            bin_dir = root / "bin"
            bin_dir.mkdir()
            git = bin_dir / "git"
            git.write_text('#!/usr/bin/env bash\nset -eu\n[[ "$1" == clone ]]\nmkdir -p "$3/.git"\n')
            git.chmod(0o755)
            workspace = root / "workspace"
            workspace.mkdir()
            env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ["PATH"],
                       KAIRO_WORKSPACE_ROOT=str(workspace))
            script = str(SCRIPTS / "bootstrap_workspace.sh")
            run(["bash", script], env=env)
            self.assertEqual({p.name for p in workspace.iterdir()}, set(self.profile()))
            run(["bash", script, "--portfolio"], env=env)
            self.assertEqual({p.name for p in workspace.iterdir()}, set(self.profile("--portfolio")))

    def test_build_wrapper_selects_distinct_directories(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            scripts = root / "engine" / "scripts"
            scripts.mkdir(parents=True)
            shutil.copy(SCRIPTS / "build_and_test.sh", scripts)
            bin_dir = root / "bin"
            bin_dir.mkdir()
            log = root / "calls"
            for name in ("cmake", "ctest"):
                tool = bin_dir / name
                tool.write_text('#!/usr/bin/env bash\nprintf "%s %s\\n" "${0##*/}" "$*" >> "$KAIRO_TEST_LOG"\n')
                tool.chmod(0o755)
            env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ["PATH"],
                       KAIRO_TEST_LOG=str(log), KAIRO_WORKSPACE_ROOT=str(root))
            run(["bash", str(scripts / "build_and_test.sh")], env=env)
            self.assertIn("cmake --preset game-clang", log.read_text())
            self.assertIn("ctest --preset game-clang", log.read_text())
            run(["bash", str(scripts / "build_and_test.sh"), "--portfolio"], env=env)
            self.assertIn("cmake --preset dev-clang", log.read_text())
            self.assertTrue((scripts.parent / "build/game-clang/integration-build.log").is_file())
            self.assertTrue((scripts.parent / "build/dev-clang/integration-build.log").is_file())

if __name__ == "__main__":
    unittest.main()
