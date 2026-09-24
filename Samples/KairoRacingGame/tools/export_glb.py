import bpy
import sys
from pathlib import Path
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
if len(args) != 2:
    raise SystemExit("usage: export_glb.py <source.glb> <destination.glb>")

source = Path(args[0]).resolve()
destination = Path(args[1]).resolve()
destination.parent.mkdir(parents=True, exist_ok=True)

print(f"[KAIRO] Importing exact upstream runtime asset {source}")
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(source))

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
    raise RuntimeError(f"Runtime asset contains no mesh objects: {source}")

# Blender evaluates imported glTF in its native Z-up coordinate system.
# KAIRO and glTF use right-handed Y-up. Convert the world AABB back into
# KAIRO/glTF coordinates before validating gameplay positions:
#   Kx = Bx, Ky = Bz, Kz = -By.
kairo_minimum = Vector((minimum.x, minimum.z, -maximum.y))
kairo_maximum = Vector((maximum.x, maximum.z, -minimum.y))

bounds_path = destination.with_suffix(".bounds.txt")
bounds_path.write_text(
    f"mesh_count {mesh_count}\n"
    f"min {kairo_minimum.x:.9g} {kairo_minimum.y:.9g} {kairo_minimum.z:.9g}\n"
    f"max {kairo_maximum.x:.9g} {kairo_maximum.y:.9g} {kairo_maximum.z:.9g}\n"
)
print(
    f"[KAIRO] KAIRO-space bounds meshes={mesh_count} "
    f"min=({kairo_minimum.x:.4f},{kairo_minimum.y:.4f},{kairo_minimum.z:.4f}) "
    f"max=({kairo_maximum.x:.4f},{kairo_maximum.y:.4f},{kairo_maximum.z:.4f})"
)

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
