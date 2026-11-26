#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <filesystem>
#include <system_error>
#include <unordered_set>

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

// TEACHING TEST: CPU-only test: bind fake renderer handles to typed AssetIDs; check
// stable ObjectID=Entity.Value, shadow flag, 500 candela -> 5 intensity units,
// ambient scaling, environment texture and empty-layer rejection. No native GPU
// upload is exercised.
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
    scene.SetMeshRenderer(mesh, { { MeshID }, { MaterialID }, true, {} });
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

// TEACHING TEST: One authored SceneInstance expands two primitive bindings. Parent
// entity +3,+4,+5 plus primitive local Y+2 produces world Y+6; both draws retain the
// single scene Entity pick ID. Another layer mask excludes them.
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

// TEACHING TEST: Compare rest pose to t=0.5 animated glTF: static child world X
// becomes 5 after scene+node+joint composition, while skinned draw Model remains
// outer X=3 and joint palette X becomes 1. This detects double-applying the node
// transform.
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

// TEACHING TEST: A prebound list of static primitives has no glTF node/clip source.
// Requesting playback is an error, not an implicit empty palette or fabricated
// animation.
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

// TEACHING TEST: Use an active primary orthographic camera; SelectRenderCamera must
// copy world translation and near/far/size/FOV parameters into backend-neutral
// CameraPose.
TEST_CASE("Camera extraction preserves authored projection parameters",
    "[KairoRealtimeRenderBridge][Camera]")
{
    kairo::engine::Scene scene;
    const auto cameraEntity = scene.CreateEntity("Main Camera");
    kairo::engine::CameraComponent camera;
    camera.Primary = true;
    camera.Projection = kairo::engine::CameraProjection::Orthographic;
    camera.VerticalFovRadians = 0.8f;
    camera.OrthographicSize = 28.0f;
    camera.NearPlane = 0.25f;
    camera.FarPlane = 2500.0f;
    scene.SetCamera(cameraEntity, camera);
    scene.Transform(cameraEntity).Local.Translation = { 4.0f, 6.0f, 8.0f };

    const auto extracted = SelectRenderCamera(scene);
    CHECK(extracted.Position == kairo::foundation::math::Vec3f{ 4.0f, 6.0f, 8.0f });
    CHECK(extracted.Projection == kairo::renderer::CameraProjectionMode::Orthographic);
    CHECK(extracted.VerticalFovRadians == camera.VerticalFovRadians);
    CHECK(extracted.OrthographicSize == camera.OrthographicSize);
    CHECK(extracted.NearPlane == camera.NearPlane);
    CHECK(extracted.FarPlane == camera.FarPlane);
}


TEST_CASE("Frame extraction preserves stable ordering and refreshes hierarchy after edits",
    "[KairoRealtimeRenderBridge][Hierarchy]")
{
    kairo::assets::AssetRegistry registry;
    RegisterAssets(registry);
    RenderAssetBindings assets(registry);
    assets.BindMesh({ MeshID }, 7u);
    assets.BindScene({ SceneID }, { { 41u, {}, kairo::foundation::math::Mat4f::Identity() } });

    kairo::engine::Scene scene;
    // Parents need not have lower IDs than their descendants.
    const auto imported = scene.CreateEntityWithID({ 9u }, "Imported");
    const auto mesh = scene.CreateEntityWithID({ 5u }, "Mesh");
    const auto parent = scene.CreateEntityWithID({ 80u }, "Parent");
    const auto light = scene.CreateEntityWithID({ 2u }, "Light");
    const auto hidden = scene.CreateEntityWithID({ 3u }, "Hidden");
    scene.SetParent(imported, parent);
    scene.SetParent(mesh, parent);
    scene.SetParent(light, mesh);
    scene.SetParent(hidden, parent);
    scene.Transform(parent).Local.Translation = { 10.0f, 2.0f, 1.0f };
    scene.Transform(parent).Local.Scale = { 2.0f, 3.0f, 4.0f };
    scene.Transform(mesh).Local.Translation = { 1.0f, 2.0f, 3.0f };
    scene.Transform(light).Local.Translation = { 3.0f, 1.0f, 0.0f };
    scene.SetMeshRenderer(mesh, { { MeshID }, { MaterialID }, true, {} });
    scene.SetMeshRenderer(hidden, { { MeshID }, { MaterialID }, false, {} });
    scene.SetSceneInstance(imported, { { SceneID }, true, true, true, 0x1u });
    kairo::engine::LightComponent point;
    point.Type = kairo::engine::LightType::Point;
    point.Unit = kairo::engine::PhotometricUnit::Candela;
    point.RenderLayers = 0x1u;
    scene.SetLight(light, point);

    const auto firstEnvironment = scene.CreateEntityWithID({ 30u }, "First world");
    const auto secondEnvironment = scene.CreateEntityWithID({ 40u }, "Second world");
    kairo::engine::EnvironmentComponent environment;
    environment.Priority = 5;
    environment.BackgroundColor = { 0.2f, 0.3f, 0.4f };
    scene.SetEnvironment(firstEnvironment, environment);
    environment.BackgroundColor = { 0.6f, 0.7f, 0.8f };
    scene.SetEnvironment(secondEnvironment, environment);

    const auto check = [&]
    {
        const auto out = BuildRenderScene(scene, assets, 0x1u);
        REQUIRE(out.Draws().size() == 2u);
        CHECK(out.Draws()[0].ObjectID == mesh.Value);
        CHECK(out.Draws()[1].ObjectID == imported.Value);
        CHECK(out.Draws()[0].Model ==
            kairo::foundation::math::ToMatrix4(scene.WorldTransform(mesh)));
        CHECK(out.Draws()[1].Model ==
            kairo::foundation::math::ToMatrix4(scene.WorldTransform(imported)));
        REQUIRE(out.Lights().size() == 1u);
        CHECK(out.Lights()[0].Position == scene.WorldTransform(light).Translation);
        CHECK(out.Environment().BackgroundColor ==
            scene.Environment(*scene.ActiveEnvironment()).BackgroundColor);
    };
    check();
    scene.Transform(parent).Local.Translation.x = -17.0f;
    check();
    scene.SetEnabled(parent, false);
    const auto disabled = BuildRenderScene(scene, assets, 0x1u);
    CHECK(disabled.Draws().empty());
    CHECK(disabled.Lights().empty());
    scene.SetEnabled(parent, true);
    scene.SetEnabled(firstEnvironment, false);
    scene.SetParent(mesh, std::nullopt);
    check();
}


TEST_CASE("Shared glTF rest snapshots isolate independent animated instances",
    "[KairoRealtimeRenderBridge][Animation][Skinning]")
{
    kairo::assets::AssetRegistry registry;
    RegisterAssets(registry);
    RenderAssetBindings assets(registry);
    auto source = AnimatedGltfScene();
    // Non-identity inverse bind exposes multiplication-order mistakes.
    source.Skins[0].InverseBindMatrices[0][12] = -2.0f;
    const auto renderAsset = kairo::renderer::MakeGltfRenderAsset(source);
    const std::array<kairo::renderer::MeshHandle, 2u> meshes{ 41u, 42u };
    assets.BindGltfScene({ SceneID }, source, renderAsset, meshes);
    const auto& binding = assets.ResolveSceneBinding({ SceneID });
    REQUIRE(binding.RestPose.has_value());
    REQUIRE(binding.RestPose->Palettes.size() == 1u);
    const auto restPose = kairo::engine::BuildGltfRestPose(source);
    const auto restReference = kairo::engine::BuildGltfAssetSpaceSkinPalette(source, restPose, 0u);
    CHECK(binding.RestPose->Palettes.at(0u).JointMatrices == restReference.JointMatrices);

    kairo::engine::Scene scene;
    const auto a = scene.CreateEntity("A");
    const auto b = scene.CreateEntity("B");
    const auto c = scene.CreateEntity("Rest");
    for (const auto entity : { a, b, c })
        scene.SetSceneInstance(entity, { { SceneID }, true, true, true, 0x4u });
    scene.Transform(a).Local.Translation.x = 3.0f;
    scene.Transform(b).Local.Translation.x = 7.0f;
    scene.Transform(c).Local.Translation.x = 11.0f;

    SceneAnimationOverrides playback;
    playback.Set(a, { 0u, 0.25f, kairo::engine::AnimationTimeMode::Clamp });
    playback.Set(b, { 0u, 0.75f, kairo::engine::AnimationTimeMode::Clamp });
    const auto output = BuildRenderScene(scene, assets, playback, 0x4u);
    REQUIRE(output.Draws().size() == 6u);
    for (std::size_t i = 0u; i < 3u; ++i)
    {
        const auto entity = std::array{ a, b, c }[i];
        const auto pose = i == 2u ? restPose :
            kairo::engine::SampleGltfAnimation(source, 0u, i == 0u ? 0.25f : 0.75f,
                kairo::engine::AnimationTimeMode::Clamp);
        const auto expectedWorld = kairo::engine::ResolveGltfWorldMatrices(source, pose);
        const auto expectedSkin = kairo::engine::BuildGltfAssetSpaceSkinPalette(source, pose, 0u);
        const auto outer = kairo::foundation::math::ToMatrix4(scene.WorldTransform(entity));
        const auto& rigid = output.Draws()[i * 2u];
        const auto& skinned = output.Draws()[i * 2u + 1u];
        CHECK(rigid.ObjectID == entity.Value);
        CHECK(skinned.ObjectID == entity.Value);
        CHECK(rigid.Model == outer * expectedWorld[1]);
        CHECK(skinned.Model == outer);
        CHECK(skinned.Skinning.JointMatrices == expectedSkin.JointMatrices);
    }

    // Subsequent rest-only extraction must not inherit the previous frame's animation.
    const auto reset = BuildRenderScene(scene, assets, 0x4u);
    REQUIRE(reset.Draws().size() == 6u);
    for (std::size_t i = 0u; i < 3u; ++i)
        CHECK(reset.Draws()[i * 2u + 1u].Skinning.JointMatrices == restReference.JointMatrices);
    CHECK(binding.RestPose->Palettes.at(0u).JointMatrices == restReference.JointMatrices);
}


TEST_CASE("Frame asset lookups refresh after registry removal and replacement",
    "[KairoRealtimeRenderBridge][Assets]")
{
    kairo::assets::AssetRegistry registry;
    RegisterAssets(registry);
    RenderAssetBindings assets(registry);
    assets.BindMesh({ MeshID }, 7u);
    kairo::engine::Scene scene;
    for (unsigned i = 0u; i < 64u; ++i)
    {
        const auto entity = scene.CreateEntity("Repeated mesh");
        scene.SetMeshRenderer(entity, { { MeshID }, { MaterialID }, true, {} });
        scene.Transform(entity).Local.Translation.x = static_cast<float>(i);
    }
    const auto first = BuildRenderScene(scene, assets);
    REQUIRE(first.Draws().size() == 64u);
    for (unsigned i = 0u; i < 64u; ++i)
    {
        CHECK(first.Draws()[i].Mesh == 7u);
        CHECK(first.Draws()[i].Model(0u, 3u) == static_cast<float>(i));
    }
    const auto metadata = registry.At(MeshID);
    registry.Remove(MeshID);
    REQUIRE_THROWS_AS(BuildRenderScene(scene, assets), std::out_of_range);
    registry.Insert(metadata);
    const auto restored = BuildRenderScene(scene, assets);
    REQUIRE(restored.Draws().size() == first.Draws().size());
    CHECK(restored.Draws()[63].ObjectID == first.Draws()[63].ObjectID);
}


// Exercise the actual glTF artifact -> sampled pose -> IK -> shared palette
// extraction, with two instances of the same immutable imported binding.
TEST_CASE("Imported two-bone IK corrects instance palettes without altering the shared rest or clip",
    "[KairoRealtimeRenderBridge][Animation][Skinning][IK]")
{
    using namespace kairo::assets;
    auto source=AnimatedGltfScene();
    GltfNodeData tip;tip.Name="Tip";tip.Parent=1;tip.HasRestTRS=true;
    tip.LocalTransform[12]=1.f;tip.RestTranslation={1.f,0.f,0.f};
    source.Nodes.push_back(tip);
    auto& skin=source.Skins[0];skin.Joints={0u,1u,3u};
    auto inverse=skin.InverseBindMatrices[0];inverse[12]=-1.f;skin.InverseBindMatrices.push_back(inverse);
    inverse[12]=-2.f;skin.InverseBindMatrices.push_back(inverse);
    source.Primitives[1].Skinning[1].Joints={1u,0u,0u,0u};
    source.Primitives[1].Skinning[2].Joints={2u,0u,0u,0u};
    ValidateGltfSceneArtifactData(source);
    kairo::assets::AssetRegistry registry;RegisterAssets(registry);
    RenderAssetBindings assets(registry);
    const auto renderAsset=kairo::renderer::MakeGltfRenderAsset(source);
    const std::array<kairo::renderer::MeshHandle,2> meshes{41u,42u};
    assets.BindGltfScene({SceneID},source,renderAsset,meshes);

    kairo::engine::Scene scene;
    const auto a=scene.CreateEntity("IK"),b=scene.CreateEntity("Clip only");
    for(const auto e:{a,b})scene.SetSceneInstance(e,{{SceneID},true,true,true,0x4u});
    scene.Transform(a).Local.Translation.x=3.f;scene.Transform(b).Local.Translation.x=7.f;
    SceneAnimationOverrides overrides;
    for(const auto e:{a,b})overrides.Set(e,{0u,.5f,kairo::engine::AnimationTimeMode::Clamp});
    const auto baseline=BuildRenderScene(scene,assets,overrides,0x4u);
    REQUIRE(baseline.Draws().size()==4u);
    const SceneTwoBoneIK request{{0u,1u,3u},{1.,2.,0.},{0.,0.,1.}};
    overrides.SetTwoBoneIK(a,SceneID,request);
    const auto corrected=BuildRenderScene(scene,assets,overrides,0x4u);
    REQUIRE(corrected.Draws().size()==4u);
    const auto& rigid=corrected.Draws()[0];const auto& skinned=corrected.Draws()[1];
    CHECK(std::abs(rigid.Model(0u,3u)-4.f)<1e-5f);
    CHECK(std::abs(rigid.Model(1u,3u)-1.f)<1e-5f);
    CHECK(skinned.Model==baseline.Draws()[1].Model);
    CHECK(skinned.ObjectID==a.Value);
    REQUIRE(skinned.Skinning.Size()==3u);
    const auto& palette=skinned.Skinning.JointMatrices[2];
    // The bind-space tip at x=2 transforms to asset-space target (1,2,0).
    CHECK(std::abs(2.f*palette(0u,0u)+palette(0u,3u)-1.f)<1e-5f);
    CHECK(std::abs(2.f*palette(1u,0u)+palette(1u,3u)-2.f)<1e-5f);
    CHECK(std::abs(2.f*palette(2u,0u)+palette(2u,3u))<1e-5f);
    CHECK(corrected.Draws()[3].Skinning.JointMatrices==baseline.Draws()[3].Skinning.JointMatrices);

    // Rest and clip snapshots must not share mutable procedural state.
    const auto sampled=kairo::engine::SampleGltfAnimation(source,0u,.5f,kairo::engine::AnimationTimeMode::Clamp);
    const auto saved=sampled;const auto [pose,reached]=ApplyGltfTwoBoneIK(source,sampled,request);
    CHECK(reached);CHECK(sampled==saved);CHECK(pose!=sampled);
    const auto expected=kairo::engine::BuildGltfAssetSpaceSkinPalette(source,pose,0u);
    CHECK(skinned.Skinning.JointMatrices==expected.JointMatrices);
    overrides.ClearTwoBoneIK(a);
    CHECK(BuildRenderScene(scene,assets,overrides,0x4u).Draws()[1].Skinning.JointMatrices==baseline.Draws()[1].Skinning.JointMatrices);
    overrides.SetTwoBoneIK(a,SceneID,request);
    overrides.ForgetMismatchedIK(a,MeshID);
    CHECK(overrides.FindTwoBoneIK(a,SceneID)==nullptr);
    overrides.SetTwoBoneIK(a,SceneID,request);
    overrides.RetainTwoBoneIK(std::unordered_set<std::uint32_t>{b.Value});
    CHECK(overrides.FindTwoBoneIK(a,SceneID)==nullptr);
    auto scaled=sampled;scaled.Nodes[1].LocalTRS.Scale.y=2.f;
    REQUIRE_THROWS_AS(ApplyGltfTwoBoneIK(source,scaled,request),std::invalid_argument);
    auto bad=request;bad.Nodes={0u,3u,1u};
    REQUIRE_THROWS_AS(ApplyGltfTwoBoneIK(source,sampled,bad),std::invalid_argument);
}

TEST_CASE("Authored animation clips persist without changing imported clips or render handles",
    "[KairoRealtimeRenderBridge][AuthoredClip]")
{
    using namespace kairo::assets;
    const auto root=std::filesystem::temp_directory_path()/("kairo-clip-"+GenerateAssetID().ToString());
    struct Cleanup {std::filesystem::path Path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(Path,ec);}} cleanup{root};
    std::filesystem::create_directories(root);
    AssetRegistry registry;
    RegisterAssets(registry);
    RenderAssetBindings assets(registry);
    const auto source=AnimatedGltfScene();
    const auto key=DerivedDataKey::Parse(std::string(64,'a'));
    const auto otherKey=DerivedDataKey::Parse(std::string(64,'b'));
    const auto renderAsset=kairo::renderer::MakeGltfRenderAsset(source);
    const std::array<kairo::renderer::MeshHandle,2> meshes{41u,42u};
    assets.BindGltfScene({SceneID},source,renderAsset,meshes,key);
    auto draft=source.Animations.front();
    draft.Name="Authored";
    draft.Channels.front().Keyframes.back().Value[0]=9.f;
    CHECK_FALSE(assets.HasAuthoredClip({SceneID}));
    const auto index=assets.CommitAuthoredClip(root,{SceneID},draft);
    CHECK(index==source.Animations.size());
    CHECK(assets.HasAuthoredClip({SceneID}));
    const auto* bound=assets.ResolveGltfSource({SceneID});
    REQUIRE(bound);
    CHECK(bound->Animations.front()==source.Animations.front());
    CHECK(bound->Animations.back()==draft);
    CHECK(assets.ResolveScene({SceneID}).front().Mesh==41u);
    auto loaded=source;
    const auto revision=ApplyAuthoredClip(root,SceneID,key,loaded);
    REQUIRE(revision);
    CHECK(loaded.Animations==bound->Animations);
    auto wrong=source;
    CHECK_THROWS(ApplyAuthoredClip(root,SceneID,otherKey,wrong));
    CHECK(wrong==source);
    auto invalid=draft;
    invalid.Channels.front().TargetNode=999999u;
    CHECK_THROWS(assets.CommitAuthoredClip(root,{SceneID},invalid));
    CHECK(ReadAuthoredClip(root,SceneID)==revision);
    auto changed=draft;
    changed.Channels.front().Keyframes.back().Value[0]=12.f;
    CHECK(assets.CommitAuthoredClip(root,{SceneID},changed)==index);
    CHECK(assets.ResolveGltfSource({SceneID})->Animations.size()==source.Animations.size()+1);
    CHECK(assets.ResolveGltfSource({SceneID})->Animations.back()==changed);
    // The older independently loaded session cannot overwrite the new file.
    RenderAssetBindings stale(registry);
    stale.BindGltfScene({SceneID},loaded,renderAsset,meshes,key,revision);
    CHECK_THROWS(stale.CommitAuthoredClip(root,{SceneID},draft));
    CHECK(stale.ResolveGltfSource({SceneID})->Animations.back()==draft);
    auto reopened=source;
    REQUIRE(ApplyAuthoredClip(root,SceneID,key,reopened));
    CHECK(reopened.Animations.back()==changed);
    const auto pose=kairo::engine::SampleGltfAnimation(reopened,index,
        changed.DurationSeconds(),kairo::engine::AnimationTimeMode::Clamp);
    CHECK(pose==kairo::engine::SampleGltfAnimation(*assets.ResolveGltfSource({SceneID}),index,
        changed.DurationSeconds(),kairo::engine::AnimationTimeMode::Clamp));
}
