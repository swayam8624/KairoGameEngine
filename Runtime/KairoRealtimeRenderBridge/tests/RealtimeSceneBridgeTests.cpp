#include <catch2/catch_test_macros.hpp>

#include <array>

import Kairo.Runtime.RealtimeSceneBridge;
import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Renderer;

using namespace kairo::runtime::renderbridge;

namespace
{
    const auto MeshID = kairo::assets::AssetID::Parse(
        "00000000-0000-4000-8000-000000000501");
    const auto MaterialID = kairo::assets::AssetID::Parse(
        "00000000-0000-4000-8000-000000000502");
    const auto SceneID = kairo::assets::AssetID::Parse(
        "00000000-0000-4000-8000-000000000503");
    const auto TextureID = kairo::assets::AssetID::Parse(
        "00000000-0000-4000-8000-000000000504");

    void RegisterAssets(kairo::assets::AssetRegistry& registry)
    {
        registry.Insert({ MeshID, kairo::assets::AssetType::Mesh,
            kairo::assets::AssetOrigin::Builtin, "builtin/cube",
            "kairo.builtin.cube", 1u, {} });
        registry.Insert({ MaterialID, kairo::assets::AssetType::Material,
            kairo::assets::AssetOrigin::Builtin, "builtin/material",
            "kairo.builtin.material", 1u, {} });
        registry.Insert({ SceneID, kairo::assets::AssetType::Scene,
            kairo::assets::AssetOrigin::SourceFile, "Scenes/model.glb",
            "kairo.gltf.scene", 1u, {} });
        registry.Insert({ TextureID, kairo::assets::AssetType::Texture2D,
            kairo::assets::AssetOrigin::SourceFile, "Textures/environment.hdr",
            "kairo.texture.stb", 1u, {} });
    }
}

TEST_CASE("Shared real-time extraction preserves authored render semantics",
    "[KairoRealtimeRenderBridge][Scene]")
{
    kairo::assets::AssetRegistry registry;
    RegisterAssets(registry);
    RenderAssetBindings assets(registry);
    assets.BindMesh({ MeshID }, 7u);
    assets.BindTexture({ TextureID }, 11u);
    kairo::renderer::PBRMaterial material;
    material.BaseColor = { 0.2f, 0.4f, 0.8f };
    material.Metallic = 0.65f;
    assets.BindMaterial({ MaterialID }, material);

    kairo::engine::Scene scene;
    const auto mesh = scene.CreateEntity("Mesh");
    scene.SetMeshRenderer(mesh, { { MeshID }, { MaterialID }, true });
    scene.MeshRenderer(mesh).RenderLayers = 0x1u;
    scene.MeshRenderer(mesh).CastShadows = false;
    scene.Transform(mesh).Local.Translation = { 2.0f, 0.0f, 0.0f };

    const auto lightEntity = scene.CreateEntity("Light");
    kairo::engine::LightComponent light;
    light.Type = kairo::engine::LightType::Point;
    light.Unit = kairo::engine::PhotometricUnit::Candela;
    light.Intensity = 500.0f;
    light.RenderLayers = 0x1u;
    scene.SetLight(lightEntity, light);

    const auto environmentEntity = scene.CreateEntity("World");
    kairo::engine::EnvironmentComponent environment;
    environment.BackgroundColor = { 0.1f, 0.2f, 0.3f };
    environment.AmbientIntensity = 0.25f;
    environment.EnvironmentIntensity = 2.0f;
    environment.EnvironmentTexture = kairo::assets::TextureAssetHandle{ TextureID };
    scene.SetEnvironment(environmentEntity, environment);

    const auto output = BuildRenderScene(scene, assets, 0x1u);
    REQUIRE(output.Draws().size() == 1u);
    CHECK(output.Draws()[0].ObjectID == mesh.Value);
    CHECK(output.Draws()[0].Material.Metallic == 0.65f);
    CHECK_FALSE(output.Draws()[0].CastShadows);
    REQUIRE(output.Lights().size() == 1u);
    CHECK(output.Lights()[0].Intensity == 5.0f);
    CHECK(output.Environment().AmbientIntensity == 0.5f);
    CHECK(output.Environment().EnvironmentTexture == 11u);
    REQUIRE_THROWS_AS(BuildRenderScene(scene, assets, 0u), std::invalid_argument);
}

TEST_CASE("Shared extraction expands scene instances and honors layer masks",
    "[KairoRealtimeRenderBridge][SceneInstance]")
{
    kairo::assets::AssetRegistry registry;
    RegisterAssets(registry);
    RenderAssetBindings assets(registry);
    auto local = kairo::foundation::math::Mat4f::Identity();
    local(1u, 3u) = 2.0f;
    assets.BindScene({ SceneID }, { { 41u, {}, local },
        { 42u, {}, kairo::foundation::math::Mat4f::Identity() } });

    kairo::engine::Scene scene;
    const auto entity = scene.CreateEntity("Imported");
    scene.SetSceneInstance(entity, { { SceneID }, true, false, true, 0x4u });
    scene.Transform(entity).Local.Translation = { 3.0f, 4.0f, 5.0f };

    const auto output = BuildRenderScene(scene, assets, 0x4u);
    REQUIRE(output.Draws().size() == 2u);
    CHECK(output.Draws()[0].Model(0u, 3u) == 3.0f);
    CHECK(output.Draws()[0].Model(1u, 3u) == 6.0f);
    CHECK(output.Draws()[0].ObjectID == entity.Value);
    CHECK(BuildRenderScene(scene, assets, 0x2u).Draws().empty());
}


namespace
{
    [[nodiscard]] kairo::assets::GltfSceneArtifactData AnimatedGltfScene()
    {
        using namespace kairo::assets;
        GltfSceneArtifactData source;

        GltfPrimitiveData staticPrimitive;
        staticPrimitive.Mesh.HasNormals = true;
        staticPrimitive.Mesh.Vertices = {
            { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, {} },
            { {  0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, {} },
            { {  0.0f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, {} }
        };
        staticPrimitive.Mesh.Indices = { 0u, 1u, 2u };
        source.Primitives.push_back(staticPrimitive);

        GltfPrimitiveData skinnedPrimitive = staticPrimitive;
        skinnedPrimitive.Skinning.resize(3u);
        for (auto& influence : skinnedPrimitive.Skinning)
        {
            influence.Joints = { 0u, 0u, 0u, 0u };
            influence.Weights = { 1.0f, 0.0f, 0.0f, 0.0f };
        }
        source.Primitives.push_back(skinnedPrimitive);

        GltfNodeData joint;
        joint.Name = "Joint";
        joint.HasRestTRS = true;
        source.Nodes.push_back(joint);

        GltfNodeData staticNode;
        staticNode.Name = "StaticAnimated";
        staticNode.Parent = 0;
        staticNode.LocalTransform[12] = 1.0f;
        staticNode.PrimitiveIndices = { 0u };
        staticNode.HasRestTRS = true;
        staticNode.RestTranslation = { 1.0f, 0.0f, 0.0f };
        source.Nodes.push_back(staticNode);

        GltfNodeData skinnedNode;
        skinnedNode.Name = "Skinned";
        skinnedNode.PrimitiveIndices = { 1u };
        skinnedNode.SkinIndex = 0u;
        skinnedNode.HasRestTRS = true;
        source.Nodes.push_back(skinnedNode);
        source.RootNodes = { 0u, 2u };

        GltfSkinData skin;
        skin.Name = "Skin";
        skin.SkeletonRoot = 0u;
        skin.Joints = { 0u };
        skin.InverseBindMatrices.push_back({
            1.0f,0.0f,0.0f,0.0f,
            0.0f,1.0f,0.0f,0.0f,
            0.0f,0.0f,1.0f,0.0f,
            0.0f,0.0f,0.0f,1.0f });
        source.Skins.push_back(skin);

        GltfAnimationChannelData channel;
        channel.TargetNode = 0u;
        channel.Path = GltfAnimationPath::Translation;
        channel.Interpolation = GltfAnimationInterpolation::Linear;
        GltfAnimationKeyframe begin;
        begin.TimeSeconds = 0.0f;
        begin.Value = { 0.0f, 0.0f, 0.0f, 0.0f };
        GltfAnimationKeyframe end;
        end.TimeSeconds = 1.0f;
        end.Value = { 2.0f, 0.0f, 0.0f, 0.0f };
        channel.Keyframes = { begin, end };
        GltfAnimationClipData clip;
        clip.Name = "MoveJoint";
        clip.Channels.push_back(channel);
        source.Animations.push_back(clip);
        ValidateGltfSceneArtifactData(source);
        return source;
    }
}

TEST_CASE("glTF scene extraction evaluates static nodes and skin palettes",
    "[KairoRealtimeRenderBridge][Animation][Skinning]")
{
    kairo::assets::AssetRegistry registry;
    RegisterAssets(registry);
    RenderAssetBindings assets(registry);
    auto source = AnimatedGltfScene();
    const auto renderAsset = kairo::renderer::MakeGltfRenderAsset(source);
    REQUIRE(renderAsset.Primitives.size() == 2u);
    const std::array<kairo::renderer::MeshHandle, 2u> meshes{ 41u, 42u };
    assets.BindGltfScene({ SceneID }, source, renderAsset, meshes);

    kairo::engine::Scene scene;
    const auto entity = scene.CreateEntity("AnimatedImported");
    scene.SetSceneInstance(entity, { { SceneID }, true, true, true, 0x4u });
    scene.Transform(entity).Local.Translation = { 3.0f, 0.0f, 0.0f };

    const auto rest = BuildRenderScene(scene, assets, 0x4u);
    REQUIRE(rest.Draws().size() == 2u);
    CHECK(rest.Draws()[0].Mesh == 41u);
    CHECK(rest.Draws()[0].Model(0u, 3u) == 4.0f);
    CHECK(rest.Draws()[0].Skinning.Empty());
    CHECK(rest.Draws()[1].Mesh == 42u);
    CHECK(rest.Draws()[1].Model(0u, 3u) == 3.0f);
    REQUIRE(rest.Draws()[1].Skinning.Size() == 1u);
    CHECK(rest.Draws()[1].Skinning.JointMatrices[0](0u, 3u) == 0.0f);

    SceneAnimationOverrides playback;
    playback.Set(entity, { 0u, 0.5f, kairo::engine::AnimationTimeMode::Clamp });
    const auto animated = BuildRenderScene(scene, assets, playback, 0x4u);
    REQUIRE(animated.Draws().size() == 2u);
    // Root joint moved +1; static child retains its authored +1 local offset,
    // then the outer scene instance contributes +3 => +5 world translation.
    CHECK(animated.Draws()[0].Model(0u, 3u) == 5.0f);
    // Skinned primitive receives only outer placement; deformation lives in the
    // asset-space joint palette and must not be applied again on Model.
    CHECK(animated.Draws()[1].Model(0u, 3u) == 3.0f);
    REQUIRE(animated.Draws()[1].Skinning.Size() == 1u);
    CHECK(animated.Draws()[1].Skinning.JointMatrices[0](0u, 3u) == 1.0f);
}

TEST_CASE("animation playback rejects generic static scene bindings",
    "[KairoRealtimeRenderBridge][Animation][Validation]")
{
    kairo::assets::AssetRegistry registry;
    RegisterAssets(registry);
    RenderAssetBindings assets(registry);
    assets.BindScene({ SceneID }, { { 41u, {},
        kairo::foundation::math::Mat4f::Identity() } });
    kairo::engine::Scene scene;
    const auto entity = scene.CreateEntity("Generic");
    scene.SetSceneInstance(entity, { { SceneID }, true, false, true, 0x1u });
    SceneAnimationOverrides playback;
    playback.Set(entity, { 0u, 0.25f, kairo::engine::AnimationTimeMode::Loop });
    REQUIRE_THROWS_AS(BuildRenderScene(scene, assets, playback, 0x1u),
        std::invalid_argument);
}
