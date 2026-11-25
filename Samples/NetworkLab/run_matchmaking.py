#!/usr/bin/env python3
"""Use the session directory to discover and join the existing native network lab."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def run(binary, directory_script):
    """Publish only after server readiness; always close the advertisement."""
    with tempfile.TemporaryDirectory() as tmp:
        db = str(Path(tmp) / 'sessions.db')
        def command(*args):
            result = subprocess.run([sys.executable, str(directory_script), db, *args], capture_output=True, text=True, check=True, timeout=10)
            return json.loads(result.stdout)
        with subprocess.Popen([str(binary.resolve(strict=True)), 'server'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) as server:
            session = None
            try:
                ready = server.stdout.readline().split()
                if len(ready) != 2 or ready[0] != 'READY':
                    raise RuntimeError('Server did not announce port')
                session = command('create', 'arena', ready[1], '--capacity', '1')
                listing = command('list')
                if len(listing) != 1 or listing[0]['id'] != session['id']:
                    raise RuntimeError('Session discovery failed')
                member = command('join', listing[0]['id'], 'client')
                client = subprocess.run([str(binary.resolve()), 'client', str(member['port'])], capture_output=True, text=True, check=True, timeout=10)
                stdout, stderr = server.communicate(timeout=10)
                if server.returncode or stdout.strip() != 'SERVER 10 10 0' or client.stdout.strip() != 'CLIENT 10 10 0':
                    raise RuntimeError(f'Native exchange failed: {stdout} {stderr} {client.stdout}')
                if not command('leave', member['token']):
                    raise RuntimeError('Membership release failed')
                print('Discovered session, joined native server, converged on tick 10, released seat')
            finally:
                if server.poll() is None:
                    server.kill(); server.wait()
                if session is not None:
                    if not command('close', session['id'], session['owner']) or command('list'):
                        raise RuntimeError('Session cleanup failed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('directory_script', type=Path)
    args = parser.parse_args()
    run(args.binary, args.directory_script)
