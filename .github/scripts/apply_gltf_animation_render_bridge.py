from pathlib import Path

p = Path('Runtime/KairoRealtimeRenderBridge/RealtimeSceneBridge.cppm')
t = p.read_text()

def rep(old, new, count=1):
    global t
    if t.count(old) < count:
        raise SystemExit(f'missing target: {old[:120]!r}')
    t = t.replace(old, new, count)

rep('#include <cstddef>\n', '#include <algorithm>\n#include <cmath>\n#include <cstddef>\n')
rep('#include <optional>\n', '#include <optional>\n#include <span>\n')

old_import = '''    /// Task: import one hierarchy-preserving glTF/GLB scene and convert its
    /// portable primitives/materials through KairoRenderer's canonical adapter.
    [[nodiscard]] inline kairo::renderer::GltfRenderAsset ImportRenderGltfScene(
        const std::filesystem::path& projectRoot,
        kairo::assets::SceneAssetHandle asset,
        const kairo::assets::AssetRegistry& registry,
        kairo::assets::ImportDatabase& imports,
        const kairo::assets::DerivedDataCache& cache,
        const kairo::renderer::GltfTextureResolver& resolveTexture = {})
    {
        const auto metadata = registry.Resolve(asset);
        kairo::assets::GltfSceneImporter importer;
        if (metadata.Origin != kairo::assets::AssetOrigin::SourceFile ||
            metadata.Importer != importer.Identifier())
            throw std::invalid_argument(
                "Render scene import requires a source glTF asset using kairo.gltf.scene.");
        kairo::assets::ImportRecord record{ metadata.ID, metadata.Path,
            importer.Identifier(), importer.Version(), {}, {}, 1u };
        auto outcome = kairo::assets::ImportSourceAsset(projectRoot, std::move(record),
            importer, registry, imports, cache);
        return kairo::renderer::MakeGltfRenderAsset(
            kairo::assets::ParseGltfSceneDerivedArtifact(outcome.Artifact), resolveTexture);
    }
'''
new_import = '''    /// Complete CPU-side glTF import needed by animation-aware runtime binding.
    /// Keep the validated source artifact beside the renderer adaptation so node,
    /// skin, rest-pose, and clip metadata are not discarded after mesh upload.
    struct RenderGltfSceneImport final
    {
        kairo::assets::GltfSceneArtifactData Source;
        kairo::renderer::GltfRenderAsset RenderAsset;
        kairo::assets::DerivedDataKey CacheKey;
        bool CacheHit = false;
    };

    [[nodiscard]] inline RenderGltfSceneImport ImportRenderGltfSceneWithSource(
        const std::filesystem::path& projectRoot,
        kairo::assets::SceneAssetHandle asset,
        const kairo::assets::AssetRegistry& registry,
        kairo::assets::ImportDatabase& imports,
        const kairo::assets::DerivedDataCache& cache,
        const kairo::renderer::GltfTextureResolver& resolveTexture = {})
    {
        const auto metadata = registry.Resolve(asset);
        kairo::assets::GltfSceneImporter importer;
        if (metadata.Origin != kairo::assets::AssetOrigin::SourceFile ||
            metadata.Importer != importer.Identifier())
            throw std::invalid_argument(
                "Render scene import requires a source glTF asset using kairo.gltf.scene.");
        kairo::assets::ImportRecord record{ metadata.ID, metadata.Path,
            importer.Identifier(), importer.Version(), {}, {}, 1u };
        auto outcome = kairo::assets::ImportSourceAsset(projectRoot, std::move(record),
            importer, registry, imports, cache);
        auto source = kairo::assets::ParseGltfSceneDerivedArtifact(outcome.Artifact);
        auto renderAsset = kairo::renderer::MakeGltfRenderAsset(source, resolveTexture);
        return { std::move(source), std::move(renderAsset), outcome.Key,
            outcome.CacheHit };
    }

    /// Compatibility projection for callers that only need static render data.
    [[nodiscard]] inline kairo::renderer::GltfRenderAsset ImportRenderGltfScene(
        const std::filesystem::path& projectRoot,
        kairo::assets::SceneAssetHandle asset,
        const kairo::assets::AssetRegistry& registry,
        kairo::assets::ImportDatabase& imports,
        const kairo::assets::DerivedDataCache& cache,
        const kairo::renderer::GltfTextureResolver& resolveTexture = {})
    {
        return ImportRenderGltfSceneWithSource(projectRoot, asset, registry,
            imports, cache, resolveTexture).RenderAsset;
    }
'''
rep(old_import, new_import)

rep('''        struct ScenePrimitive final
        {
            kairo::renderer::MeshHandle Mesh = kairo::renderer::InvalidMeshHandle;
            kairo::renderer::PBRMaterial Material;
            kairo::foundation::math::Mat4f LocalToAsset =
                kairo::foundation::math::Mat4f::Identity();
        };
''', '''        struct ScenePrimitive final
        {
            kairo::renderer::MeshHandle Mesh = kairo::renderer::InvalidMeshHandle;
            kairo::renderer::PBRMaterial Material;
            kairo::foundation::math::Mat4f LocalToAsset =
                kairo::foundation::math::Mat4f::Identity();
            std::uint32_t NodeIndex = kairo::assets::GltfMissingIndex;
            std::uint32_t PrimitiveIndex = kairo::assets::GltfMissingIndex;
            std::uint32_t SkinIndex = kairo::assets::GltfMissingIndex;
        };

        struct SceneBinding final
        {
            std::vector<ScenePrimitive> Primitives;
            std::optional<kairo::assets::GltfSceneArtifactData> GltfSource;
        };
''')

old_scene_methods = '''        void BindScene(kairo::assets::SceneAssetHandle asset,
            std::vector<ScenePrimitive> primitives)
        {
            (void)m_Registry.Resolve(asset);
            if (primitives.empty())
                throw std::invalid_argument("A render scene binding requires primitives.");
            for (const auto& primitive : primitives)
            {
                if (primitive.Mesh == kairo::renderer::InvalidMeshHandle)
                    throw std::invalid_argument(
                        "A render scene primitive requires a valid mesh handle.");
                primitive.Material.Validate();
            }
            if (!m_Scenes.emplace(asset.ID, std::move(primitives)).second)
                throw std::invalid_argument("A render scene asset is already bound.");
        }

        [[nodiscard]] const std::vector<ScenePrimitive>& ResolveScene(
            kairo::assets::SceneAssetHandle asset) const
        {
            (void)m_Registry.Resolve(asset);
            const auto found = m_Scenes.find(asset.ID);
            if (found == m_Scenes.end())
                throw std::out_of_range(
                    "No renderer scene is bound for asset ID: " + asset.ID.ToString());
            return found->second;
        }
'''
new_scene_methods = '''        void BindScene(kairo::assets::SceneAssetHandle asset,
            std::vector<ScenePrimitive> primitives)
        {
            (void)m_Registry.Resolve(asset);
            ValidateScenePrimitives(primitives);
            if (!m_Scenes.emplace(asset.ID,
                SceneBinding{ std::move(primitives), std::nullopt }).second)
                throw std::invalid_argument("A render scene asset is already bound.");
        }

        /// Bind the renderer handles produced from one exact glTF source while
        /// retaining the node/skin identities needed by animation extraction.
        void BindGltfScene(kairo::assets::SceneAssetHandle asset,
            kairo::assets::GltfSceneArtifactData source,
            const kairo::renderer::GltfRenderAsset& renderAsset,
            std::span<const kairo::renderer::MeshHandle> meshHandles)
        {
            (void)m_Registry.Resolve(asset);
            kairo::assets::ValidateGltfSceneArtifactData(source);
            if (renderAsset.Primitives.size() != meshHandles.size())
                throw std::invalid_argument(
                    "glTF scene binding requires one GPU mesh handle per render primitive.");
            std::vector<ScenePrimitive> primitives;
            primitives.reserve(renderAsset.Primitives.size());
            for (std::size_t index = 0u; index < renderAsset.Primitives.size(); ++index)
            {
                const auto& render = renderAsset.Primitives[index];
                if (render.NodeIndex >= source.Nodes.size() ||
                    render.PrimitiveIndex >= source.Primitives.size())
                    throw std::invalid_argument(
                        "glTF render primitive references source metadata outside the bound artifact.");
                const auto& node = source.Nodes[render.NodeIndex];
                if (std::ranges::find(node.PrimitiveIndices, render.PrimitiveIndex) ==
                    node.PrimitiveIndices.end() || node.SkinIndex != render.SkinIndex)
                    throw std::invalid_argument(
                        "glTF render primitive metadata does not match the bound source artifact.");
                primitives.push_back({ meshHandles[index], render.Material,
                    render.LocalToAsset, render.NodeIndex, render.PrimitiveIndex,
                    render.SkinIndex });
            }
            ValidateScenePrimitives(primitives);
            if (!m_Scenes.emplace(asset.ID,
                SceneBinding{ std::move(primitives), std::move(source) }).second)
                throw std::invalid_argument("A render scene asset is already bound.");
        }

        [[nodiscard]] const std::vector<ScenePrimitive>& ResolveScene(
            kairo::assets::SceneAssetHandle asset) const
        {
            return ResolveSceneBinding(asset).Primitives;
        }

        [[nodiscard]] const kairo::assets::GltfSceneArtifactData* ResolveGltfSource(
            kairo::assets::SceneAssetHandle asset) const
        {
            const auto& binding = ResolveSceneBinding(asset);
            return binding.GltfSource.has_value() ? &*binding.GltfSource : nullptr;
        }
'''
rep(old_scene_methods, new_scene_methods)

rep('''        std::unordered_map<kairo::assets::AssetID, std::vector<ScenePrimitive>,
            kairo::assets::AssetIDHash> m_Scenes;
    };
''', '''        std::unordered_map<kairo::assets::AssetID, SceneBinding,
            kairo::assets::AssetIDHash> m_Scenes;

        static void ValidateScenePrimitives(
            std::span<const ScenePrimitive> primitives)
        {
            if (primitives.empty())
                throw std::invalid_argument("A render scene binding requires primitives.");
            for (const auto& primitive : primitives)
            {
                if (primitive.Mesh == kairo::renderer::InvalidMeshHandle)
                    throw std::invalid_argument(
                        "A render scene primitive requires a valid mesh handle.");
                primitive.Material.Validate();
            }
        }

        [[nodiscard]] const SceneBinding& ResolveSceneBinding(
            kairo::assets::SceneAssetHandle asset) const
        {
            (void)m_Registry.Resolve(asset);
            const auto found = m_Scenes.find(asset.ID);
            if (found == m_Scenes.end())
                throw std::out_of_range(
                    "No renderer scene is bound for asset ID: " + asset.ID.ToString());
            return found->second;
        }
    };

    struct SceneAnimationPlayback final
    {
        std::uint32_t ClipIndex = 0u;
        float TimeSeconds = 0.0f;
        kairo::engine::AnimationTimeMode TimeMode =
            kairo::engine::AnimationTimeMode::Loop;

        void Validate() const
        {
            if (!std::isfinite(TimeSeconds))
                throw std::invalid_argument(
                    "Scene animation playback time must be finite.");
        }
    };

    /// Per-instance playback overrides. Animation state is intentionally kept
    /// outside serialized ECS in this first runtime slice so Editor/Player can
    /// drive preview/playback without changing the Scene schema prematurely.
    class SceneAnimationOverrides final
    {
        std::unordered_map<std::uint32_t, SceneAnimationPlayback> m_Playback;

    public:
        void Set(kairo::engine::Entity entity, SceneAnimationPlayback playback)
        {
            playback.Validate();
            m_Playback.insert_or_assign(entity.Value, playback);
        }

        void Clear(kairo::engine::Entity entity) noexcept
        {
            m_Playback.erase(entity.Value);
        }

        [[nodiscard]] const SceneAnimationPlayback* Find(
            kairo::engine::Entity entity) const noexcept
        {
            const auto found = m_Playback.find(entity.Value);
            return found == m_Playback.end() ? nullptr : &found->second;
        }

        [[nodiscard]] bool Empty() const noexcept { return m_Playback.empty(); }
    };
''')

old_build = '''    [[nodiscard]] inline kairo::renderer::RenderScene BuildRenderScene(
        const kairo::engine::Scene& scene,
        const RenderAssetBindings& assets,
        std::uint64_t renderLayers = kairo::engine::AllRenderLayers)
    {
        if (renderLayers == 0u)
            throw std::invalid_argument("Render extraction requires a non-empty layer mask.");
        kairo::renderer::RenderScene result;
        for (const kairo::engine::Entity entity : scene.RenderableEntities())
        {
            const auto& source = scene.MeshRenderer(entity);
            if ((source.RenderLayers & renderLayers) == 0u) continue;
            result.Add({ .Mesh = assets.ResolveMesh(source.MeshAsset),
                .Model = kairo::foundation::math::ToMatrix4(scene.WorldTransform(entity)),
                .Material = assets.ResolveMaterial(source.MaterialForSlot(0u)),
                .ObjectID = entity.Value,
                .CastShadows = source.CastShadows,
                .ReceiveShadows = source.ReceiveShadows });
        }
        for (const kairo::engine::Entity entity : scene.SceneInstanceEntities())
        {
            const auto& source = scene.SceneInstance(entity);
            if ((source.RenderLayers & renderLayers) == 0u) continue;
            const auto world = kairo::foundation::math::ToMatrix4(scene.WorldTransform(entity));
            for (const auto& primitive : assets.ResolveScene(source.SceneAsset))
                result.Add({ .Mesh = primitive.Mesh,
                    .Model = world * primitive.LocalToAsset,
                    .Material = primitive.Material,
                    .ObjectID = entity.Value,
                    .CastShadows = source.CastShadows,
                    .ReceiveShadows = source.ReceiveShadows });
        }
        for (const kairo::engine::Entity entity : scene.LightEntities())
            if ((scene.Light(entity).RenderLayers & renderLayers) != 0u)
                result.AddLight(MakeRenderLight(scene.Light(entity), scene.WorldTransform(entity)));
        if (const auto active = scene.ActiveEnvironment(); active.has_value())
        {
            const auto& source = scene.Environment(*active);
            kairo::renderer::RenderEnvironment environment;
            environment.BackgroundColor = source.BackgroundColor;
            environment.AmbientIntensity = source.AmbientIntensity * source.EnvironmentIntensity;
            environment.EnvironmentIntensity = source.EnvironmentIntensity;
            environment.ExposureEV100 = source.ExposureEV100;
            if (source.EnvironmentTexture.has_value())
                environment.EnvironmentTexture = assets.ResolveTexture(*source.EnvironmentTexture);
            result.SetEnvironment(environment);
        }
        return result;
    }
'''
new_build = '''    [[nodiscard]] inline kairo::renderer::RenderScene BuildRenderScene(
        const kairo::engine::Scene& scene,
        const RenderAssetBindings& assets,
        const SceneAnimationOverrides& animations,
        std::uint64_t renderLayers = kairo::engine::AllRenderLayers)
    {
        if (renderLayers == 0u)
            throw std::invalid_argument("Render extraction requires a non-empty layer mask.");
        kairo::renderer::RenderScene result;
        for (const kairo::engine::Entity entity : scene.RenderableEntities())
        {
            const auto& source = scene.MeshRenderer(entity);
            if ((source.RenderLayers & renderLayers) == 0u) continue;
            result.Add({ .Mesh = assets.ResolveMesh(source.MeshAsset),
                .Model = kairo::foundation::math::ToMatrix4(scene.WorldTransform(entity)),
                .Material = assets.ResolveMaterial(source.MaterialForSlot(0u)),
                .ObjectID = entity.Value,
                .CastShadows = source.CastShadows,
                .ReceiveShadows = source.ReceiveShadows });
        }
        for (const kairo::engine::Entity entity : scene.SceneInstanceEntities())
        {
            const auto& source = scene.SceneInstance(entity);
            if ((source.RenderLayers & renderLayers) == 0u) continue;
            const auto world = kairo::foundation::math::ToMatrix4(scene.WorldTransform(entity));
            const auto& primitives = assets.ResolveScene(source.SceneAsset);
            const auto* gltf = assets.ResolveGltfSource(source.SceneAsset);
            const auto* playback = animations.Find(entity);
            if (gltf == nullptr)
            {
                if (playback != nullptr)
                    throw std::invalid_argument(
                        "Animation playback requires a glTF-aware scene binding.");
                for (const auto& primitive : primitives)
                    result.Add({ .Mesh = primitive.Mesh,
                        .Model = world * primitive.LocalToAsset,
                        .Material = primitive.Material,
                        .ObjectID = entity.Value,
                        .CastShadows = source.CastShadows,
                        .ReceiveShadows = source.ReceiveShadows });
                continue;
            }

            const auto pose = playback != nullptr
                ? kairo::engine::SampleGltfAnimation(*gltf, playback->ClipIndex,
                    playback->TimeSeconds, playback->TimeMode)
                : kairo::engine::BuildGltfRestPose(*gltf);
            const auto poseWorld = kairo::engine::ResolveGltfWorldMatrices(*gltf, pose);
            std::unordered_map<std::uint32_t, kairo::renderer::SkinPalette> palettes;
            for (const auto& primitive : primitives)
            {
                if (primitive.NodeIndex >= poseWorld.size())
                    throw std::logic_error(
                        "Bound glTF primitive node index is outside evaluated pose.");
                kairo::renderer::MeshDraw draw;
                draw.Mesh = primitive.Mesh;
                draw.Material = primitive.Material;
                draw.ObjectID = entity.Value;
                draw.CastShadows = source.CastShadows;
                draw.ReceiveShadows = source.ReceiveShadows;
                if (primitive.SkinIndex == kairo::assets::GltfMissingIndex)
                    draw.Model = world * poseWorld[primitive.NodeIndex];
                else
                {
                    // EngineCore returns jointWorld * inverseBind in imported-
                    // asset space. The outer entity transform belongs on Model;
                    // applying the glTF mesh-node world here would double it.
                    draw.Model = world;
                    auto found = palettes.find(primitive.SkinIndex);
                    if (found == palettes.end())
                    {
                        const auto evaluated =
                            kairo::engine::BuildGltfAssetSpaceSkinPalette(
                                *gltf, pose, primitive.SkinIndex);
                        kairo::renderer::SkinPalette converted;
                        converted.JointMatrices = evaluated.JointMatrices;
                        found = palettes.emplace(primitive.SkinIndex,
                            std::move(converted)).first;
                    }
                    draw.Skinning = found->second;
                }
                result.Add(std::move(draw));
            }
        }
        for (const kairo::engine::Entity entity : scene.LightEntities())
            if ((scene.Light(entity).RenderLayers & renderLayers) != 0u)
                result.AddLight(MakeRenderLight(scene.Light(entity), scene.WorldTransform(entity)));
        if (const auto active = scene.ActiveEnvironment(); active.has_value())
        {
            const auto& source = scene.Environment(*active);
            kairo::renderer::RenderEnvironment environment;
            environment.BackgroundColor = source.BackgroundColor;
            environment.AmbientIntensity = source.AmbientIntensity * source.EnvironmentIntensity;
            environment.EnvironmentIntensity = source.EnvironmentIntensity;
            environment.ExposureEV100 = source.ExposureEV100;
            if (source.EnvironmentTexture.has_value())
                environment.EnvironmentTexture = assets.ResolveTexture(*source.EnvironmentTexture);
            result.SetEnvironment(environment);
        }
        return result;
    }

    [[nodiscard]] inline kairo::renderer::RenderScene BuildRenderScene(
        const kairo::engine::Scene& scene,
        const RenderAssetBindings& assets,
        std::uint64_t renderLayers = kairo::engine::AllRenderLayers)
    {
        const SceneAnimationOverrides none;
        return BuildRenderScene(scene, assets, none, renderLayers);
    }
'''
rep(old_build, new_build)
p.write_text(t)

# Extend tests with an end-to-end animated static + skinned glTF binding.
test_path = Path('Runtime/KairoRealtimeRenderBridge/tests/RealtimeSceneBridgeTests.cpp')
test = test_path.read_text()
append = r'''

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
'''
if 'glTF scene extraction evaluates static nodes and skin palettes' in test:
    raise SystemExit('tests already patched')
test_path.write_text(test + append)

Path('.github/workflows/apply-gltf-animation-render-bridge.yml').unlink(missing_ok=True)
Path('.github/scripts/apply_gltf_animation_render_bridge.py').unlink(missing_ok=True)
