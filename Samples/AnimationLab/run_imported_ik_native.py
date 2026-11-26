#!/usr/bin/env python3
"""Generate an original skinned GLB and exercise the native import/IK fixture."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile


def build_glb():
    """Three rigid joints deform a connected two-segment ribbon.

    Positions are bind-space coordinates; JOINTS_0 indexes skin.joints,
    not the scene nodes. Inverse binds translate each joint back to origin.
    A clip moves the root by +0.5 X over one second. All data is embedded.
    """
    binary = bytearray()
    views, accessors = [], []

    def accessor(values, fmt, kind, count, component, target=None, bounds=None):
        # GLB buffer views are 4-byte aligned, including ushort index data.
        binary.extend(b'\0' * (-len(binary) % 4))
        offset = len(binary)
        binary.extend(struct.pack('<' + fmt * len(values), *values))
        view = {'buffer': 0, 'byteOffset': offset, 'byteLength': len(binary) - offset}
        if target is not None:
            view['target'] = target
        views.append(view)
        acc = {'bufferView': len(views) - 1, 'componentType': component, 'count': count, 'type': kind}
        if bounds is not None:
            acc['min'], acc['max'] = bounds
        accessors.append(acc)
        return len(accessors) - 1

    positions = [v for x in range(3) for y in (-.2, .2) for v in (x, y, 0)]
    pos = accessor(positions, 'f', 'VEC3', 6, 5126, 34962, ([0, -.2, 0], [2, .2, 0]))
    normals = accessor([0, 0, 1] * 6, 'f', 'VEC3', 6, 5126, 34962)
    joints = accessor([v for j in range(3) for _ in range(2) for v in (j, 0, 0, 0)], 'H', 'VEC4', 6, 5123, 34962)
    weights = accessor([1, 0, 0, 0] * 6, 'f', 'VEC4', 6, 5126, 34962)
    indices = accessor([0, 2, 1, 1, 2, 3, 2, 4, 3, 3, 4, 5], 'H', 'SCALAR', 12, 5123, 34963)
    matrices = [v for j in range(3) for v in (1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -j, 0, 0, 1)]
    binds = accessor(matrices, 'f', 'MAT4', 3, 5126)
    times = accessor([0, 1], 'f', 'SCALAR', 2, 5126, bounds=([0], [1]))
    translations = accessor([0, 0, 0, .5, 0, 0], 'f', 'VEC3', 2, 5126)
    doc = {
        'asset': {'version': '2.0', 'generator': 'Kairo original IK ribbon fixture'},
        'scene': 0, 'scenes': [{'nodes': [0, 3]}],
        'nodes': [{'name': 'Root', 'children': [1]},
                  {'name': 'Middle', 'translation': [1, 0, 0], 'children': [2]},
                  {'name': 'Tip', 'translation': [1, 0, 0]},
                  {'name': 'Ribbon', 'mesh': 0, 'skin': 0}],
        'skins': [{'name': 'RibbonRig', 'skeleton': 0, 'joints': [0, 1, 2], 'inverseBindMatrices': binds}],
        'materials': [{'name': 'RibbonBlue', 'doubleSided': True,
                       'pbrMetallicRoughness': {'baseColorFactor': [.15, .65, .95, 1], 'metallicFactor': 0, 'roughnessFactor': .6}}],
        'meshes': [{'primitives': [{'attributes': {'POSITION': pos, 'NORMAL': normals, 'JOINTS_0': joints, 'WEIGHTS_0': weights},
                                    'indices': indices, 'material': 0}]}],
        'animations': [{'name': 'RootMove', 'samplers': [{'input': times, 'output': translations, 'interpolation': 'LINEAR'}],
                        'channels': [{'sampler': 0, 'target': {'node': 0, 'path': 'translation'}}]}],
        'buffers': [{'byteLength': len(binary)}], 'bufferViews': views, 'accessors': accessors,
    }
    encoded = json.dumps(doc, separators=(',', ':'), allow_nan=False).encode('utf-8')
    encoded += b' ' * (-len(encoded) % 4)
    binary.extend(b'\0' * (-len(binary) % 4))
    total = 12 + 8 + len(encoded) + 8 + len(binary)
    return struct.pack('<4sII', b'glTF', 2, total) + struct.pack('<I4s', len(encoded), b'JSON') + encoded + struct.pack('<I4s', len(binary), b'BIN\0') + binary


def write_fixture(root):
    # Exclusive creation prevents accidental overwrite of an authored model.
    path = root / 'Character.glb'
    with path.open('xb') as stream:
        stream.write(build_glb())
    return path


def run(binary, backend):
    with tempfile.TemporaryDirectory(prefix='kairo imported ik ') as temp:
        root = Path(temp)
        write_fixture(root)
        subprocess.run([str(binary.resolve(strict=True)), str(root), backend], cwd=root, check=True, timeout=65)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('backend', choices=('metal', 'vulkan'))
    args = parser.parse_args()
    run(args.binary, args.backend)
