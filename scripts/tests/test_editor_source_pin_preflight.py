import pathlib
import subprocess
import tempfile
import unittest
import editor_source_pin_preflight as preflight


class Tests(unittest.TestCase):
    def test_literal_declarations(self):
        text = '\n'.join(f'FetchContent_Declare({name}\n GIT_REPOSITORY https://github.com/{origin}.git\n GIT_TAG {"a" * 40})' for name, (origin, _) in preflight.SOURCES.items())
        result = preflight.declarations(text)
        self.assertEqual(set(result), set(preflight.SOURCES))
        for bad in (text.replace('a' * 40, 'main'), text + '\n' + text,
                    text.replace('ocornut/imgui', 'unknown/imgui')):
            with self.assertRaises(ValueError):
                preflight.declarations(bad)

    def test_real_git_fetch_and_notice(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            def git(*args):
                return subprocess.check_output(['git', '-C', str(root), *args]).decode().strip()
            git('init', '-q')
            git('config', 'user.email', 'fixture@example.invalid')
            git('config', 'user.name', 'Fixture')
            (root / 'LICENSE').write_text('fixture notice\n')
            git('add', '.')
            git('commit', '-qm', 'notice')
            pin = git('rev-parse', 'HEAD')
            result = preflight.probe(root.as_uri(), pin, 'LICENSE')
            self.assertEqual(result['actual_commit'], pin)
            self.assertEqual(result['notice_blob'], git('rev-parse', 'HEAD:LICENSE'))
            with self.assertRaises(ValueError):
                preflight.probe(root.as_uri(), '0' * 40, 'LICENSE')
            with self.assertRaises(ValueError):
                preflight.probe(root.as_uri(), pin, 'MISSING')


if __name__ == '__main__':
    unittest.main()
