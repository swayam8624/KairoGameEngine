#!/usr/bin/env python3
"""Launch two distinct native network processes and compare their final state."""
import argparse
from pathlib import Path
import subprocess


def run(binary: Path) -> None:
    """The server chooses its own port; no sleeps or fixed port assumptions."""
    executable = str(binary.resolve(strict=True))
    with subprocess.Popen([executable, "server"], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True) as server:
        try:
            # Server emits READY immediately after bind/listen, then accept
            # itself has a bounded wait. Its port is a loopback-only endpoint.
            ready = server.stdout.readline().split()
            if len(ready) != 2 or ready[0] != "READY":
                raise RuntimeError("Server failed to publish its port")
            client = subprocess.run([executable, "client", ready[1]],
                                    capture_output=True, text=True, timeout=10, check=True)
            stdout, stderr = server.communicate(timeout=10)
            if server.returncode or stdout.strip() != "SERVER 10 10 0" or client.stdout.strip() != "CLIENT 10 10 0":
                raise RuntimeError(f"Network pair failed: {stdout} {stderr} {client.stdout} {client.stderr}")
            print("Two native loopback processes agreed on tick 10, position (10,0)")
        finally:
            if server.poll() is None:
                server.kill()
                server.wait()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    run(parser.parse_args().binary)
