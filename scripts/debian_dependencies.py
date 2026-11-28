"""Resolve Debian package dependencies from a trusted, built ELF executable."""
import re
import subprocess
import tempfile
from pathlib import Path

# No alternatives or qualifiers are currently supported. Reject rather than
# silently weakening a version constraint or accepting a control-field injection.
DEPENDENCY = re.compile(r'[a-z0-9][a-z0-9+.-]{1,63}(?: \((?:>=|<=|=|<<|>>) [0-9][A-Za-z0-9.+:~\-]{0,127}\))?\Z')


def valid_dependency(value):
    return isinstance(value, str) and DEPENDENCY.fullmatch(value) is not None


def parse_shlibdeps(output):
    lines = output.splitlines()
    if len(lines) != 1 or not lines[0].startswith('shlibs:Depends='):
        raise ValueError('Unexpected dpkg-shlibdeps output')
    value = lines[0].removeprefix('shlibs:Depends=')
    dependencies = value.split(', ') if value else []
    if not dependencies or not all(map(valid_dependency, dependencies)):
        raise ValueError('Missing or unsupported dependency relation')
    if len(dependencies) != len(set(dependencies)):
        raise ValueError('Duplicate dependency relation')
    return dependencies


def resolve(binary):
    binary = Path(binary).resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix='kairo-shlibdeps-') as temp:
        root = Path(temp)
        (root / 'debian').mkdir()
        (root / 'debian/control').write_text(
            'Source: kairo-signal-vault\nSection: games\nPriority: optional\n'
            'Maintainer: Kairo <fixture@example.invalid>\n\n'
            'Package: kairo-signal-vault\nArchitecture: any\nDescription: Signal Vault\n',
            encoding='ascii')
        # No --ignore-missing-info: libraries without package metadata block
        # this gate. dpkg-shlibdeps supplies actual version constraints.
        result = subprocess.run(['dpkg-shlibdeps', '-O', '-e' + str(binary)],
                                cwd=root, check=True, capture_output=True, text=True, timeout=60)
        return parse_shlibdeps(result.stdout)
