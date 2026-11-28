#!/usr/bin/env python3
"""Launch the real server and two graphical clients; require both GPU captures."""
import argparse
import os
import secrets
from pathlib import Path
import subprocess
import tempfile
from run_coop import ready, stop


def run(server_binary, client_binary, backend):
    base_env={k:v for k,v in os.environ.items()
              if k not in ('KAIRO_COOP_SEAT0','KAIRO_COOP_SEAT1','KAIRO_COOP_TICKET')}
    tickets=[secrets.token_hex(32) for _ in range(2)]
    server = subprocess.Popen([str(server_binary.resolve(strict=True)), 'server'],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                              env=dict(base_env,KAIRO_COOP_SEAT0=tickets[0],KAIRO_COOP_SEAT1=tickets[1]))
    clients = []
    try:
        port = ready(server)
        expected = [f'COOP TURN {t} {1+t} 1 {1+t} 5 {int(t==6)}' for t in range(1,7)]
        slots = set()
        # Redirect graphics logs to files so an unread pipe cannot block a GPU
        # process while we wait for the other client. Preserve errors in CTest.
        with tempfile.TemporaryDirectory(prefix='kairo-coop-native-') as tmp:
            logs = [Path(tmp)/f'client-{i}.log' for i in range(2)]
            try:
                for slot,path in enumerate(logs):
                    with path.open('w',encoding='utf-8') as output:
                        clients.append(subprocess.Popen([str(client_binary.resolve(strict=True)), str(port),
                                                         '--renderer',backend,'--smoke'],stdout=output,stderr=subprocess.STDOUT,
                                                         env=dict(base_env,KAIRO_COOP_TICKET=tickets[slot])))
                for slot,(client,path) in enumerate(zip(clients,logs)):
                    code=client.wait(timeout=75)
                    text=path.read_text(encoding='utf-8',errors='replace')
                    lines=[line for line in text.splitlines() if line.startswith('COOP ')]
                    if code or not lines or lines[0]!=f'COOP SLOT {slot}' or lines[1:]!=expected+['COOP CAPTURE PASS']:
                        raise RuntimeError(f'Native client mismatch ({code}): {text}')
                    slots.add(lines[0])
                out,err=server.communicate(timeout=10)
                if server.returncode or out.strip()!='SERVER 6 7 1 7 5 1' or len(slots)!=2:
                    raise RuntimeError(f'Native co-op server mismatch: {out} {err} {slots}')
            except BaseException:
                for client in clients:
                    if client.poll() is None:client.kill()
                    client.wait(timeout=5)
                for path in logs:
                    if path.exists():print(path.name,path.read_text(encoding='utf-8',errors='replace'))
                # The client's EOF is usually a consequence, not the cause.
                # Preserve the actual server error even if it already exited.
                if server.poll() is None:server.kill()
                out,err=server.communicate(timeout=5)
                print(f"server exit={server.returncode}\nstdout: {out}\nstderr: {err}")
                raise
        print('Both graphical clients reached the same authoritative victory and changed actual native captures')
    finally:
        for client in clients:
            if client.poll() is None:client.kill()
            client.wait(timeout=5)
        stop(server)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('server',type=Path);parser.add_argument('client',type=Path)
    parser.add_argument('backend',choices=('metal','vulkan'))
    args=parser.parse_args();run(args.server,args.client,args.backend)
