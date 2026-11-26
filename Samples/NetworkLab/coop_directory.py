#!/usr/bin/env python3
"""Host, discover and join a protected local Kairo co-op match."""
import argparse
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
from kairo_matchmaking import Directory
from run_coop import ready, stop


def clean_env():
    return {k:v for k,v in os.environ.items()
            if k not in ('KAIRO_COOP_SEAT0','KAIRO_COOP_SEAT1','KAIRO_COOP_TICKET')}


def host(directory,binary,name):
    """Keep the advertisement alive only while this server is running."""
    tickets=(secrets.token_hex(32),secrets.token_hex(32))
    server=subprocess.Popen([str(binary.resolve(strict=True)),'server'],
                            stdout=subprocess.PIPE,stderr=None,text=True,
                            env=dict(clean_env(),KAIRO_COOP_SEAT0=tickets[0],KAIRO_COOP_SEAT1=tickets[1]))
    session=None
    try:
        port=ready(server)
        session=directory.create(name,port,ttl=25,tickets=tickets)
        print(f"SESSION {session['id']} {port}",flush=True)
        out,_=server.communicate()
        if out:
            print(out,end='',flush=True)
        return server.returncode
    finally:
        try:
            stop(server)
        finally:
            if session is not None:
                directory.close(session['id'],session['owner'])


def join(directory,binary,session,player,graphical=False,backend='auto',smoke=False):
    """Reserve once, pass only this seat credential, then clean membership."""
    executable=str(binary.resolve(strict=True))
    member=directory.join(session,player)
    try:
        if 'seat' not in member:
            raise ValueError('Select a protected co-op session')
        args=[executable,str(member['port'])] if graphical else [executable,'client',str(member['port'])]
        if graphical:
            if backend!='auto':
                args+=['--renderer',backend]
            if smoke:
                args+=['--smoke']
        return subprocess.call(args,env=dict(clean_env(),KAIRO_COOP_TICKET=member['token']))
    finally:
        directory.leave(member['token'])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('database',type=Path,help='Trusted local SQLite file')
    sub=parser.add_subparsers(dest='command',required=True)
    h=sub.add_parser('host');h.add_argument('binary',type=Path);h.add_argument('name')
    sub.add_parser('list')
    j=sub.add_parser('join');j.add_argument('binary',type=Path);j.add_argument('session');j.add_argument('player')
    j.add_argument('--graphical',action='store_true')
    j.add_argument('--renderer',choices=('auto','metal','vulkan'),default='auto')
    j.add_argument('--smoke',action='store_true')
    args=parser.parse_args()
    try:
        directory=Directory(args.database)
        if args.command=='host':
            return host(directory,args.binary,args.name)
        if args.command=='list':
            print(json.dumps(directory.list()));return 0
        if not args.graphical and (args.smoke or args.renderer!='auto'):
            raise ValueError('Renderer and smoke flags require --graphical')
        return join(directory,args.binary,args.session,args.player,args.graphical,args.renderer,args.smoke)
    except (ValueError,OSError,RuntimeError,subprocess.SubprocessError) as error:
        print(f'Co-op launcher failed: {error}',file=sys.stderr);return 1
    except KeyboardInterrupt:
        return 130


if __name__=='__main__':
    sys.exit(main())
