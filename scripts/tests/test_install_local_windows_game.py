"""Filesystem fixtures test the real ZIP and local installation contract."""
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

import kairo_release as release
from install_local_windows_game import DLLS, install


def pe(machine=0x8664, dll=False):
    data = bytearray(128)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 64)
    data[64:68] = b'PE\0\0'
    struct.pack_into('<H', data, 68, machine)
    struct.pack_into('<H', data, 86, 0x2000 if dll else 2)
    struct.pack_into('<H', data, 88, 0x20b)
    return bytes(data)


def sha(data):
    return hashlib.sha256(data).hexdigest()


class Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        bundle = self.root / 'Native bundle'
        (bundle / 'bin').mkdir(parents=True)
        (bundle / 'project').mkdir()
        (bundle / 'bin/KairoPlayer.exe').write_bytes(pe())
        (bundle / 'project/Project.kproject').write_bytes(b'fixture')
        (bundle / 'run.cmd').write_text('fixture')
        (bundle / 'package.kmanifest').write_text('kairo-package 1\nproject project/Project.kproject\nprofile Release\nkind release\nengine-version 0.1.0\nplayer bin/KairoPlayer.exe\nproject-files 1\nproject-bytes 7\n')
        self.archive = self.root / 'Game.zip'
        self.index = release.export(bundle, self.archive)
        self.original = sha(self.archive.read_bytes())
        self.runtime = self.root / 'Local runtime'
        self.runtime.mkdir()
        self.destination = self.root / 'Installed game'
        self.hashes = {}
        for name in DLLS:
            data = pe(dll=True) + name.encode()
            (self.runtime / name).write_bytes(data)
            self.hashes[name] = sha(data)

    def test_install_and_no_overwrite(self):
        receipt = install(self.archive, self.destination, self.runtime, self.hashes)
        self.assertEqual(receipt['source_archive_sha256'], self.original)
        self.assertTrue(receipt['private_installation_only'])
        self.assertFalse(receipt['redistribution_approved'])
        self.assertEqual(json.loads((self.destination / 'release.json').read_text()), self.index)
        self.assertEqual(json.loads((self.destination / 'local-runtime.json').read_text()), receipt)
        for name in DLLS:
            self.assertEqual(sha((self.destination / 'bin' / name).read_bytes()), self.hashes[name])
        self.assertEqual(sha(self.archive.read_bytes()), self.original)
        with self.assertRaises(ValueError):
            install(self.archive, self.destination, self.runtime, self.hashes)
        self.assertEqual(json.loads((self.destination / 'local-runtime.json').read_text()), receipt)

    def test_invalid_dlls_fail_before_extract(self):
        for data in (pe(machine=0xaa64, dll=True), pe(dll=False), b'not a binary'):
            (self.runtime / DLLS[0]).write_bytes(data)
            self.hashes[DLLS[0]] = sha(data)
            with self.assertRaises(ValueError):
                install(self.archive, self.destination, self.runtime, self.hashes)
            self.assertFalse(self.destination.exists())
        (self.runtime / DLLS[0]).write_bytes(pe(dll=True))
        with self.assertRaises(ValueError):
            install(self.archive, self.destination, self.runtime, self.hashes)
        self.assertFalse(self.destination.exists())

    def test_copy_failure_rolls_back_only_new_install(self):
        original_copy = release.copy_hash
        def fail_on_dll(src, dest=None):
            if dest is not None and str(getattr(dest, 'name', '')).endswith('.dll'):
                raise OSError('fixture copy failure')
            return original_copy(src, dest)
        with patch('kairo_release.copy_hash', side_effect=fail_on_dll):
            with self.assertRaisesRegex(OSError, 'fixture copy failure'):
                install(self.archive, self.destination, self.runtime, self.hashes)
        self.assertFalse(self.destination.exists())
        self.assertEqual(sha(self.archive.read_bytes()), self.original)
        self.assertTrue(all((self.runtime / name).is_file() for name in DLLS))


if __name__ == '__main__':
    unittest.main()
