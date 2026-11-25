"""Verify real native/WASM state parity independently of browser permissions."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from build_web_platformer import build, ROOT


class WasmTests(unittest.TestCase):
    def test_native_parity(self):
        with tempfile.TemporaryDirectory(prefix='kairo-wasm-') as tmp:
            root = Path(tmp)
            site = build(root/'site')
            trace = root/'trace'
            subprocess.run(['c++', '-std=c++23', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-I', str(ROOT/'Runtime/KairoPlayer'), str(ROOT/'Samples/WebPlatformer/PlatformerNativeTrace.cpp'),
                            '-o', str(trace)], check=True, timeout=90)
            subprocess.run(['node', str(ROOT/'Samples/WebPlatformer/verify_wasm.mjs'), str(site/'platformer.wasm'), str(trace)], check=True, timeout=30)
            subprocess.run(['node', '--check', str(site/'game.js')], check=True, timeout=10)
            with self.assertRaises(ValueError): build(site)


if __name__ == '__main__':
    unittest.main()
