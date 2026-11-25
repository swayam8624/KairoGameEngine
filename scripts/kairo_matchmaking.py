#!/usr/bin/env python3
"""Local, explicitly shared SQLite session directory for the loopback network lab."""
import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import re
import secrets
import sqlite3
import time


class Directory:
    """A local directory, not a remote authentication or relay service.

    Each write takes a SQLite IMMEDIATE transaction, so independent processes
    cannot both claim the last available seat. Expired rows are cascaded away.
    The clock is injectable for deterministic tests; all times are Unix seconds.
    """

    def __init__(self, path, clock=time.time):
        self.path = Path(path)
        self.clock = clock
        with self.transaction() as db:
            db.execute('CREATE TABLE IF NOT EXISTS sessions (id TEXT PRIMARY KEY, name TEXT NOT NULL, port INTEGER NOT NULL UNIQUE, capacity INTEGER NOT NULL, expires REAL NOT NULL, owner TEXT NOT NULL)')
            db.execute('CREATE TABLE IF NOT EXISTS members (session TEXT REFERENCES sessions(id) ON DELETE CASCADE, player TEXT NOT NULL, token TEXT NOT NULL UNIQUE, PRIMARY KEY(session, player))')

    @contextmanager
    def transaction(self):
        """Acquire a cross-process write reservation; roll back on any exception."""
        db = sqlite3.connect(self.path, timeout=5, isolation_level=None)
        try:
            db.execute('PRAGMA foreign_keys=ON')
            db.execute('BEGIN IMMEDIATE')
            yield db
            db.commit()
        except BaseException:
            db.rollback()
            raise
        finally:
            db.close()

    def prune(self, db):
        """Remove dead advertisements before any availability decision."""
        db.execute('DELETE FROM sessions WHERE expires<=?', (self.clock(),))

    @staticmethod
    def label(value):
        if not isinstance(value, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', value):
            raise ValueError('Invalid session/player label')
        return value

    def create(self, name, port, capacity=2, ttl=60):
        """Advertise a loopback port already owned by a host. Capacity excludes host."""
        self.label(name)
        if type(port) is not int or not 1 <= port <= 65535 or type(capacity) is not int or not 1 <= capacity <= 64 or type(ttl) is not int or not 1 <= ttl <= 3600:
            raise ValueError('Invalid port, capacity or lifetime')
        session, owner = secrets.token_hex(16), secrets.token_hex(32)
        with self.transaction() as db:
            self.prune(db)
            if db.execute('SELECT 1 FROM sessions WHERE port=?', (port,)).fetchone():
                raise ValueError('Port already advertised')
            db.execute('INSERT INTO sessions VALUES (?,?,?,?,?,?)', (session, name, port, capacity, self.clock()+ttl, owner))
        return {'id': session, 'owner': owner}

    def list(self):
        """Return public metadata only, never ownership or membership tokens."""
        with self.transaction() as db:
            self.prune(db)
            rows = db.execute('SELECT s.id,s.name,s.port,s.capacity,COUNT(m.player) FROM sessions s LEFT JOIN members m ON s.id=m.session GROUP BY s.id ORDER BY s.name,s.id').fetchall()
        return [dict(id=i, name=n, host='127.0.0.1', port=p, capacity=c, joined=j) for i,n,p,c,j in rows]

    def join(self, session, player):
        """Reserve one seat atomically; duplicate player IDs cannot consume two."""
        self.label(player)
        token = secrets.token_hex(32)
        with self.transaction() as db:
            self.prune(db)
            row = db.execute('SELECT port,capacity FROM sessions WHERE id=?', (session,)).fetchone()
            if row is None:
                raise ValueError('Session unavailable')
            if db.execute('SELECT 1 FROM members WHERE session=? AND player=?', (session,player)).fetchone():
                raise ValueError('Player already joined')
            if db.execute('SELECT COUNT(*) FROM members WHERE session=?', (session,)).fetchone()[0] >= row[1]:
                raise ValueError('Session full')
            db.execute('INSERT INTO members VALUES (?,?,?)', (session,player,token))
        return {'host': '127.0.0.1', 'port': row[0], 'token': token}

    def leave(self, token):
        """Release one membership; unknown/expired tokens are harmless."""
        with self.transaction() as db:
            self.prune(db)
            return db.execute('DELETE FROM members WHERE token=?', (token,)).rowcount == 1

    def close(self, session, owner):
        """Close only a session with its owner token; cascade all members."""
        with self.transaction() as db:
            self.prune(db)
            return db.execute('DELETE FROM sessions WHERE id=? AND owner=?', (session,owner)).rowcount == 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('database', type=Path)
    sub = parser.add_subparsers(dest='command', required=True)
    create = sub.add_parser('create'); create.add_argument('name'); create.add_argument('port', type=int); create.add_argument('--capacity', type=int, default=2); create.add_argument('--ttl', type=int, default=60)
    sub.add_parser('list')
    join = sub.add_parser('join'); join.add_argument('session'); join.add_argument('player')
    leave = sub.add_parser('leave'); leave.add_argument('token')
    close = sub.add_parser('close'); close.add_argument('session'); close.add_argument('owner')
    args = parser.parse_args()
    try:
        directory = Directory(args.database)
        if args.command == 'create': result = directory.create(args.name,args.port,args.capacity,args.ttl)
        elif args.command == 'list': result = directory.list()
        elif args.command == 'join': result = directory.join(args.session,args.player)
        elif args.command == 'leave': result = directory.leave(args.token)
        else: result = directory.close(args.session,args.owner)
        print(json.dumps(result))
    except (ValueError, sqlite3.Error, OSError) as error:
        parser.exit(1, f'Matchmaking rejected: {error}\n')


if __name__ == '__main__':
    main()
