import hashlib
import json
from pathlib import Path
import shutil
import stat
import tempfile
import unittest
import zipfile
import kairo_release as release


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        tmp=tempfile.TemporaryDirectory(); self.addCleanup(tmp.cleanup)
        self.root=Path(tmp.name); self.bundle=self.root/'bundle'
        (self.bundle/'bin').mkdir(parents=True); (self.bundle/'project').mkdir()
        # Real host ELF, not a KairoPlayer build. Archive tooling never executes it.
        shutil.copyfile('/bin/true',self.bundle/'bin/KairoPlayer')
        (self.bundle/'run.sh').write_text('#!/bin/sh\nexit 0\n')
        (self.bundle/'project/Project.kproject').write_text('fixture\n')
        (self.bundle/'project/asset.bin').write_bytes(bytes(range(256))*100)
        self.manifest()

    def manifest(self):
        files=[p for p in (self.bundle/'project').rglob('*') if p.is_file()]
        (self.bundle/'package.kmanifest').write_text('kairo-package 1\nproject "project/Project.kproject"\nprofile "Release"\nkind release\nengine-version "0.1.0"\nplayer "bin/KairoPlayer"\n'+f'project-files {len(files)}\nproject-bytes {sum(p.stat().st_size for p in files)}\n')

    def test_roundtrip_determinism_and_no_overwrite(self):
        a,b=self.root/'a.zip',self.root/'b.zip'
        index=release.export(self.bundle,a)
        self.assertEqual(index,release.verify(a))
        release.export(self.bundle,b)
        self.assertEqual(hashlib.sha256(a.read_bytes()).digest(),hashlib.sha256(b.read_bytes()).digest())
        with self.assertRaises(ValueError):release.export(self.bundle,a)
        self.assertTrue(index['target'].startswith('linux-'))

    def test_detect_modified_asset(self):
        a,b=self.root/'a.zip',self.root/'b.zip';release.export(self.bundle,a)
        with zipfile.ZipFile(a) as src,zipfile.ZipFile(b,'w') as dst:
            for info in src.infolist():
                data=src.read(info)
                if info.filename=='project/asset.bin':data=b'X'+data[1:]
                dst.writestr(info,data)
        with self.assertRaises(ValueError):release.verify(b)

    def test_unsafe_paths(self):
        for name in ('../escape','project/CON.txt','project/AUX','/root','project/a\\b','project/a.','project/a:stream'):
            with self.subTest(name=name), self.assertRaises(ValueError):release.safe_name(name)
        a=self.root/'a.zip'
        with zipfile.ZipFile(a,'w') as dst:dst.writestr(release.zip_info('../escape'),b'x')
        with self.assertRaises(ValueError):release.verify(a)

    def test_symlink_and_case_collision(self):
        link=self.bundle/'project/link';link.symlink_to(self.root)
        with self.assertRaises(ValueError):release.export(self.bundle,self.root/'a.zip')
        link.unlink();(self.bundle/'project/ASSET.bin').write_bytes(b'x');self.manifest()
        with self.assertRaises(ValueError):release.export(self.bundle,self.root/'a.zip')

    def test_manifest_count_mismatch(self):
        (self.bundle/'project/asset.bin').write_bytes(b'changed')
        with self.assertRaises(ValueError):release.export(self.bundle,self.root/'a.zip')

    def test_transient_data_excluded(self):
        p=self.bundle/'project/.kairo/progress';p.mkdir(parents=True);(p/'save').write_bytes(b'private');self.manifest()
        with self.assertRaises(ValueError):release.export(self.bundle,self.root/'a.zip')

    def test_file_directory_conflict(self):
        a=self.root/'a.zip'
        with zipfile.ZipFile(a,'w') as dst:
            dst.writestr(release.zip_info('project/a'),b'a');dst.writestr(release.zip_info('project/A/b'),b'b')
        with self.assertRaises(ValueError):release.verify(a)

    def test_unpack_verified_archive(self):
        archive=self.root/'release.zip';target=self.root/'relocated game'
        index=release.export(self.bundle,archive)
        self.assertEqual(release.unpack(archive,target),index)
        for name,record in index['files'].items():
            self.assertEqual(hashlib.sha256((target/name).read_bytes()).hexdigest(),record['sha256'])
        self.assertEqual(stat.S_IMODE((target/'run.sh').stat().st_mode),0o755)
        with self.assertRaises(ValueError):release.unpack(archive,target)
        self.assertTrue((target/'project/asset.bin').is_file())

    def test_custom_native_executable_name(self):
        player=self.bundle/'bin/KairoPlayer'
        player.rename(self.bundle/'bin/KairoPhase1Game')
        manifest=self.bundle/'package.kmanifest'
        manifest.write_text(manifest.read_text().replace('bin/KairoPlayer','bin/KairoPhase1Game'))
        archive=self.root/'phase1.zip'
        index=release.export(self.bundle,archive)
        self.assertIn('bin/KairoPhase1Game',index['files'])
        self.assertEqual(release.verify(archive),index)
        for unsafe in ('bin/bad$name','bin/../game','bin/CON.exe','bin/game%PATH%','bin/a/b'):
            with self.subTest(name=unsafe), self.assertRaises(ValueError):
                release.manifest(manifest.read_bytes().replace(b'bin/KairoPhase1Game',unsafe.encode()))

    def test_executable_header(self):
        import io,struct
        header=bytearray(64);header[:4]=b'\xcf\xfa\xed\xfe';struct.pack_into('<III',header,4,0x100000c,0,2)
        self.assertEqual(release.platform_of(io.BytesIO(header)),'macos-arm64')
        pe=bytearray(90);pe[:2]=b'MZ';struct.pack_into('<I',pe,60,64);pe[64:68]=b'PE\0\0';struct.pack_into('<H',pe,68,0x8664);pe[88:90]=b'\x0b\x02'
        self.assertEqual(release.platform_of(io.BytesIO(pe)),'windows-x86_64')
        with self.assertRaises(ValueError):release.platform_of(io.BytesIO(bytes(64)))


if __name__=='__main__':unittest.main()
