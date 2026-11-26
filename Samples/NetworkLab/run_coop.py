#!/usr/bin/env python3
"""Bounded three-process co-op test with actual TCP peers and failure injection."""
import argparse
from pathlib import Path
import queue
import socket
import struct
import subprocess
import threading


def ready(process):
    # readline itself has no deadline; a daemon reader permits a bounded wait.
    lines = queue.Queue(maxsize=1)
    threading.Thread(target=lambda: lines.put(process.stdout.readline()), daemon=True).start()
    line = lines.get(timeout=8).split()
    if len(line) != 2 or line[0] != 'READY':
        raise RuntimeError(f'Invalid ready line: {line}')
    return int(line[1])


def stop(process):
    if process.poll() is None:
        process.kill()
    process.communicate(timeout=5)


def run(binary):
    executable = str(binary.resolve(strict=True))
    # Observe every snapshot on both clients, not just the final position.
    processes = []
    def spawn(*args):
        p = subprocess.Popen([executable, *map(str,args)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        processes.append(p)
        return p
    try:
        server = spawn('server');port = ready(server)
        clients = [spawn('client',port),spawn('client',port)]
        expected = [f'TURN {t} {1+t} 1 {1+t} 5 {int(t==6)}' for t in range(1,7)]
        slots = set()
        for client in clients:
            out, err = client.communicate(timeout=12)
            lines = out.splitlines()
            if client.returncode or not lines or lines[1:] != expected or lines[0] not in ('SLOT 0','SLOT 1'):
                raise RuntimeError(f'Client mismatch: {out} {err}')
            slots.add(lines[0])
        out,err=server.communicate(timeout=12)
        if server.returncode or out.strip()!='SERVER 6 7 1 7 5 1' or len(slots)!=2:
            raise RuntimeError(f'Server mismatch: {out} {err} {slots}')
        # Send a valid first-player input followed by a bad second-player
        # tick, or disconnect. Neither peer may receive a committed snapshot.
        for failure in ('future','disconnect','diagonal','bad-header'):
            server=spawn('server');port=ready(server)
            with socket.create_connection(('127.0.0.1',port),timeout=5) as a, socket.create_connection(('127.0.0.1',port),timeout=5) as b:
                for peer in (a,b):
                    data=b''
                    while len(data)<60:
                        part=peer.recv(60-len(data))
                        if not part: raise RuntimeError('Truncated handshake')
                        data+=part
                    if data[:4]!=b'KNET': raise RuntimeError('Invalid handshake')
                frame=lambda t,x,y:struct.pack('!4sBBBBIii',b'KNET',1,1,0,0,t,x,y)
                a.sendall(frame(1,1,0))
                if failure=='disconnect': b.shutdown(socket.SHUT_RDWR)
                elif failure=='future': b.sendall(frame(2,1,0))
                elif failure=='diagonal': b.sendall(frame(1,1,1))
                else: b.sendall(b'BAD!'+frame(1,0,0)[4:])
                out,err=server.communicate(timeout=8)
                if server.returncode==0 or out.strip() or not err.strip():
                    raise RuntimeError(f'Invalid turn did not abort: {failure} {out} {err}')
                try: tail=a.recv(20)
                except ConnectionResetError: tail=b''
                if tail: raise RuntimeError(f'Partial turn published: {failure}')
        print('Three native processes agreed on all six turns; four invalid/disconnect cases aborted without partial publication')
    finally:
        for process in processes: stop(process)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary',type=Path);run(parser.parse_args().binary)
