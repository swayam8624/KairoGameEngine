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

minimum = Vector((float("inf"), float("inf"), float("inf")))
maximum = Vector((float("-inf"), float("-inf"), float("-inf")))
mesh_count = 0

for obj in bpy.context.scene.objects:
    if obj.type != "MESH":
        continue
    mesh_count += 1
    for corner in obj.bound_box:
        world = obj.matrix_world @ Vector(corner)
        minimum.x = min(minimum.x, world.x)
        minimum.y = min(minimum.y, world.y)
        minimum.z = min(minimum.z, world.z)
        maximum.x = max(maximum.x, world.x)
        maximum.y = max(maximum.y, world.y)
        maximum.z = max(maximum.z, world.z)

if mesh_count == 0:
    raise RuntimeError(f"{source} contains no mesh objects")

bounds_path = destination.with_suffix(".bounds.txt")
bounds_path.write_text(
    "source=" + str(source) + "\n"
    + f"mesh_count={mesh_count}\n"
    + f"min={minimum.x:.9f},{minimum.y:.9f},{minimum.z:.9f}\n"
    + f"max={maximum.x:.9f},{maximum.y:.9f},{maximum.z:.9f}\n"
    + f"size={maximum.x-minimum.x:.9f},{maximum.y-minimum.y:.9f},{maximum.z-minimum.z:.9f}\n"
)

print(
    "[KAIRO] Bounds "
    f"min=({minimum.x:.3f}, {minimum.y:.3f}, {minimum.z:.3f}) "
    f"max=({maximum.x:.3f}, {maximum.y:.3f}, {maximum.z:.3f}) "
    f"meshes={mesh_count}"
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
print(f"[KAIRO] Bounds report {bounds_path}")
