import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import unittest

import unix_dependency_resolution as audit


class Tests(unittest.TestCase):
    def test_loader_parser(self):
        rows = audit.parse_loader_list('linux-vdso.so.1 (0x123)\n libx.so => /path with space/libx.so (0xabc)\n /lib/ld.so (0xabc)\n libz.so => not found\n')
        self.assertEqual([r['status'] for r in rows], ['kernel-virtual-object', 'resolved-file', 'resolved-file', 'unresolved'])
        self.assertEqual(rows[1]['path'], '/path with space/libx.so')
        for text in ('garbage', 'libx => relative.so (0x123)', 'linux-vdso.so.1 (0x123)\nlinux-vdso.so.1 (0x123)'):
            with self.assertRaises(ValueError):
                audit.parse_loader_list(text)

    @unittest.skipUnless(sys.platform.startswith('linux') and shutil.which('cc') and platform.machine() == 'x86_64', 'Linux x86_64 compiler required')
    def test_real_transitive_elf_and_missing_library(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'b.c').write_text('int b(void){return 7;}')
            (root / 'a.c').write_text('int b(void); int a(void){return b();}')
            (root / 'main.c').write_text('int a(void); int main(void){return a()!=7;}')
            def cc(*args):
                subprocess.run(['cc', *args], cwd=root, check=True, capture_output=True)
            cc('-shared', '-fPIC', 'b.c', '-Wl,-soname,libkairo_b.so', '-o', 'libkairo_b.so')
            cc('-shared', '-fPIC', 'a.c', '-L.', '-lkairo_b', '-Wl,-rpath,$ORIGIN', '-Wl,-soname,libkairo_a.so', '-o', 'libkairo_a.so')
            cc('main.c', '-L.', '-lkairo_a', '-Wl,-rpath,$ORIGIN', '-Wl,-rpath-link,.', '-o', 'app')
            args = (root / 'app', 'linux-x86_64', '/lib64/ld-linux-x86-64.so.2')
            result = audit.linux_loader_inventory(*args)
            rows = {r['name']: r for r in result['objects']}
            for name in ('libkairo_a.so', 'libkairo_b.so'):
                self.assertEqual(rows[name]['sha256'], audit.digest(root / name))
            self.assertFalse(result['fresh_machine_qualified'])
            (root / 'libkairo_b.so').unlink()
            with self.assertRaises(ValueError):
                audit.linux_loader_inventory(*args)
            with self.assertRaises(ValueError):
                audit.linux_loader_inventory(root / 'app', 'linux-x86_64', '/bin/sh')

    def test_macos_path_candidates(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            owner = root / 'Frameworks/a.dylib'; owner.parent.mkdir(); owner.touch()
            exe = root / 'MacOS/player'; exe.parent.mkdir(); exe.touch()
            library = owner.parent / 'b.dylib'; library.touch()
            self.assertEqual(audit.macos_candidates('@loader_path/b.dylib', owner, exe, [])['candidates'], [str(library)])
            self.assertEqual(audit.macos_candidates('@rpath/b.dylib', owner, exe, ['@executable_path/../Frameworks'])['status'], 'unique-file-candidate')
            (exe.parent / 'b.dylib').touch()
            self.assertEqual(audit.macos_candidates('@rpath/b.dylib', owner, exe, ['@loader_path', '@executable_path'])['status'], 'ambiguous')
            self.assertEqual(audit.macos_candidates('@rpath/missing.dylib', owner, exe, [])['status'], 'unresolved')
            self.assertEqual(audit.macos_candidates('/usr/lib/libSystem.B.dylib', owner, exe, [])['status'], 'os-library-reference')

    def test_macos_graph_cycle_and_missing_candidate(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            exe = root / 'player'; exe.touch()
            a = root / 'a.dylib'; a.touch()
            b = root / 'b.dylib'; b.touch()
            def evidence(*names):
                return {'imports': list(names), 'search_paths': ['@loader_path']}
            deps = {a: evidence('@rpath/b.dylib'), b: evidence('@rpath/a.dylib', '@rpath/missing.dylib')}
            result = audit.macos_graph(exe, evidence('@rpath/a.dylib'), lambda path: deps[path])
            self.assertEqual(len(result['images']), 3)
            self.assertEqual(result['images'][-1]['edges'][-1]['status'], 'unresolved')
            self.assertFalse(result['host_loader_resolution_verified'])


if __name__ == '__main__':
    unittest.main()
