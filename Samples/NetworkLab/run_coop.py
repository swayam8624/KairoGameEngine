#!/usr/bin/env python3
"""Bounded three-process co-op test with actual TCP peers and failure injection."""
import argparse
import os
import secrets
from pathlib import Path
import queue
import socket
import struct
import subprocess
import threading
import tempfile
import sys

# Standalone invocation and the portable runner use the same directory implementation.
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
from kairo_matchmaking import Directory


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
    base_env = {k: v for k,v in os.environ.items()
                if k not in ('KAIRO_COOP_SEAT0','KAIRO_COOP_SEAT1','KAIRO_COOP_TICKET')}
    def spawn(*args, env=None):
        p = subprocess.Popen([executable, *map(str,args)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                             env=base_env if env is None else env)
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
        # Protected server assigns seats by tickets, not accept order.
        # A bad, truncated or replayed ticket may not consume a seat.
        a_ticket, b_ticket = secrets.token_hex(32), secrets.token_hex(32)
        protected_env = dict(base_env, KAIRO_COOP_SEAT0=a_ticket, KAIRO_COOP_SEAT1=b_ticket)
        server = spawn('server', env=protected_env); port = ready(server)
        def ticket_bytes(token):
            return b''.join(struct.pack('!4sBBBBIii', b'KNET',1,2,0,0,
                                        int(token[i*8:i*8+8],16),i,7291) for i in range(8))
        def reject(data):
            with socket.create_connection(('127.0.0.1',port),timeout=5) as peer:
                peer.sendall(data)
                try: received=peer.recv(1)
                except ConnectionResetError: received=b''
                if received: raise RuntimeError('Rejected ticket received game data')
        reject(ticket_bytes(secrets.token_hex(32)))
        reject(b'BAD!'+ticket_bytes(a_ticket)[4:20])
        reject(ticket_bytes(a_ticket)[:20]) # fixed handshake deadline
        def receive_exact(peer,n):
            data=b''
            while len(data)<n:
                part=peer.recv(n-len(data))
                if not part: raise RuntimeError('Truncated protected snapshot')
                data+=part
            return data
        with socket.create_connection(('127.0.0.1',port),timeout=5) as raw:
            raw.sendall(ticket_bytes(b_ticket)) # seat 1 connects first
            reject(ticket_bytes(b_ticket)) # replay must not take seat 0
            client=spawn('client',port,env=dict(base_env,KAIRO_COOP_TICKET=a_ticket))
            hello=receive_exact(raw,60)
            if struct.unpack('!4sBBBBIii',hello[:20])[-2:]!=(1,0):
                raise RuntimeError('Ticket did not retain seat 1')
            for t in range(1,7):
                raw.sendall(frame(t,1,0))
                receive_exact(raw,40)
            out,err=client.communicate(timeout=12)
            if client.returncode or out.splitlines()!=['SLOT 0',*expected]:
                raise RuntimeError(f'Protected client mismatch: {out} {err}')
        out,err=server.communicate(timeout=12)
        if server.returncode or out.strip()!='SERVER 6 7 1 7 5 1':
            raise RuntimeError(f'Protected server mismatch: {out} {err}')
        # Both native CLI clients must also be able to perform admission.
        server=spawn('server',env=protected_env);port=ready(server)
        for_ticket=[(a_ticket,0),(b_ticket,1)]
        clients=[spawn('client',port,env=dict(base_env,KAIRO_COOP_TICKET=token)) for token,_ in for_ticket]
        for client,(_,slot) in zip(clients,for_ticket):
            out,err=client.communicate(timeout=12)
            if client.returncode or out.splitlines()!=[f'SLOT {slot}',*expected]:
                raise RuntimeError(f'Protected pair mismatch: {out} {err}')
        out,err=server.communicate(timeout=12)
        if server.returncode or out.strip()!='SERVER 6 7 1 7 5 1':
            raise RuntimeError(f'Protected pair server mismatch: {out} {err}')
        print('Protected seats passed wrong, malformed, truncated and replay rejection; both native clients reached victory')
        # Issue the exact server credentials via the real session directory.
        # Start listening first, publish second, join atomically, then launch.
        with tempfile.TemporaryDirectory(prefix='kairo-protected-directory-') as tmp:
            directory=Directory(Path(tmp)/'sessions.db')
            tickets=(secrets.token_hex(32),secrets.token_hex(32))
            server=spawn('server',env=dict(base_env,KAIRO_COOP_SEAT0=tickets[0],KAIRO_COOP_SEAT1=tickets[1]))
            port=ready(server)
            session=directory.create('coop',port,tickets=tickets)
            try:
                listing=directory.list()
                if len(listing)!=1 or listing[0]['id']!=session['id']:
                    raise RuntimeError('Protected directory discovery failed')
                members=[directory.join(listing[0]['id'],player) for player in ('alice','bob')]
                if [(m['seat'],m['token']) for m in members]!=list(enumerate(tickets)):
                    raise RuntimeError('Directory issued unexpected seats')
                clients=[spawn('client',m['port'],env=dict(base_env,KAIRO_COOP_TICKET=m['token'])) for m in reversed(members)]
                for client,member in zip(clients,reversed(members)):
                    out,err=client.communicate(timeout=12)
                    if client.returncode or out.splitlines()!=[f"SLOT {member['seat']}",*expected]:
                        raise RuntimeError(f'Directory client failed: {out} {err}')
                out,err=server.communicate(timeout=12)
                if server.returncode or out.strip()!='SERVER 6 7 1 7 5 1':
                    raise RuntimeError(f'Directory server failed: {out} {err}')
                for member in members:
                    if not directory.leave(member['token']):
                        raise RuntimeError('Protected membership cleanup failed')
                if directory.list()[0]['joined']!=2:
                    raise RuntimeError('Protected seats were recycled')
            finally:
                if not directory.close(session['id'],session['owner']) or directory.list():
                    raise RuntimeError('Protected directory cleanup failed')
        print('Protected directory discovery, ticket issuance, native co-op victory and cleanup passed')
        # Exercise the actual user-facing host/list/join commands in separate
        # processes as well, without exposing credentials in process arguments.
        with tempfile.TemporaryDirectory(prefix='kairo-launcher-') as tmp:
            db=Path(tmp)/'sessions.db'
            launcher=Path(__file__).with_name('coop_directory.py')
            prefix=[sys.executable,str(launcher),str(db)]
            host=subprocess.Popen([*prefix,'host',executable,'arena'],
                                  stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=base_env)
            processes.append(host)
            lines=queue.Queue(maxsize=1)
            threading.Thread(target=lambda:lines.put(host.stdout.readline()),daemon=True).start()
            announcement=lines.get(timeout=8).split()
            if len(announcement)!=3 or announcement[0]!='SESSION':
                raise RuntimeError('Launcher did not advertise session')
            session=announcement[1]
            listing=subprocess.run([*prefix,'list'],capture_output=True,text=True,check=True,timeout=8,env=base_env)
            import json
            rows=json.loads(listing.stdout)
            if len(rows)!=1 or rows[0]['id']!=session or not rows[0]['protected']:
                raise RuntimeError('Launcher session listing mismatch')
            clients=[]
            for player in ('alice','bob'):
                client=subprocess.Popen([*prefix,'join',executable,session,player],
                                         stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=base_env)
                clients.append(client);processes.append(client)
            slots=set()
            for client in clients:
                out,err=client.communicate(timeout=15)
                lines=out.splitlines()
                if client.returncode or not lines or lines[0] not in ('SLOT 0','SLOT 1') or lines[1:]!=expected:
                    raise RuntimeError(f'Launcher client mismatch: {out} {err}')
                slots.add(lines[0])
            out,err=host.communicate(timeout=12)
            if host.returncode or len(slots)!=2 or out.strip()!='SERVER 6 7 1 7 5 1' or Directory(db).list():
                raise RuntimeError(f'Launcher host mismatch: {out} {err}')
        print('User-facing host, discover and join commands completed a protected match')



    finally:
        for process in processes: stop(process)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary',type=Path);run(parser.parse_args().binary)
