import bpy
import sys
from pathlib import Path

args = sys.argv[sys.argv.index("--") + 1:]
if len(args) != 2:
    raise SystemExit("usage: export_glb.py <source.blend> <destination.glb>")

source = Path(args[0]).resolve()
destination = Path(args[1]).resolve()
destination.parent.mkdir(parents=True, exist_ok=True)

print(f"[KAIRO] Loading {source}")
bpy.ops.wm.open_mainfile(filepath=str(source))
print(f"[KAIRO] Exporting {destination}")
bpy.ops.export_scene.gltf(
    filepath=str(destination),
    export_format="GLB",
    export_apply=True,
    export_draco_mesh_compression_enable=False,
)
if not destination.is_file():
    raise RuntimeError(f"GLB export did not produce {destination}")
print(f"[KAIRO] Exported {destination} ({destination.stat().st_size} bytes)")
