"""Exercise the desktop workflow with real archives and injected native host identity."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from test_verify_private_game_install import Tests as ArchiveFixtures
from test_install_local_windows_game import pe
from install_local_windows_game import DLLS
from verify_private_game_install import record
from kairo_game_installer import install_game, read_ticket, use_ticket


class DesktopTests(unittest.TestCase):
    bundle = ArchiveFixtures.bundle

    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.archive, _ = self.bundle(True)
        self.runtime = self.root / 'Local runtime'
        self.runtime.mkdir()
        self.dest = self.root / 'Installed Game'
        self.ticket = self.root / 'Game ticket.json'
        self.hashes = []
        for name in DLLS:
            path = self.runtime / name
            path.write_bytes(pe(dll=True) + name.encode())
            self.hashes.append(record(path)['sha256'])

    def install(self, ticket=None, hashes=None):
        with patch('kairo_game_installer.platform.system', return_value='Windows'), \
             patch('kairo_game_installer.platform.machine', return_value='AMD64'):
            return install_game(self.archive, self.dest, self.runtime,
                                *(self.hashes if hashes is None else hashes),
                                ticket or self.ticket)

    def test_external_pin_and_no_replace(self):
        receipt = self.install()
        self.assertTrue(receipt['immutable_file_integrity_verified'])
        archive, dest, pin = read_ticket(self.ticket)
        self.assertEqual((archive, dest), (self.archive, self.dest))
        self.assertEqual(pin, record(dest / 'local-runtime.json')['sha256'])
        before = self.ticket.read_bytes()
        with self.assertRaisesRegex(ValueError, 'Ticket already exists'):
            self.install()
        self.assertEqual(self.ticket.read_bytes(), before)
        self.assertTrue(dest.is_dir())

    def test_ticket_must_be_external_and_wrong_hash_cannot_launch(self):
        with self.assertRaises(ValueError):
            self.install(self.dest / 'inside.json')
        self.assertFalse(self.dest.exists())
        with self.assertRaisesRegex(ValueError, 'outside the runtime source'):
            self.install(self.runtime / 'inside.json')
        with self.assertRaisesRegex(ValueError, 'Installation or ticket creation failed'):
            self.install(hashes=['0' * 64, self.hashes[1]])
        self.assertFalse(self.dest.exists())
        with self.assertRaises(ValueError):
            read_ticket(self.ticket)

    def test_ticket_checks_and_explicit_dispatch(self):
        self.install()
        with patch('launch_private_game.platform.system', return_value='Windows'), \
             patch('launch_private_game.platform.machine', return_value='AMD64'), \
             patch('kairo_game_installer.subprocess.run') as run:
            self.assertTrue(use_ticket(self.ticket, 'auto', 'check')['native_import_preflight_passed'])
            run.assert_not_called()
            run.return_value.returncode = 0
            self.assertEqual(use_ticket(self.ticket, 'd3d12', 'smoke')['exit_code'], 0)
            args = run.call_args.args[0]
            self.assertIn(str(self.archive), args)
            self.assertEqual(args[-4:], ['--renderer', 'd3d12', '--action', 'smoke'])
            run.return_value.returncode = 9
            with self.assertRaisesRegex(ValueError, 'exit 9'):
                use_ticket(self.ticket, 'auto', 'play')
            run.reset_mock()
            (self.dest / 'project/Assets/texture').write_bytes(b'changed')
            with self.assertRaises(ValueError):
                use_ticket(self.ticket, 'auto', 'check')
            run.assert_not_called()

    def test_malformed_tickets(self):
        for value in ({}, {'schema': True, 'archive': 'x', 'installation': 'y', 'receipt_sha256': 'z'},
                      {'schema': 1, 'archive': 'relative.zip', 'installation': 'relative', 'receipt_sha256': 'z'}):
            self.ticket.write_text(json.dumps(value))
            with self.assertRaises(ValueError):
                read_ticket(self.ticket)


if __name__ == '__main__':
    unittest.main()
