"""Build a real WASM site and require the real Chromium canvas/game smoke."""
import functools
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import queue
import shutil
import subprocess
import tempfile
import threading
import time
import unittest
from build_web_platformer import build


def browser_smoke(browser, root, site):
    """Wait for the browser's terminal DOM state, not a virtual-time snapshot.

    Only the temporary test copy receives the observer. The actual game
    continues to run the original WASM, keyboard and canvas self-test.
    """
    index = site/'index.html'
    html = index.read_text(encoding='utf-8')
    if html.count('</body>') != 1:
        raise ValueError('Unexpected test page structure')
    observer = """<script>
const smokeObserver = setInterval(() => {
    const state = document.body.dataset.smoke;
    if (state !== 'passed' && state !== 'failed') return;
    clearInterval(smokeObserver);
    fetch('/__kairo_smoke_result', {method:'POST',
        headers:{'Content-Type':'application/json'},
        body:JSON.stringify({state, status:document.querySelector('#status').textContent})});
}, 50);
</script>"""
    index.write_text(html.replace('</body>', observer+'\n</body>'), encoding='utf-8')
    results = queue.Queue(maxsize=1)

    class Handler(SimpleHTTPRequestHandler):
        def do_POST(self):
            try:
                length = int(self.headers.get('Content-Length', '0'))
                if self.path != '/__kairo_smoke_result' or not 0 < length <= 4096:
                    self.send_error(400)
                    return
                result = json.loads(self.rfile.read(length))
                if (not isinstance(result, dict) or result.get('state') not in ('passed','failed')
                        or not isinstance(result.get('status'), str)):
                    self.send_error(400)
                    return
                results.put_nowait(result)
            except (ValueError, queue.Full):
                self.send_error(400)
                return
            self.send_response(204)
            self.end_headers()

    handler = functools.partial(Handler, directory=str(site))
    with ThreadingHTTPServer(('127.0.0.1',0), handler) as server, tempfile.TemporaryFile() as logs:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        process = None
        try:
            args = [browser, '--headless', '--disable-gpu', '--disable-dev-shm-usage', '--no-first-run',
                    '--user-data-dir='+str(root/'profile'), '--no-default-browser-check']
            if hasattr(os, 'geteuid') and os.geteuid()==0:
                args.append('--no-sandbox')
            args.append(f'http://127.0.0.1:{server.server_port}/?selftest=1')
            process = subprocess.Popen(args, stdout=logs, stderr=logs)
            deadline = time.monotonic()+45
            while time.monotonic()<deadline:
                try:
                    return results.get(timeout=.25)
                except queue.Empty:
                    if process.poll() is not None:
                        raise RuntimeError(f'Browser exited before reporting: {process.returncode}')
            raise TimeoutError('Browser did not report a terminal smoke state')
        except (RuntimeError, TimeoutError) as error:
            logs.seek(0, os.SEEK_END)
            logs.seek(max(0, logs.tell()-16000))
            raise AssertionError(f'{error}\n{logs.read().decode(errors="replace")}') from error
        finally:
            if process is not None:
                if process.poll() is None:
                    process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            server.shutdown()
            thread.join(timeout=5)


class WebTests(unittest.TestCase):
    def test_browser(self):
        selected = os.environ.get('KAIRO_BROWSER')
        browser = shutil.which(selected) if selected else (
            shutil.which('google-chrome') or shutil.which('google-chrome-stable') or
            shutil.which('chromium') or shutil.which('chromium-browser'))
        self.assertIsNotNone(browser, 'Chromium is required for this gate')
        with tempfile.TemporaryDirectory(prefix='kairo-web-') as tmp:
            root = Path(tmp)
            site = build(root/'site')
            self.assertEqual((site/'platformer.wasm').read_bytes()[:4], b'\0asm')
            with self.assertRaises(ValueError):
                build(site)
            result = browser_smoke(browser, root, site)
            self.assertEqual(result['state'], 'passed', result)
            self.assertEqual(result['status'], 'Level complete!', result)


if __name__ == '__main__':
    unittest.main()
