"""Independent byte-level GLB fixture checks; no claim of native import."""
import json
import math
from pathlib import Path
import struct
import tempfile
import unittest
from run_imported_ik_native import build_glb, write_fixture


class FixtureTests(unittest.TestCase):
    def test_glb_and_skin_data(self):
        blob = build_glb()
        self.assertEqual(blob, build_glb())
        magic, version, total = struct.unpack_from('<4sII', blob)
        self.assertEqual((magic, version, total), (b'glTF', 2, len(blob)))
        size, kind = struct.unpack_from('<I4s', blob, 12)
        self.assertEqual(kind, b'JSON')
        self.assertEqual(size % 4, 0)
        doc = json.loads(blob[20:20 + size])
        n, kind = struct.unpack_from('<I4s', blob, 20 + size)
        self.assertEqual(kind, b'BIN\0')
        binary = blob[28 + size:]
        self.assertEqual(n, len(binary))
        self.assertEqual(len(binary), doc['buffers'][0]['byteLength'])
        self.assertNotIn('uri', doc['buffers'][0])
        widths = {'SCALAR': 1, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}
        formats = {5126: 'f', 5123: 'H'}
        decoded = []
        previous = 0
        for acc in doc['accessors']:
            view = doc['bufferViews'][acc['bufferView']]
            offset = view['byteOffset']
            self.assertEqual(offset % 4, 0)
            self.assertGreaterEqual(offset, previous)
            fmt = '<' + formats[acc['componentType']] * (acc['count'] * widths[acc['type']])
            self.assertEqual(struct.calcsize(fmt), view['byteLength'])
            previous = offset + view['byteLength']
            self.assertLessEqual(previous, len(binary))
            decoded.append(struct.unpack_from(fmt, binary, offset))
        skin = doc['skins'][0]
        self.assertEqual(skin['joints'], [0, 1, 2])
        for j in range(3):
            m = decoded[skin['inverseBindMatrices']][16*j:16*j+16]
            self.assertEqual(m, (1,0,0,0,0,1,0,0,0,0,1,0,-j,0,0,1))
        primitive = doc['meshes'][0]['primitives'][0]
        attributes = primitive['attributes']
        pos, joints, weights = (decoded[attributes[k]] for k in ('POSITION','JOINTS_0','WEIGHTS_0'))
        self.assertEqual(len(pos), 18)
        for vertex in range(6):
            self.assertEqual(joints[4*vertex:4*vertex+4], (vertex//2,0,0,0))
            self.assertEqual(weights[4*vertex:4*vertex+4], (1,0,0,0))
        indices = decoded[primitive['indices']]
        for i in range(0, len(indices), 3):
            a,b,c = [pos[3*j:3*j+3] for j in indices[i:i+3]]
            self.assertGreater((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]), 0)
        clip = doc['animations'][0]
        self.assertEqual(decoded[clip['samplers'][0]['input']], (0,1))
        self.assertEqual(decoded[clip['samplers'][0]['output']], (0,0,0,.5,0,0))
        self.assertTrue(all(math.isfinite(v) for group in decoded for v in group))

    def test_exclusive_write(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = write_fixture(Path(tmp))
            self.assertEqual(path.read_bytes(), build_glb())
            with self.assertRaises(FileExistsError):
                write_fixture(Path(tmp))
            self.assertEqual(path.read_bytes(), build_glb())


if __name__ == '__main__':
    unittest.main()
