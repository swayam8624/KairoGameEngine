import shutil
import subprocess
import tempfile
from pathlib import Path
import unittest

import native_dependency_audit as audit


class DependencyTests(unittest.TestCase):
    def test_real_elf(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / 'main.c'
            source.write_text('#include <stdio.h>\nint main(void){return puts("fixture")<0;}\n')
            binary = root / 'test binary'
            subprocess.run(['cc', str(source), '-Wl,-rpath,$ORIGIN/lib', '-o', str(binary)], check=True)
            result = audit.audit(binary, 'linux-x86_64')
            self.assertTrue(result['imports'])
            self.assertEqual(result['search_paths'], ['$ORIGIN/lib'])
            self.assertTrue(result['interpreter'])
            self.assertFalse(result['dependency_closure_verified'])
            self.assertFalse(result['third_party_licenses_verified'])

    def test_macho_universal_fixture(self):
        linked = ('game (architecture x86_64):\n'
                  '\t@rpath/libglfw.3.dylib (compatibility version 3.0.0, current version 3.4.0)\n'
                  'game (architecture arm64):\n'
                  '\t/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1351.0.0)\n')
        commands = 'Load command 0\n cmd LC_RPATH\n cmdsize 40\n path @loader_path/lib (offset 12)\n'
        result = audit.parse_macho(linked, commands)
        self.assertEqual(result['imports'], ['/usr/lib/libSystem.B.dylib', '@rpath/libglfw.3.dylib'])
        self.assertEqual(result['search_paths'], ['@loader_path/lib'])
        with self.assertRaises(ValueError):
            audit.parse_macho(linked, 'Load command 0\n cmd LC_RPATH\n')

    def test_pe_and_delay_imports(self):
        output = ('Dump of file game.exe\n Image has the following dependencies:\n'
                  '    KERNEL32.dll\n    glfw3.dll\n\n'
                  ' Image has the following delay load dependencies:\n'
                  '    vulkan-1.dll\n\n Summary\n  1000 .text\n')
        self.assertEqual(audit.parse_pe(output)['imports'], ['glfw3.dll', 'kernel32.dll', 'vulkan-1.dll'])
        with self.assertRaises(ValueError):
            audit.parse_pe(output.replace('glfw3.dll', 'unexpected path'))

    def test_invalid_and_static(self):
        for parser, args in [(audit.parse_elf, ('bad', 'bad')), (audit.parse_macho, ('bad', 'bad')),
                             (audit.parse_pe, ('bad',))]:
            with self.assertRaises(ValueError):
                parser(*args)
        self.assertEqual(audit.parse_elf('There is no dynamic section', 'Program Headers:')['imports'], [])
        with self.assertRaises(ValueError):
            audit.parse_elf('Dynamic section at offset 1\n(NEEDED) broken', 'Program Headers:')
        with self.assertRaises(ValueError):
            audit.audit(shutil.which('true'), 'windows-x86_64')


if __name__ == '__main__':
    unittest.main()
