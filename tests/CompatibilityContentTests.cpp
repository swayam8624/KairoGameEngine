#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

import Kairo.Assets;

namespace
{
    [[nodiscard]] std::filesystem::path Content(std::string_view relative)
    {
        return std::filesystem::path(KAIRO_COMPATIBILITY_CONTENT_DIR) / relative;
    }

    [[nodiscard]] std::vector<std::byte> ReadBytes(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) throw std::runtime_error("Cannot open compatibility fixture: " + path.string());
        const std::streampos end = input.tellg();
        if (end < 0) throw std::runtime_error("Cannot size compatibility fixture: " + path.string());
        std::vector<std::byte> result(static_cast<std::size_t>(end));
        input.seekg(0);
        if (!result.empty() && !input.read(reinterpret_cast<char*>(result.data()),
            static_cast<std::streamsize>(result.size())))
            throw std::runtime_error("Cannot read compatibility fixture: " + path.string());
        return result;
    }

    [[nodiscard]] kairo::assets::GltfSceneArtifactData ImportScene(
        std::string_view relative)
    {
        using namespace kairo::assets;
        const std::filesystem::path path = Content(relative);
        const std::vector<std::byte> bytes = ReadBytes(path);
        GltfSceneImporter importer;
        return ParseGltfSceneDerivedArtifact(importer.Import({
            {}, AssetType::Scene, bytes, path }));
    }
}

TEST_CASE("Reviewed Khronos fixtures import as deterministic static scenes",
    "[CompatibilityContent][Assets]")
{
    const auto spheres = ImportScene(
        "committed/MetalRoughSpheresNoTextures/MetalRoughSpheresNoTextures.glb");
    CHECK_FALSE(spheres.Primitives.empty());
    CHECK_FALSE(spheres.Materials.empty());
    CHECK_FALSE(spheres.Nodes.empty());

    const auto car = ImportScene("committed/ToyCar/ToyCar.glb");
    CHECK(car.Primitives.size() > 1u);
    CHECK(car.Materials.size() > 1u);
    CHECK(car.Nodes.size() > 1u);
}

TEST_CASE("Reviewed Poly Haven HDR fixture imports as finite RGBA16F",
    "[CompatibilityContent][Assets]")
{
    using namespace kairo::assets;
    const std::filesystem::path path = Content(
        "committed/KloppenheimSky/kloppenheim_06_puresky_1k.hdr");
    const std::vector<std::byte> bytes = ReadBytes(path);
    StbTextureImporter importer;
    ImportRecord record;
    record.CanonicalSettings = CanonicalTextureImportSettings({
        TextureColorSpace::Linear, false, true, 4096u });
    const TextureArtifactData texture = ParseTextureDerivedArtifact(importer.Import({
        record, AssetType::Texture2D, bytes, path }));
    CHECK(texture.Format == TexturePixelFormat::RGBA16Float);
    CHECK(texture.ColorSpace == TextureColorSpace::Linear);
    CHECK(texture.Semantic == TextureSemantic::HDR);
    CHECK(texture.Mips.back().Width == 1u);
    CHECK(texture.Mips.back().Height == 1u);
}

TEST_CASE("Known extension fixture never fails without a controlled diagnostic",
    "[CompatibilityContent][Assets][Diagnostics]")
{
    try
    {
        const auto scene = ImportScene("committed/LightVisibility/LightVisibility.glb");
        CHECK_FALSE(scene.Nodes.empty());
    }
    catch (const std::exception& error)
    {
        CHECK_FALSE(std::string(error.what()).empty());
    }
}

TEST_CASE("Malformed glTF fixture produces a stable located decoder failure",
    "[CompatibilityContent][Assets][Diagnostics]")
{
    using namespace kairo::assets;
    const std::filesystem::path path = Content(
        "committed/Malformed/invalid_json.gltf");
    GltfSceneImporter importer;
    try
    {
        (void)importer.Import({ {}, AssetType::Scene, ReadBytes(path), path });
        FAIL("Malformed glTF unexpectedly imported successfully.");
    }
    catch (const std::invalid_argument& error)
    {
        CHECK(std::string(error.what()).find("invalid JSON") != std::string::npos);
    }
}
