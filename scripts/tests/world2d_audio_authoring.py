#!/usr/bin/env python3
"""Real WAV import and project validation using the native tool and Player."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import wave

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from kairo_audio2d_editor import import_wave,add_wave,native,LIMIT
from kairo_world2d_editor import publish_config
TOOL=Path(sys.argv.pop(1)).resolve(strict=True)
PLAYER=Path(sys.argv.pop(1)).resolve(strict=True)
SAMPLE=Path(sys.argv.pop(1)).resolve(strict=True)


class AudioTests(unittest.TestCase):
    def setUp(self):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup)
        self.root=Path(temp.name)/"Project";shutil.copytree(SAMPLE,self.root)
        self.target=self.root/"Config/Audio.kaudio";self.baseline=self.target.read_bytes()
        self.source=Path(temp.name)/"original.wav"
        with wave.open(str(self.source),"wb") as stream:
            stream.setnchannels(2);stream.setsampwidth(2);stream.setframerate(24000)
            stream.writeframes(b"\x00\x20\x00\xf0"*2400)

    def test_real_wave_import_and_package(self):
        doc=import_wave(self.source,self.root,"music_track",self.baseline.decode("ascii"),TOOL)
        asset=self.root/"Assets/Audio/music_track.wav"
        self.assertEqual(asset.read_bytes(),self.source.read_bytes())
        with self.assertRaises(ValueError):import_wave(self.source,self.root,"music_track",doc,TOOL)
        doc=doc.replace("music - 70","music music_track 70").encode("ascii")
        publish_config(self.target,self.baseline,doc,LIMIT,[TOOL,"--validate",self.root])
        self.assertEqual(self.target.read_bytes(),doc)
        subprocess.run([str(PLAYER),str(self.root/"Project.kproject"),"--validate"],check=True,capture_output=True,timeout=30)
        subprocess.run([str(PLAYER),str(self.root/"Project.kproject"),"--package","Release"],check=True,capture_output=True,timeout=60)
        # Validate copied source independently of original asset directory.
        bundle=self.root/"Build/Release/project"
        self.assertEqual((bundle/"Assets/Audio/music_track.wav").read_bytes(),self.source.read_bytes())
        asset.unlink()
        subprocess.run([str(PLAYER),str(bundle/"Project.kproject"),"--validate"],check=True,capture_output=True,timeout=30)

    def test_reject_bad_wave_and_configuration(self):
        self.source.write_bytes(b"not a WAV")
        with self.assertRaises(ValueError):import_wave(self.source,self.root,"broken",self.baseline.decode("ascii"),TOOL)
        self.assertFalse((self.root/"Assets/Audio/broken.wav").exists())
        self.assertEqual(self.target.read_bytes(),self.baseline)
        missing=add_wave(self.baseline.decode("ascii"),"missing").encode("ascii")
        with self.assertRaises(ValueError):publish_config(self.target,self.baseline,missing,LIMIT,[TOOL,"--validate",self.root])
        bad=self.baseline.replace(b"main 8 hum",b"main 999 hum")
        with self.assertRaises(ValueError):publish_config(self.target,self.baseline,bad,LIMIT,[TOOL,"--validate",self.root])
        self.assertEqual(self.target.read_bytes(),self.baseline)
        for name in ("../other","con","bad.name","Capital"):
            with self.assertRaises(ValueError):add_wave(self.baseline.decode("ascii"),name)


if __name__=="__main__":unittest.main()
