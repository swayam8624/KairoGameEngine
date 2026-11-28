import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import debian_dependencies as dep


class DebianDependencyTests(unittest.TestCase):
    def test_versioned_relations(self):
        self.assertEqual(dep.parse_shlibdeps('shlibs:Depends=libc6 (>= 2.34), libstdc++6 (>= 14), libglfw3\n'),
                         ['libc6 (>= 2.34)', 'libstdc++6 (>= 14)', 'libglfw3'])
        self.assertTrue(dep.valid_dependency('libc6 (>= 1:2.34-1~deb12u1)'))

    def test_reject_unsupported_and_injection(self):
        for value in ['', 'libc6 | libc7', 'libc6:any', 'libc6 (>= 2.3)\nMaintainer: other',
                      'libc6 (>= 2.3), libc6 (>= 2.3)', 'libc6,', 'libc6 (>= 2.3); evil']:
            with self.subTest(value=value), self.assertRaises(ValueError):
                dep.parse_shlibdeps('shlibs:Depends=' + value)
        self.assertFalse(dep.valid_dependency('libc6\n'))

    def test_resolver_uses_real_tool_and_fails_closed(self):
        with tempfile.TemporaryDirectory() as temp:
            binary = Path(temp) / 'game'
            binary.write_bytes(b'fixture')
            with patch.object(dep.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, 'shlibs:Depends=libc6 (>= 2.34)\n', '')) as run:
                self.assertEqual(dep.resolve(binary), ['libc6 (>= 2.34)'])
                self.assertEqual(run.call_args.args[0], ['dpkg-shlibdeps', '-O', '-e' + str(binary)])
                self.assertTrue(run.call_args.kwargs['check'])
            with patch.object(dep.subprocess, 'run', side_effect=subprocess.CalledProcessError(2, 'dpkg-shlibdeps')):
                with self.assertRaises(subprocess.CalledProcessError):
                    dep.resolve(binary)


if __name__ == '__main__':
    unittest.main()
