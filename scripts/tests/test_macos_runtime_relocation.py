import copy
import unittest
from macos_runtime_relocation import EXPECTED, plan


def evidence():
    imports = list(EXPECTED)
    edges = [{'import': name, 'status': 'unique-file-candidate', 'candidates': ['/opt/homebrew/Cellar/' + EXPECTED[name]], 'sha256': str(i) * 64} for i, name in enumerate(imports, 1)]
    return {'target': 'macos-arm64', 'tools': [
        {'path': f'build/game-clang/Runtime/KairoPlayer/tool{i}', 'sha256': '0' * 64,
         'imports': imports + ['/usr/lib/libSystem.B.dylib'] if i == 0 else ['/usr/lib/libSystem.B.dylib'],
         'host_resolution': {'images': [{'edges': edges}] if i == 0 else []}}
        for i in range(7)]}


class Tests(unittest.TestCase):
    def test_expected_plan(self):
        tools, libraries = plan(evidence())
        self.assertEqual(len(tools), 7)
        self.assertEqual(set(libraries), set(EXPECTED))
        self.assertEqual(sum(bool(row['changes']) for row in tools.values()), 1)

    def test_unsafe_or_unknown_rejected(self):
        for mutate in (lambda e: e['tools'][0].update(path='build/game-clang/Runtime/../tool'),
                       lambda e: e['tools'][0]['imports'].append('/tmp/unknown.dylib'),
                       lambda e: e['tools'][0]['host_resolution']['images'][0]['edges'][0].update(status='ambiguous'),
                       lambda e: e['tools'][1].update(path=e['tools'][0]['path'])):
            data = copy.deepcopy(evidence())
            mutate(data)
            with self.assertRaises(ValueError):
                plan(data)


if __name__ == '__main__':
    unittest.main()
