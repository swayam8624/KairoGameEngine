#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <string_view>

import Kairo.Assets;
import Kairo.Editor.SceneRenderBridge;
import Kairo.Player.RuntimeProject;
import Kairo.Renderer;
import Kairo.Runtime.RealtimeSceneBridge;

namespace
{
    class TemporaryDirectory final
    {
        std::filesystem::path m_Path;

    public:
        TemporaryDirectory()
            : m_Path(std::filesystem::temp_directory_path() /
                ("kairo-shared-contract-" + std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count())))
        {
            std::filesystem::create_directories(m_Path);
        }

        ~TemporaryDirectory()
        {
            std::error_code ignored;
            std::filesystem::remove_all(m_Path, ignored);
        }

        [[nodiscard]] const std::filesystem::path& Path() const noexcept
        {
            return m_Path;
        }
    };

    void CompareMaterial(const kairo::renderer::PBRMaterial& left,
        const kairo::renderer::PBRMaterial& right)
    {
        CHECK(left.BaseColor == right.BaseColor);
        CHECK(left.BaseColorAlpha == right.BaseColorAlpha);
        CHECK(left.Metallic == right.Metallic);
        CHECK(left.Roughness == right.Roughness);
        CHECK(left.Emissive == right.Emissive);
        CHECK(left.AmbientOcclusion == right.AmbientOcclusion);
        CHECK(left.NormalScale == right.NormalScale);
        CHECK(left.AlphaMode == right.AlphaMode);
        CHECK(left.AlphaCutoff == right.AlphaCutoff);
        CHECK(left.DoubleSided == right.DoubleSided);
        CHECK(left.BaseColorTexture == right.BaseColorTexture);
        CHECK(left.NormalTexture == right.NormalTexture);
        CHECK(left.MetallicRoughnessTexture == right.MetallicRoughnessTexture);
        CHECK(left.EmissiveTexture == right.EmissiveTexture);
        CHECK(left.OcclusionTexture == right.OcclusionTexture);
    }
}

TEST_CASE("Serialized showcase extracts identically for Editor and Player",
    "[Integration][Assets][EngineCore][Renderer][Editor][Player]")
{
    const kairo::player::RuntimeProject project(KAIRO_SHARED_CONTENT_PROJECT);
    const auto& registry = project.Assets();
    kairo::runtime::renderbridge::RenderAssetBindings runtimeBindings(registry);
    kairo::editor::RenderAssetBindings editorBindings(registry);

    kairo::renderer::MeshHandle nextMesh = 1u;
    kairo::renderer::TextureHandle nextTexture = 100u;
    for (const auto& asset : registry.Snapshot())
    {
        if (asset.Type == kairo::assets::AssetType::Texture2D)
        {
            runtimeBindings.BindTexture({ asset.ID }, nextTexture);
            editorBindings.BindTexture({ asset.ID }, nextTexture);
            ++nextTexture;
        }
        if (asset.Type == kairo::assets::AssetType::Mesh)
        {
            REQUIRE(kairo::runtime::renderbridge::MakeBuiltinRenderMesh(asset)
                .has_value());
            runtimeBindings.BindMesh({ asset.ID }, nextMesh);
            editorBindings.BindMesh({ asset.ID }, nextMesh);
            ++nextMesh;
        }
    }

    TemporaryDirectory cacheRoot;
    kairo::assets::DerivedDataCache cache(cacheRoot.Path());
    kairo::assets::ImportDatabase imports;
    std::size_t importedScenes = 0u;
    for (const auto& asset : registry.Snapshot())
    {
        if (asset.Type != kairo::assets::AssetType::Scene ||
            asset.Origin != kairo::assets::AssetOrigin::SourceFile) continue;
        const auto resolveTexture = [&](std::string_view uri,
            kairo::assets::TextureSemantic)
        {
            const auto path = (asset.Path.parent_path() /
                std::filesystem::path(uri)).lexically_normal();
            const auto texture = registry.FindByPath(path);
            REQUIRE(texture.has_value());
            return runtimeBindings.ResolveTexture({ texture->ID });
        };
        const auto imported =
            kairo::runtime::renderbridge::ImportRenderGltfScene(
                project.Root(), { asset.ID }, registry, imports, cache,
                resolveTexture);
        std::vector<kairo::runtime::renderbridge::RenderAssetBindings::ScenePrimitive>
            primitives;
        for (const auto& primitive : imported.Primitives)
            primitives.push_back({ nextMesh++, primitive.Material,
                primitive.LocalToAsset });
        runtimeBindings.BindScene({ asset.ID }, primitives);
        editorBindings.BindScene({ asset.ID }, std::move(primitives));
        ++importedScenes;
    }
    REQUIRE(importedScenes == 2u);

    const auto runtime = kairo::runtime::renderbridge::BuildRenderScene(
        project.Scene(), runtimeBindings);
    const auto editor = kairo::editor::BuildRenderScene(
        project.Scene(), editorBindings);
    REQUIRE(runtime.Draws().size() == editor.Draws().size());
    REQUIRE(runtime.Draws().size() >= 8u);
    for (std::size_t index = 0u; index < runtime.Draws().size(); ++index)
    {
        const auto& left = runtime.Draws()[index];
        const auto& right = editor.Draws()[index];
        CHECK(left.Mesh == right.Mesh);
        CHECK(left.Model == right.Model);
        CHECK(left.ObjectID == right.ObjectID);
        CHECK(left.CastShadows == right.CastShadows);
        CHECK(left.ReceiveShadows == right.ReceiveShadows);
        CompareMaterial(left.Material, right.Material);
    }

    REQUIRE(runtime.Lights().size() == editor.Lights().size());
    REQUIRE(runtime.Lights().size() == 2u);
    for (std::size_t index = 0u; index < runtime.Lights().size(); ++index)
    {
        const auto& left = runtime.Lights()[index];
        const auto& right = editor.Lights()[index];
        CHECK(left.Type == right.Type);
        CHECK(left.Position == right.Position);
        CHECK(left.Direction == right.Direction);
        CHECK(left.Color == right.Color);
        CHECK(left.Intensity == right.Intensity);
        CHECK(left.CastShadows == right.CastShadows);
    }
    CHECK(runtime.Environment().BackgroundColor ==
        editor.Environment().BackgroundColor);
    CHECK(runtime.Environment().AmbientColor ==
        editor.Environment().AmbientColor);
    CHECK(runtime.Environment().ExposureEV100 ==
        editor.Environment().ExposureEV100);

    const auto runtimeCamera =
        kairo::runtime::renderbridge::SelectRenderCamera(project.Scene());
    const auto editorCamera = kairo::editor::SelectRenderCamera(project.Scene());
    CHECK(runtimeCamera.Position == editorCamera.Position);
    CHECK(runtimeCamera.Target == editorCamera.Target);
    CHECK(runtimeCamera.Up == editorCamera.Up);
}
