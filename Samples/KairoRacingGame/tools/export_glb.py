import bpy
import sys
from pathlib import Path
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
if len(args) != 2:
    raise SystemExit("usage: export_glb.py <source.blend> <destination.glb>")

source = Path(args[0]).resolve()
destination = Path(args[1]).resolve()
destination.parent.mkdir(parents=True, exist_ok=True)

print(f"[KAIRO] Loading {source}")
bpy.ops.wm.open_mainfile(filepath=str(source))
mesh_objects = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
if not mesh_objects:
    raise RuntimeError(f"No mesh objects exist in {source}")

minimum = Vector((float("inf"), float("inf"), float("inf")))
maximum = Vector((float("-inf"), float("-inf"), float("-inf")))
for obj in mesh_objects:
    for corner in obj.bound_box:
        world = obj.matrix_world @ Vector(corner)
        minimum.x = min(minimum.x, world.x)
        minimum.y = min(minimum.y, world.y)
        minimum.z = min(minimum.z, world.z)
        maximum.x = max(maximum.x, world.x)
        maximum.y = max(maximum.y, world.y)
        maximum.z = max(maximum.z, world.z)

print(
    "[KAIRO_BOUNDS] "
    f"{source.name} "
    f"min=({minimum.x:.6f},{minimum.y:.6f},{minimum.z:.6f}) "
    f"max=({maximum.x:.6f},{maximum.y:.6f},{maximum.z:.6f}) "
    f"meshes={len(mesh_objects)}"
)

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
