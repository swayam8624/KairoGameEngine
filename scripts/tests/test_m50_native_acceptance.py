"""Exercise the report checker with actual CMake/CTest fixture processes."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from run_m50_native_acceptance import GATES, run_ctest


class NativeEvidenceTests(unittest.TestCase):
    def fixture(self, mode):
        root = Path(self.tmp.name)
        source = root/'src';source.mkdir()
        build = root/'build'
        lines = ['cmake_minimum_required(VERSION 3.21)', 'project(NativeEvidenceFixture NONE)', 'enable_testing()']
        for i, name in enumerate(sorted(GATES)):
            if i == 0 and mode == 'missing': continue
            command = 'false' if i == 0 and mode in ('failed','skipped') else 'true'
            lines.append(f'add_test(NAME {name} COMMAND ${{CMAKE_COMMAND}} -E {command})')
            lines.append(f'set_tests_properties({name} PROPERTIES LABELS m50-native)')
            if i == 0 and mode == 'skipped': lines.append(f'set_tests_properties({name} PROPERTIES SKIP_RETURN_CODE 1)')
        (source/'CMakeLists.txt').write_text('\n'.join(lines)+'\n')
        subprocess.run(['cmake','-S',str(source),'-B',str(build)],check=True,capture_output=True,timeout=20)
        return build,root/'evidence'

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def test_complete(self):
        build,evidence = self.fixture('passed')
        self.assertEqual(set(run_ctest(build,evidence)), GATES)
        self.assertTrue((evidence/'results.xml').is_file())
        with self.assertRaises(FileExistsError): run_ctest(build,evidence)

    def test_missing(self):
        build,evidence = self.fixture('missing')
        with self.assertRaisesRegex(RuntimeError,'Wrong native gate inventory'): run_ctest(build,evidence)

    def test_failed(self):
        build,evidence = self.fixture('failed')
        with self.assertRaisesRegex(RuntimeError,'Native CTest failed'): run_ctest(build,evidence)

    def test_skipped(self):
        build,evidence = self.fixture('skipped')
        with self.assertRaisesRegex(RuntimeError,'did not run successfully'): run_ctest(build,evidence)


if __name__ == '__main__':
    unittest.main()
