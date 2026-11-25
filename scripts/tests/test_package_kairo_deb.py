"""Build a real .deb and launch its relocated unprivileged ELF fixture."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from package_kairo_deb import package


@unittest.skipUnless(shutil.which('dpkg-deb') and shutil.which('cc'), 'Needs Linux dpkg-deb and C compiler')
class DebTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='kairo package ')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.bundle = self.root / 'original bundle'
        (self.bundle / 'project').mkdir(parents=True)
        (self.bundle / 'bin').mkdir()
        descriptor = self.bundle / 'project/Project.kproject'
        descriptor.write_text('fixture\n')
        source = self.root / 'fixture.c'
        # Actual native executable consumes the descriptor and writes a save
        # beside it. No dependency on the original project/bundle paths.
        source.write_text(r'''#include <stdio.h>
#include <string.h>
int main(int argc,char **argv) {
    if(argc!=3||strcmp(argv[2],"--validate"))return 1;
    FILE *in=fopen(argv[1],"r"); if(!in)return 2;
    char text[16]; if(!fgets(text,sizeof(text),in))return 3; fclose(in);
    if(strcmp(text,"fixture\n"))return 4;
    char path[4096]; if(snprintf(path,sizeof(path),"%s.save",argv[1])>=(int)sizeof(path))return 5;
    FILE *out=fopen(path,"a"); if(!out)return 6; fputs("played\n",out); return fclose(out)!=0;
}''')
        subprocess.run(['cc', '-std=c17', '-Wall', '-Wextra', '-Werror', str(source), '-o', str(self.bundle / 'bin/KairoPlayer')], check=True)
        self.manifest = self.bundle / 'package.kmanifest'
        self.manifest.write_text(f'kairo-package 1\nproject "project/Project.kproject"\nprofile "Release"\nkind release\nengine-version "0.1.0"\nplayer "bin/KairoPlayer"\nproject-files 1\nproject-bytes {descriptor.stat().st_size}\n')

    def build(self, output):
        return package(self.bundle, output, 'kairo-fixture', '1.0.0', 'Kairo test fixture')

    def test_real_archive_and_relocation(self):
        output = self.root / 'fixture.deb'
        self.build(output)
        self.assertEqual(subprocess.check_output(['dpkg-deb', '-f', str(output), 'Package'], text=True).strip(), 'kairo-fixture')
        with self.assertRaises(ValueError): self.build(output)
        extracted = self.root / 'relocated tree'
        subprocess.run(['dpkg-deb', '-x', str(output), str(extracted)], check=True)
        shutil.rmtree(self.bundle)
        data = self.root / 'user data'
        env = dict(os.environ, XDG_DATA_HOME=str(data))
        command = [str(extracted / 'usr/bin/kairo-fixture'), '--validate']
        for _ in range(2): subprocess.run(command, cwd=self.root, env=env, check=True, timeout=10)
        self.assertEqual((data / 'kairo/kairo-fixture/1.0.0/project/Project.kproject.save').read_text(), 'played\nplayed\n')
        self.assertFalse((extracted / 'usr/lib/kairo/kairo-fixture/project/Project.kproject.save').exists())

    def test_rejections(self):
        output = self.root / 'bad.deb'
        original = self.manifest.read_text()
        self.manifest.write_text(original.replace('project-files 1', 'project-files 2'))
        with self.assertRaises(ValueError): self.build(output)
        self.manifest.write_text(original)
        (self.bundle / 'project/link').symlink_to(self.root)
        with self.assertRaises(ValueError): self.build(output)
        self.assertFalse(output.exists())
        (self.bundle / 'project/link').unlink()
        self.manifest.write_text(original.replace('bin/KairoPlayer', 'bin/../KairoPlayer'))
        with self.assertRaises(ValueError): self.build(output)
        self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
