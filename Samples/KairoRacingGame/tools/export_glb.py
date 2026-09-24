import bpy
import sys
from pathlib import Path

args = sys.argv[sys.argv.index("--") + 1:]
if len(args) != 2:
    raise SystemExit("usage: export_glb.py <source.glb> <destination.glb>")

source = Path(args[0]).resolve()
destination = Path(args[1]).resolve()
destination.parent.mkdir(parents=True, exist_ok=True)

print(f"[KAIRO] Importing exact upstream runtime asset {source}")
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(source))

print(f"[KAIRO] Re-exporting uncompressed runtime asset {destination}")
bpy.ops.export_scene.gltf(
    filepath=str(destination),
    export_format="GLB",
    export_apply=False,
    export_draco_mesh_compression_enable=False,
)

if not destination.is_file():
    raise RuntimeError(f"GLB export did not produce {destination}")

print(f"[KAIRO] Exported {destination} ({destination.stat().st_size} bytes)")
