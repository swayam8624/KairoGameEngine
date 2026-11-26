"""Exercise release verification with a real ELF fixture and Debian archive."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from verify_project_release import inventory, source_files, verify, verify_provenance


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='kairo release test ')
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.project=self.root/'original project';self.project.mkdir()
        self.descriptor=self.project/'Project.kproject'
        self.descriptor.write_text('kairo-project 2\nbuild-profile "Development" development "Build/Development"\nbuild-profile "Release" release "Build/Release"\n')
        for path,data in {'.kairo/compiled/logic.bin':b'compiled', '.kairo/cache/temp':b'cache',
                          'Build/Release/stale':b'old', '.git/HEAD':b'git', 'Content/Imported/model.glb':b'fixture-model',
                          'Licenses/model.txt':b'fixture-license'}.items():
            file=self.project/path;file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(data)
        record={'path':'Content/Imported/model.glb','sha256':hashlib.sha256(b'fixture-model').hexdigest(),
                'license_file':'Licenses/model.txt','license_sha256':hashlib.sha256(b'fixture-license').hexdigest()}
        (self.project/'ASSET_LICENSES.json').write_text(json.dumps([record]))

    def fixture_player(self):
        # Real ELF for the installed launch; a tiny Python packager emulates
        # only the external package contract. This is not the Kairo engine.
        source=self.root/'fixture.c';player=self.root/'KairoPlayer'
        source.write_text(r'''#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
int main(int argc,char **argv) {
    if(argc==4&&!strcmp(argv[2],"--package")) {
        const char *script=getenv("KAIRO_FIXTURE_PACKAGER");
        if(!script)return 1;
        execlp("python3","python3",script,argv[0],argv[1],(char*)0);return 2;
    }
    if(!((argc==3&&!strcmp(argv[2],"--validate"))||(argc==5&&!strcmp(argv[2],"--renderer")&&!strcmp(argv[3],"vulkan")&&!strcmp(argv[4],"--smoke"))))return 3;
    char path[4096];if(strlen(argv[1])>=sizeof(path))return 4;strcpy(path,argv[1]);
    char *slash=strrchr(path,'/');if(!slash)return 5;*slash=0;
    if(chdir(path))return 6;
    const char *files[]={"Project.kproject",".kairo/compiled/logic.bin","Licenses/model.txt","Content/Imported/model.glb"};
    for(unsigned i=0;i<4;i++){FILE *f=fopen(files[i],"rb");if(!f)return 7;fclose(f);}
    if(argc==5) {
        mkdir(".kairo",0755);
        mkdir(".kairo/cache",0755);
        FILE *cache=fopen(".kairo/cache/runtime.bin","wb");
        if(!cache)return 8;
        fputs("generated cache",cache); if(fclose(cache))return 9;
        if(getenv("KAIRO_FIXTURE_MUTATE_AUTHORED")) {
            FILE *bad=fopen("Licenses/model.txt","ab");
            if(!bad)return 10;
            fputs("mutated",bad);if(fclose(bad))return 11;
        }
    }
    return 0;
}''')
        subprocess.run(['cc','-Wall','-Wextra','-Werror',str(source),'-o',str(player)],check=True)
        script=self.root/'packager.py';script.write_text('''import os,sys,shutil
from pathlib import Path
player,descriptor=map(Path,sys.argv[1:]);source=descriptor.parent
out=source/'Build/Release';out.mkdir(parents=True)
shutil.copytree(source,out/'project',ignore=shutil.ignore_patterns('Build'))
if os.getenv('KAIRO_FIXTURE_TAMPER')=='1': (out/'project/Licenses/model.txt').write_text('tamper-license')
if os.getenv('KAIRO_FIXTURE_TAMPER')=='2': (out/'project/extra').write_text('extra')
files=[p for p in (out/'project').rglob('*') if p.is_file()]
(out/'bin').mkdir();shutil.copy2(player,out/'bin/KairoPlayer')
(out/'package.kmanifest').write_text(f'kairo-package 1\\nproject "project/Project.kproject"\\nprofile "Release"\\nkind release\\nengine-version "0.1.0"\\nplayer "bin/KairoPlayer"\\nproject-files {len(files)}\\nproject-bytes {sum(p.stat().st_size for p in files)}\\n')
launcher=out/'run.sh';launcher.write_text('#!/bin/sh\\nset -eu\\nroot=$(dirname "$0")\\nexec "$root/bin/KairoPlayer" "$root/project/Project.kproject" "$@"\\n');launcher.chmod(0o755)
''')
        return player,script

    def test_source_policy(self):
        names={p.as_posix() for p in source_files(self.project)}
        self.assertEqual(names,{'Project.kproject','.kairo/compiled/logic.bin','Content/Imported/model.glb','Licenses/model.txt','ASSET_LICENSES.json'})
        (self.project/'link').symlink_to(self.root)
        with self.assertRaisesRegex(ValueError,'Unsafe source directory'):source_files(self.project)

    def test_provenance_rejects_tamper(self):
        self.assertEqual(verify_provenance(self.project,inventory(self.project)),1)
        record=json.loads((self.project/'ASSET_LICENSES.json').read_text())[0]
        original=dict(record);del record['license_file'];del record['license_sha256']
        (self.project/'ASSET_LICENSES.json').write_text(json.dumps([record]))
        with self.assertRaisesRegex(ValueError,'missing license evidence'):verify_provenance(self.project,inventory(self.project),True)
        (self.project/'ASSET_LICENSES.json').write_text(json.dumps([original]))
        (self.project/'Licenses/model.txt').write_bytes(b'tampered')
        with self.assertRaisesRegex(ValueError,'License evidence mismatch'):verify_provenance(self.project,inventory(self.project))

    @unittest.skipUnless(shutil.which('cc') and shutil.which('dpkg-deb'),'Requires native Linux tools')
    def test_relocated_elf_and_deb(self):
        player,script=self.fixture_player()
        with patch.dict(os.environ,{'KAIRO_FIXTURE_PACKAGER':str(script),'KAIRO_FIXTURE_TAMPER':'0'}):
            result=verify(player,self.descriptor,smoke=True,deb=True,catalog_models=1)
        self.assertEqual(result['files'],5);self.assertEqual(result['licensed_models'],1);self.assertTrue(result['debian'])
        self.assertTrue((self.project/'Build/Release/stale').exists())

    @unittest.skipUnless(shutil.which('cc'),'Requires native C compiler')
    def test_launch_rejects_authored_mutation(self):
        player,script=self.fixture_player()
        with patch.dict(os.environ,{'KAIRO_FIXTURE_PACKAGER':str(script),'KAIRO_FIXTURE_TAMPER':'0',
                                    'KAIRO_FIXTURE_MUTATE_AUTHORED':'1'}):
            with self.assertRaisesRegex(ValueError,'Relocated launch changed authored payload'):
                verify(player,self.descriptor,smoke=True)

    @unittest.skipUnless(shutil.which('cc'),'Requires native C compiler')
    def test_package_rejects_modified_or_extra_bytes(self):
        player,script=self.fixture_player()
        for mode in ('1','2'):
            with patch.dict(os.environ,{'KAIRO_FIXTURE_PACKAGER':str(script),'KAIRO_FIXTURE_TAMPER':mode}):
                with self.assertRaisesRegex(ValueError,'Relocated release payload differs'):verify(player,self.descriptor)


if __name__=='__main__':
    unittest.main()
