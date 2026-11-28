import json
from pathlib import Path
import struct
import tempfile
import unittest

from windows_vulkan_creator_smoke import driver_manifest


def pe(machine=0x8664, dll=True):
    data = bytearray(128)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 64)
    data[64:68] = b'PE\0\0'
    struct.pack_into('<H', data, 68, machine)
    struct.pack_into('<H', data, 86, 0x2000 if dll else 2)
    struct.pack_into('<H', data, 88, 0x20b)
    return data


class Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.manifest = self.root / 'vk_swiftshader_icd.json'
        self.library = self.root / 'vk_swiftshader.dll'
        self.library.write_bytes(pe())
        self.data = {'file_format_version': '1.0.0', 'ICD': {'library_path': './vk_swiftshader.dll', 'api_version': '1.3.0'}}

    def write(self):
        self.manifest.write_text(json.dumps(self.data))

    def test_expected_manifest(self):
        for value in ('vk_swiftshader.dll', './vk_swiftshader.dll', '.\\vk_swiftshader.dll'):
            self.data['ICD']['library_path'] = value
            self.write()
            self.assertEqual(driver_manifest(self.manifest)['api_version'], '1.3.0')

    def test_invalid_manifest_or_binary(self):
        for value in ('../vk_swiftshader.dll', 'C:/other/vk_swiftshader.dll', 'unknown.dll'):
            self.data['ICD']['library_path'] = value
            self.write()
            with self.assertRaises(ValueError):
                driver_manifest(self.manifest)
        self.data['ICD']['library_path'] = 'vk_swiftshader.dll'
        self.write()
        for data in (pe(0xaa64), pe(dll=False), b'bad'):
            self.library.write_bytes(data)
            with self.assertRaises(ValueError):
                driver_manifest(self.manifest)


if __name__ == '__main__':
    unittest.main()
