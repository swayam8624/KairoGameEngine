# Project recipes: reusable game-world construction

Generate an ordinary Kairo project, not a new game executable:

    python3 scripts/create_kairo_project.py Samples/ProjectRecipes/TempleArena.json /tmp/KairoTemple
    build/game-clang/Runtime/KairoPlayer/KairoPlayer /tmp/KairoTemple/Project.kproject

The current template includes the existing controllable player, camera,
lighting, floor, input actions and arena obstacles. Set the optional JSON
field `"layout": "empty"` to keep the floor, boundary walls and playable
runtime setup but remove decorative arena obstacles. A recipe adds reusable
groups of primitive objects or self-contained glTF 2.0 GLB scene instances.
Every placement expands to independent scene entities with a common parent,
local transforms and stable deterministic numbering. Edit the resulting
ordinary project in the existing KairoEditor or author a new recipe.

Each prefab contains a list of named parts with either a builtin mesh
(cube, plane, sphere, cylinder) or an asset key. Optional position, yaw
(degrees), scale, and cube-only static box collider are supported. Instance
roots support position, yaw and uniform scale. Imported assets require
file (relative to the recipe), author, license and source. The generator
copies original GLB bytes, writes stable content-linked IDs, and records
SHA-256 and user-provided attribution in ASSET_LICENSES.json. It does not
verify that the user actually has redistribution permission. Only embedded
GLB is currently accepted; no network imports, OBJ, external glTF textures
or arbitrary URI dependencies. Runtime importer still owns mesh validation.

The output must not exist. The tool validates the complete recipe before
creating it; it never edits an existing project. This is a build-time
prefab expansion workflow, not a new linked runtime prefab system.
