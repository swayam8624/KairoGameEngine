# Local licensed asset catalog

This is an offline catalog, not a hosted marketplace. It indexes licensed local GLB files and exports verified bytes, license evidence, hashes and a project recipe. Catalog owners are responsible for verifying their rights and attribution. The allowlist is a deliberately narrow policy, not proof that an asset is genuinely licensed.

From the repository root (new directories only):

    python3 Samples/AssetCatalog/create_demo_catalog.py /tmp/kairo-catalog
    python3 scripts/kairo_asset_catalog.py /tmp/kairo-catalog/catalog.json search blue
    python3 scripts/kairo_asset_catalog.py /tmp/kairo-catalog/catalog.json stage /tmp/kairo-gallery triangle
    PYTHONPATH=scripts python3 -m unittest discover -s scripts/tests -p test_kairo_asset_catalog.py

Generate an ordinary authored project from the staged recipe:

    python3 scripts/create_kairo_project.py /tmp/kairo-gallery/recipe.json /tmp/kairo-gallery-project
    KairoPlayer /tmp/kairo-gallery-project/Project.kproject --validate
    KairoPlayer /tmp/kairo-gallery-project/Project.kproject --renderer vulkan

The project contains the exact original GLB and license bytes, with SHA256
records in ASSET_LICENSES.json. Staging can be deleted after successful
generation. The importer re-verifies model and license hashes and rejects
symlink/path traversal. Native acceptance remains a separate gate. The
catalog validates the GLB container and checksum; the recipe importer
performs deeper self-contained GLB validation. No automatic downloading,
executing assets, extracting archives or hosted payment flow.
