"""Build a real WASM site and require the real Chromium canvas/game smoke."""
import functools
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import threading
import unittest
from build_web_platformer import build


class WebTests(unittest.TestCase):
    def test_browser(self):
        browser = shutil.which('chromium') or shutil.which('chromium-browser') or shutil.which('google-chrome')
        self.assertIsNotNone(browser, 'Chromium is required for this gate')
        with tempfile.TemporaryDirectory(prefix='kairo-web-') as tmp:
            root = Path(tmp)
            site = build(root/'site')
            self.assertEqual((site/'platformer.wasm').read_bytes()[:4], b'\0asm')
            with self.assertRaises(ValueError): build(site)
            handler = functools.partial(SimpleHTTPRequestHandler, directory=str(site))
            with ThreadingHTTPServer(('127.0.0.1',0), handler) as server:
                thread = threading.Thread(target=server.serve_forever, daemon=True)
                thread.start()
                try:
                    args = [browser, '--headless', '--disable-gpu', '--disable-dev-shm-usage', '--no-first-run',
                            '--user-data-dir='+str(root/'profile'), '--virtual-time-budget=5000', '--dump-dom']
                    # The isolated CI/root container cannot use Chromium's normal
                    # sandbox. Only our generated local page is opened here.
                    if os.geteuid()==0: args.append('--no-sandbox')
                    args.append(f'http://127.0.0.1:{server.server_port}/?selftest=1')
                    result = subprocess.run(args, capture_output=True, text=True, timeout=30, check=True)
                    self.assertIn('data-smoke="passed"', result.stdout, result.stdout+result.stderr)
                    self.assertIn('Level complete!', result.stdout)
                finally:
                    server.shutdown(); thread.join(timeout=5)


if __name__ == '__main__':
    unittest.main()
