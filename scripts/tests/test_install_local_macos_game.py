import hashlib
from pathlib import Path
import struct
import tempfile
import unittest

from install_local_macos_game import LIBRARIES, inspect, preflight


def string_command(command, value, prefix):
    payload = value.encode() + b'\0'
    length = (prefix + len(payload) + 7) & ~7
    return struct.pack('<III', command, length, prefix) + bytes(prefix-12) + payload + bytes(length-prefix-len(payload))


def image(kind, commands, cpu=0x0100000c):
    data = b''.join(commands)
    return struct.pack('<8I', 0xfeedfacf, cpu, 0, kind, len(commands), len(data), 0, 0) + data


class Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.player = self.root / 'Player'
        self.runtime = self.root / 'Runtime'
        self.runtime.mkdir()
        self.system = '/usr/lib/libSystem.B.dylib'
        self.commands = [string_command(0xc, name, 24) for name in LIBRARIES]
        self.commands += [string_command(0xc, self.system, 24), string_command(0x8000001c, '/opt/homebrew/lib', 12)]
        self.player.write_bytes(image(2, self.commands))
        self.hashes = {}
        for old, name in LIBRARIES.items():
            data = image(6, [string_command(0xd, old, 24), string_command(0xc, self.system, 24)])
            (self.runtime / name).write_bytes(data)
            self.hashes[name] = hashlib.sha256(data).hexdigest()

    def test_plan_exact_dependencies(self):
        plan, sources = preflight(self.player, self.runtime, self.hashes)
        self.assertEqual(set(sources), set(LIBRARIES))
        self.assertEqual(plan['rpaths'], ['/opt/homebrew/lib'])
        self.assertEqual(plan['identities'], [])

    def test_bad_header_and_command_rejected(self):
        for data in (b'bad', image(2, self.commands, 0x01000007), image(6, self.commands),
                     image(2, [struct.pack('<II', 0xc, 0)]), image(2, self.commands)[:-1]):
            self.player.write_bytes(data)
            with self.assertRaises(ValueError):
                inspect(self.player, 2)

    def test_unknown_import_and_dylib_hash_or_identity_rejected(self):
        self.player.write_bytes(image(2, self.commands + [string_command(0xc, '/tmp/unknown.dylib', 24)]))
        with self.assertRaises(ValueError):
            preflight(self.player, self.runtime, self.hashes)
        self.player.write_bytes(image(2, self.commands))
        old, name = next(iter(LIBRARIES.items()))
        data = image(6, [string_command(0xd, old, 24), string_command(0xc, '/tmp/unknown.dylib', 24)])
        (self.runtime / name).write_bytes(data)
        with self.assertRaises(ValueError):
            preflight(self.player, self.runtime, self.hashes)
        self.hashes[name] = hashlib.sha256(data).hexdigest()
        with self.assertRaises(ValueError):
            preflight(self.player, self.runtime, self.hashes)
        data = image(6, [string_command(0xd, '/tmp/wrong-id', 24)])
        (self.runtime / name).write_bytes(data)
        self.hashes[name] = hashlib.sha256(data).hexdigest()
        with self.assertRaises(ValueError):
            preflight(self.player, self.runtime, self.hashes)


if __name__ == '__main__':
    unittest.main()
