# Local licensed asset catalog

This is an offline catalog, not a hosted marketplace. It indexes licensed local GLB files and exports verified bytes, license evidence, hashes and a project recipe. Catalog owners are responsible for verifying their rights and attribution. The allowlist is a deliberately narrow policy, not proof that an asset is genuinely licensed.

From the repository root (new directories only):

    python3 Samples/AssetCatalog/create_demo_catalog.py /tmp/kairo-catalog
    python3 scripts/kairo_asset_catalog.py /tmp/kairo-catalog/catalog.json search blue
    python3 scripts/kairo_asset_catalog.py /tmp/kairo-catalog/catalog.json stage /tmp/kairo-gallery triangle
    PYTHONPATH=scripts python3 -m unittest discover -s scripts/tests -p test_kairo_asset_catalog.py

The staged recipe is compatible with the project-recipe importer candidate in #106. Once that dependency is available, use its documented CLI to create the project. Retain the staged license records and provenance when distributing the generated project: the recipe importer currently propagates asset metadata but not these separate license evidence files. The catalog validates the GLB container and checksum; the recipe importer performs its deeper self-contained GLB validation. No automatic downloading, executing assets, extracting archives, or hosted payment flow.
