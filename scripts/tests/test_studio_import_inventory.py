import tempfile
from pathlib import Path
import unittest
import studio_import_inventory as audit


class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.dll = self.root / 'dll'
        self.system = self.root / 'system'
        self.dll.mkdir()
        self.system.mkdir()

    def put(self, directory, name):
        path = directory / name
        path.write_bytes(name.encode())
        return path

    def test_recursive_graph_and_cycle(self):
        self.put(self.dll, 'A.DLL')
        self.put(self.dll, 'b.dll')
        self.put(self.system, 'kernel32.dll')
        deps = {'a.dll': ['b.dll', 'kernel32.dll'], 'b.dll': ['A.dll', 'missing.dll']}
        graph = audit.windows_candidates(['a.dll', 'api-ms-win-test.dll'], [self.dll], self.system,
                                          lambda p: {'imports': deps[p.name.lower()]})
        rows = {r['name']: r for r in graph}
        self.assertEqual(rows['a.dll']['status'], 'explicit-root-candidate')
        self.assertEqual(rows['b.dll']['status'], 'explicit-root-candidate')
        self.assertEqual(rows['missing.dll']['status'], 'unresolved')
        self.assertEqual(rows['kernel32.dll']['status'], 'host-system-directory-present')
        self.assertEqual(rows['api-ms-win-test.dll']['status'], 'os-api-contract')

    def test_ambiguous_and_system_collision_not_traversed(self):
        other = self.root / 'other'
        other.mkdir()
        self.put(self.dll, 'a.dll')
        self.put(other, 'A.DLL')
        self.put(self.dll, 'system.dll')
        self.put(self.system, 'system.dll')
        rows = audit.windows_candidates(['a.dll', 'system.dll'], [self.dll, other], self.system,
                                        lambda p: self.fail('Ambiguous/system candidate was inspected'))
        self.assertEqual([r['status'] for r in rows], ['ambiguous', 'system-and-external-candidates'])

    def test_every_tool_and_mixed_target_rejection(self):
        for name in ('editor', 'player'):
            self.put(self.root, name)
        def inspect(p, t):
            return {'imports': [p.name + '.so'], 'search_paths': [], 'interpreter': '/lib/ld.so', 'tool': 'fixture'}
        result = audit.collect(self.root, ['player', 'editor'], inspect, lambda p: 'linux-x86_64')
        self.assertEqual(result['direct_imports'], ['editor.so', 'player.so'])
        self.assertEqual(len(result['tools']), 2)
        self.assertFalse(result['dependency_closure_verified'])
        with self.assertRaises(ValueError):
            audit.collect(self.root, ['player', 'editor'], inspect, lambda p: p.name)

    def test_invalid_import_rejected(self):
        with self.assertRaises(ValueError):
            audit.windows_candidates(['../evil.dll'], [self.dll], self.system, lambda p: {})


if __name__ == '__main__':
    unittest.main()
