# Linux game installer wrapper

First build the actual Linux KairoPlayer and generate a release bundle with its
existing packaging command:

    KairoPlayer PATH/Project.kproject --package Release

Then, on Debian/Ubuntu with Python 3 and dpkg-deb:

    python3 scripts/package_kairo_deb.py PATH/Build/Release /tmp/my-game.deb --name my-game --version 1.0.0 --maintainer "Your name"

Provide runtime dependency package names explicitly with repeated --depends
arguments; inspect the actual player and renderer dynamic-library requirements
before distributing. The wrapper checks the release manifest's project
file/byte counts, bounded regular files, ELF architecture and executable
permission. It refuses overwriting output, traversal and symbolic links.
The .deb contains no privileged maintainer scripts. The launcher makes a
version-specific writable project copy in XDG_DATA_HOME (or ~/.local/share)
on first use; later launches preserve it. Upgrades do not migrate saves yet.

Test without installing into the operating system:

    PYTHONPATH=scripts python3 -m unittest discover -s scripts/tests -p test_package_kairo_deb.py

The test builds and extracts an actual archive and runs a small compiled ELF
fixture twice from the relocated path. This verifies installer mechanics,
not a full GPU game on a clean OS. There is no automatic dependency discovery,
signing, real package-manager installation test, Windows installer, macOS
notarization, or mobile/web distribution in this pass.
