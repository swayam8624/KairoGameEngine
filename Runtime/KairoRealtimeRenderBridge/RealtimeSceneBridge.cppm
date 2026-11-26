// TEACHING MODULE: Shared Editor/Player conversion from EngineCore Scene
// and KairoAssets identities into KairoRenderer RenderScene.
//
// RenderAssetBindings maps persistent authored AssetIDs to process-local
// renderer mesh/texture handles. It does not allocate, destroy, or own GPU
// resources; the selected RendererRuntime backend retains that lifetime.
//
// BuildRenderScene extracts visible draw objects/lights, resolves renderer
// handles, composes authored world transforms and assigns picking ObjectID
// from Entity ID. glTF skin palette math is evaluated via EngineCore.
// This shared module prevents Editor and Player from diverging in scene
// extraction, material bindings and photometric conversion.
// KairoEditor's SceneRenderBridge is a compatibility re-export.
module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include "MaterialFileSizeBudget.hpp"
#include "AuthoredClip.hpp"
#include <string>
#include <string_view>
#include "FrameHierarchyCache.hpp"
#include "FrameAssetCache.hpp"
// Header-only rigid IK kernel; no dependency on the Player module or target.
#include "../KairoPlayer/RuntimeImportedTwoBone.hpp"
#include <unordered_set>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

export module Kairo.Runtime.RealtimeSceneBridge;

import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Renderer;

export namespace kairo::runtime::renderbridge
{
    /// Complete CPU-side result of preparing one source mesh for rendering.
    struct RenderMeshImport final
    {
        kairo::renderer::Mesh Geometry;
        kairo::assets::DerivedDataKey CacheKey;
        bool CacheHit = false;
    };

    /// Task: import OBJ geometry through KairoAssets and adapt only the
    /// validated portable artifact into KairoRenderer geometry.
    [[nodiscard]] inline RenderMeshImport ImportRenderMesh(
        const std::filesystem::path& projectRoot,
        kairo::assets::MeshAssetHandle asset,
        const kairo::assets::AssetRegistry& registry,
        kairo::assets::ImportDatabase& imports,
        const kairo::assets::DerivedDataCache& cache)
    {
        const auto metadata = registry.Resolve(asset);
        if (metadata.Origin != kairo::assets::AssetOrigin::SourceFile)
            throw std::invalid_argument("Render mesh import requires a source-file asset.");
        kairo::assets::OBJMeshImporter importer;
        if (metadata.Importer != importer.Identifier())
            throw std::invalid_argument("Unsupported render mesh importer: " + metadata.Importer);
        kairo::assets::ImportRecord record{ metadata.ID, metadata.Path,
            importer.Identifier(), importer.Version(), {}, {}, 1u };
        auto outcome = kairo::assets::ImportSourceAsset(
            projectRoot, std::move(record), importer, registry, imports, cache);
        return { kairo::renderer::Mesh::FromArtifact(
            kairo::assets::ParseMeshDerivedArtifact(outcome.Artifact)),
            outcome.Key, outcome.CacheHit };
    }

    // One project-owned authored clip per source asset. It is separate from
    // importer caches and external glTF data, so normal packaging includes it.
    // Reject symlinks rather than following a path outside the project.
    [[nodiscard]] inline std::filesystem::path AuthoredClipPath(
        const std::filesystem::path& root,kairo::assets::AssetID asset)
    {
        const auto folder=root/"Animations";
        const auto file=folder/(asset.ToString()+".kanim");
        if(std::filesystem::is_symlink(folder)||std::filesystem::is_symlink(file))
            throw std::invalid_argument("Linked authored clip paths are unsupported.");
        if(std::filesystem::exists(folder)&&!std::filesystem::is_directory(folder))
            throw std::invalid_argument("Animations is not a directory.");
        return file;
    }

    [[nodiscard]] inline std::optional<std::string> ReadAuthoredClip(
        const std::filesystem::path& root,kairo::assets::AssetID asset)
    {
        const auto path=AuthoredClipPath(root,asset);
        if(!std::filesystem::exists(path))return std::nullopt;
        if(!std::filesystem::is_regular_file(path))
            throw std::invalid_argument("Authored clip is not a regular file.");
        if(std::filesystem::file_size(path)>clip_detail::MaxBytes)
            throw std::length_error("Authored clip exceeds byte budget.");
        std::ifstream in(path,std::ios::binary);
        if(!in)throw std::runtime_error("Cannot open authored clip.");
        std::string bytes(clip_detail::MaxBytes+1,'\0');
        in.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
        if(in.bad()||in.gcount()>static_cast<std::streamsize>(clip_detail::MaxBytes))
            throw std::runtime_error("Cannot read bounded authored clip.");
        bytes.resize(static_cast<std::size_t>(in.gcount()));
        return bytes;
    }

    inline void ApplyAuthoredClip(const std::filesystem::path& root,
        kairo::assets::AssetID asset,const kairo::assets::DerivedDataKey& key,
        kairo::assets::GltfSceneArtifactData& source)
    {
        const auto revision=ReadAuthoredClip(root,asset);
        if(!revision)return;
        auto clip=clip_detail::Decode<kairo::assets::GltfAnimationClipData>(key.ToString(),*revision);
        source.Animations.push_back(std::move(clip));
        try {kairo::assets::ValidateGltfSceneArtifactData(source);}
        catch(...) {source.Animations.pop_back();throw;}
    }

    // The loaded source already contains the previous authored clip, if any.
    // Replacing a draft must preserve imported indices and detect disk edits.
    inline void SaveAuthoredClip(const std::filesystem::path& root,
        kairo::assets::AssetID asset,const kairo::assets::DerivedDataKey& key,
        const kairo::assets::GltfSceneArtifactData& source,
        const kairo::assets::GltfAnimationClipData& draft,
        const std::optional<std::string>& expected)
    {
        const auto bytes=clip_detail::Encode(key.ToString(),draft);
        auto candidate=source;
        if(expected)
        {
            const auto previous=clip_detail::Decode<kairo::assets::GltfAnimationClipData>(key.ToString(),*expected);
            if(candidate.Animations.empty()||candidate.Animations.back()!=previous)
                throw std::runtime_error("Loaded animation revision differs from disk. Reopen the project.");
            candidate.Animations.back()=draft;
        }
        else candidate.Animations.push_back(draft);
        kairo::assets::ValidateGltfSceneArtifactData(candidate);
        const auto path=AuthoredClipPath(root,asset);
        if(ReadAuthoredClip(root,asset)!=expected)
            throw std::runtime_error("Authored clip changed on disk. Reopen the project.");
        kairo::engine::SaveTextFileAtomically(path,bytes,"authored animation clip");
    }

    /// Complete CPU-side glTF import needed by animation-aware runtime binding.
    /// Keep the validated source artifact beside the renderer adaptation so node,
    /// skin, rest-pose, and clip metadata are not discarded after mesh upload.
    struct RenderGltfSceneImport final
    {
        kairo::assets::GltfSceneArtifactData Source;
        kairo::renderer::GltfRenderAsset RenderAsset;
        kairo::assets::DerivedDataKey CacheKey;
        bool CacheHit = false;
    };

    // The ImportSourceAsset output is a typed CPU derived artifact. Retain
    // BOTH parsed glTF source (node/clip/skin metadata) and renderer-neutral
    // primitive geometry; a plain GPU mesh upload cannot recreate animation.
    // The URI texture resolver is caller-owned and chooses GPU handles.
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
        ApplyAuthoredClip(projectRoot,metadata.ID,outcome.Key,source);
        auto renderAsset = kairo::renderer::MakeGltfRenderAsset(source, resolveTexture);
        return { std::move(source), std::move(renderAsset), outcome.Key,
            outcome.CacheHit };
    }

    /// Compatibility projection for callers that only need static render data.
    // Compatibility helper for a static-only caller: discard extra parsed
    // glTF source metadata and return the render primitive projection.
    // Use WithSource when animation or skinning must survive import.
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

    /// Task: import a texture with explicit color/data semantics. Those
    /// semantics participate in the derived-data key and therefore cannot be
    /// guessed by a graphics backend.
    // Import settings (SRGB/Linear + normal semantics) are part of the
    // canonical import recipe and derived cache key, not renderer guesses.
    // ParseTextureDerivedArtifact returns owned CPU bytes; the caller's
    // RendererRuntime separately creates VkImage or MTLTexture storage.
    [[nodiscard]] inline kairo::assets::TextureArtifactData ImportRenderTexture(
        const std::filesystem::path& projectRoot,
        kairo::assets::TextureAssetHandle asset,
        const kairo::assets::TextureImportSettings& settings,
        const kairo::assets::AssetRegistry& registry,
        kairo::assets::ImportDatabase& imports,
        const kairo::assets::DerivedDataCache& cache)
    {
        const auto metadata = registry.Resolve(asset);
        kairo::assets::StbTextureImporter importer;
        if (metadata.Origin != kairo::assets::AssetOrigin::SourceFile ||
            metadata.Importer != importer.Identifier())
            throw std::invalid_argument(
                "Render texture import requires a source texture using kairo.texture.stb.");
        kairo::assets::ImportRecord record{ metadata.ID, metadata.Path,
            importer.Identifier(), importer.Version(),
            kairo::assets::CanonicalTextureImportSettings(settings), {}, 1u };
        auto outcome = kairo::assets::ImportSourceAsset(projectRoot, std::move(record),
            importer, registry, imports, cache);
        return kairo::assets::ParseTextureDerivedArtifact(outcome.Artifact);
    }

    /// Task: load the canonical generated material artifact. Source parsing and
    /// backend-specific material guesses are intentionally not permitted here.
    // Resolve stable typed ID -> registered project-local material path,
    // parse a full derived-artifact binary file, and require inner material
    // validity. This routine is CPU-only; it does not bind a descriptor set.
    // A fixed-schema material cannot require unbounded file allocation.
    // CheckedMaterialFileBytes rejects oversized input BEFORE vector resize;
    // the standard derived-artifact and material parsers still validate the
    // complete binary envelope, semantic type and trailing bytes.
    [[nodiscard]] inline kairo::assets::MaterialArtifactData LoadRenderMaterial(
        const std::filesystem::path& projectRoot,
        kairo::assets::MaterialAssetHandle asset,
        const kairo::assets::AssetRegistry& registry)
    {
        const auto metadata = registry.Resolve(asset);
        if (metadata.Type != kairo::assets::AssetType::Material ||
            metadata.Origin == kairo::assets::AssetOrigin::Builtin)
            throw std::invalid_argument(
                "Render material loading requires a non-builtin material asset.");
        const auto path = projectRoot / metadata.Path;
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) throw std::runtime_error("Cannot open material artifact: " + path.string());
        const auto length = input.tellg();
        // Require bounded input before heap allocation. MaterialArtifactData
        // has a fixed schema; even the largest canonical envelope is tiny.
        const std::size_t fileBytes = detail::CheckedMaterialFileBytes(
            static_cast<std::streamoff>(length));
        std::vector<std::byte> bytes(fileBytes);
        input.seekg(0);
        if (!bytes.empty())
            input.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        if (!input) throw std::runtime_error("Cannot read material artifact: " + path.string());
        return kairo::assets::ParseMaterialDerivedArtifact(
            kairo::assets::ParseDerivedArtifact(bytes));
    }

    /// Input: one registered builtin mesh metadata record.
    /// Output: canonical procedural geometry, or std::nullopt when the record
    /// is not a builtin mesh.
    /// Task: give Editor and Player one mapping from persistent primitive IDs
    /// to renderer geometry. Unknown builtin importers fail instead of silently
    /// disappearing from one consumer.
    [[nodiscard]] inline std::optional<kairo::renderer::Mesh> MakeBuiltinRenderMesh(
        const kairo::assets::AssetMetadata& metadata)
    {
        if (metadata.Type != kairo::assets::AssetType::Mesh ||
            metadata.Origin != kairo::assets::AssetOrigin::Builtin)
            return std::nullopt;
        if (metadata.Importer == "kairo.builtin.cube") return kairo::renderer::Mesh::MakeCube();
        if (metadata.Importer == "kairo.builtin.plane") return kairo::renderer::Mesh::MakePlane();
        if (metadata.Importer == "kairo.builtin.uv-sphere") return kairo::renderer::Mesh::MakeUVSphere();
        if (metadata.Importer == "kairo.builtin.cylinder") return kairo::renderer::Mesh::MakeCylinder();
        return std::nullopt;
    }

    /// Input: one validated EngineCore scene.
    /// Output: the active primary authored camera, the first active camera when
    /// no camera is marked primary, or KairoRenderer's default pose when the
    /// scene has no active camera.
    /// Task: keep Player, packaged games, previews, and capture tools on one
    /// deterministic camera-selection rule. Multiple active primary cameras
    /// are rejected because silently choosing one makes runtime output depend
    /// on incidental container order.
    // Traverse active authored cameras deterministically; a single marked
    // Primary wins over a previously selected nonprimary. Two active primaries
    // are a project error. Return default camera if none is active. World
    // hierarchy pose gives eye, forward and up independently of GPU backend.
    [[nodiscard]] inline kairo::renderer::CameraPose SelectRenderCamera(
        const kairo::engine::Scene& scene)
    {
        std::optional<kairo::engine::Entity> selected;
        for (const auto entity : scene.Entities())
        {
            if (!scene.HasCamera(entity) || !scene.IsActiveInHierarchy(entity))
                continue;
            const auto& camera = scene.Camera(entity);
            if (!camera.Primary && selected.has_value()) continue;
            if (camera.Primary && selected.has_value() &&
                scene.Camera(*selected).Primary)
                throw std::runtime_error(
                    "Scene contains more than one active primary camera.");
            if (camera.Primary || !selected.has_value()) selected = entity;
        }
        if (!selected.has_value()) return {};
        const auto transform = scene.WorldTransform(*selected);
        const auto& authored = scene.Camera(*selected);
        return {
            .Position = transform.Translation,
            .Target = transform.Translation + transform.Forward(),
            .Up = transform.Up(),
            .Projection = authored.Projection == kairo::engine::CameraProjection::Orthographic
                ? kairo::renderer::CameraProjectionMode::Orthographic
                : kairo::renderer::CameraProjectionMode::Perspective,
            .VerticalFovRadians = authored.VerticalFovRadians,
            .OrthographicSize = authored.OrthographicSize,
            .NearPlane = authored.NearPlane,
            .FarPlane = authored.FarPlane
        };
    }

    /// Process-local mapping from persistent KairoAssets identities to opaque
    /// KairoRenderer handles. The bridge owns no GPU resources; the host that
    /// created each handle remains responsible for destruction.
    // Maps authored IDs -> existing native renderer handles/materials.
    // These process-local maps must be refreshed on GPU device rebuild.
    // The AssetRegistry reference must outlive this lookup table. Each
    // Mesh/Texture renderer handle is BORROWED, NOT a GPU allocation owned
    // here; materials are stored by value. A device loss/recreation invalidates
    // process-local handles even when original AssetIDs remain durable.
    class RenderAssetBindings final
    {
    public:
        struct ScenePrimitive final
        {
            kairo::renderer::MeshHandle Mesh = kairo::renderer::InvalidMeshHandle;
            kairo::renderer::PBRMaterial Material;
            kairo::foundation::math::Mat4f LocalToAsset =
                kairo::foundation::math::Mat4f::Identity();
            std::uint32_t NodeIndex = kairo::assets::GltfMissingIndex;
            std::uint32_t PrimitiveIndex = kairo::assets::GltfMissingIndex;
            std::uint32_t SkinIndex = kairo::assets::GltfMissingIndex;
        };

        // Evaluated asset-space data is independent of any outer scene entity.
        // The binding owns an immutable rest snapshot; animated snapshots are
        // local to one extraction, never written back into the shared binding.
        struct EvaluatedGltfPose final
        {
            std::vector<kairo::foundation::math::Mat4f> World;
            std::unordered_map<std::uint32_t, kairo::renderer::SkinPalette> Palettes;
        };

        struct SceneBinding final
        {
            std::vector<ScenePrimitive> Primitives;
            std::optional<kairo::assets::GltfSceneArtifactData> GltfSource;
            std::optional<EvaluatedGltfPose> RestPose{};
        };

        // Source has already passed full asset validation at binding time.
        // Use the resolved node matrices for every referenced skin rather
        // than re-evaluating the hierarchy and validating the source per skin.
        [[nodiscard]] static EvaluatedGltfPose EvaluateGltfPose(
            const kairo::assets::GltfSceneArtifactData& source,
            const kairo::engine::GltfAnimationPose& pose,
            std::span<const ScenePrimitive> primitives)
        {
            EvaluatedGltfPose evaluated;
            evaluated.World = kairo::engine::ResolveGltfWorldMatrices(source, pose);
            for (const auto& primitive : primitives)
            {
                if (primitive.NodeIndex >= evaluated.World.size())
                    throw std::logic_error("Bound glTF primitive node index is outside evaluated pose.");
                if (primitive.SkinIndex == kairo::assets::GltfMissingIndex ||
                    evaluated.Palettes.contains(primitive.SkinIndex)) continue;

                if (primitive.SkinIndex >= source.Skins.size())
                    throw std::out_of_range("Bound glTF skin index is outside source.");
                const auto& skin = source.Skins[primitive.SkinIndex];
                if (skin.Joints.size() != skin.InverseBindMatrices.size())
                    throw std::invalid_argument("glTF joint and inverse bind counts differ.");
                kairo::renderer::SkinPalette palette;
                palette.JointMatrices.reserve(skin.Joints.size());
                for (std::size_t joint = 0u; joint < skin.Joints.size(); ++joint)
                {
                    // Same asset-space equation as EngineCore's reference
                    // BuildGltfAssetSpaceSkinPalette; no mesh-node inverse.
                    palette.JointMatrices.push_back(evaluated.World.at(skin.Joints[joint]) *
                        kairo::engine::GltfMatrixToKairo(skin.InverseBindMatrices[joint]));
                }
                evaluated.Palettes.emplace(primitive.SkinIndex, std::move(palette));
            }
            return evaluated;
        }

        explicit RenderAssetBindings(const kairo::assets::AssetRegistry& registry) noexcept
            : m_Registry(registry) {}

        // Resolve the typed AssetID first, then attach a valid GPU handle.
        // Upload and destruction remain the native renderer's job.
        // Check the typed persistent AssetID through KairoAssets, then bind it to a
        // valid renderer MeshHandle. RenderAssetBindings does not
        // create/upload/destroy GPU buffers; the native RendererRuntime owns that
        // lifetime.
        void BindMesh(kairo::assets::MeshAssetHandle asset, kairo::renderer::MeshHandle handle)
        {
            (void)m_Registry.Resolve(asset);
            if (handle == kairo::renderer::InvalidMeshHandle)
                throw std::invalid_argument("A render mesh binding requires a valid handle.");
            if (!m_Meshes.emplace(asset.ID, handle).second)
                throw std::invalid_argument("A render mesh asset is already bound.");
        }

        // Resolve a durable mesh asset to an already-uploaded native handle. If the
        // AssetRegistry says the type is wrong or the handle was never bound, fail
        // before emitting an invalid MeshDraw.
        [[nodiscard]] kairo::renderer::MeshHandle ResolveMesh(
            kairo::assets::MeshAssetHandle asset) const
        {
            (void)m_Registry.Resolve(asset);
            const auto found = m_Meshes.find(asset.ID);
            if (found == m_Meshes.end())
                throw std::out_of_range(
                    "No renderer mesh is bound for asset ID: " + asset.ID.ToString());
            return found->second;
        }

        // Check the stable material type and validate the backend-neutral PBRMaterial
        // before insertion. A duplicate ID is rejected instead of silently replacing
        // shader-facing material content.
        // Store validated PBR factors/texture handles by persistent ID. This
        // is a CPU conversion result; VkDescriptorSet is built later by
        // VulkanTriangle::RebuildMaterialDescriptors after frame retirement.
        void BindMaterial(kairo::assets::MaterialAssetHandle asset,
            kairo::renderer::PBRMaterial material)
        {
            (void)m_Registry.Resolve(asset);
            material.Validate();
            if (!m_Materials.emplace(asset.ID, material).second)
                throw std::invalid_argument("A render material asset is already bound.");
        }

        // A registered builtin material may use default PBR parameters when no
        // explicit runtime binding exists; imported materials without bindings must
        // fail. This is a deliberate builtin fallback, not arbitrary missing-asset
        // recovery.
        [[nodiscard]] kairo::renderer::PBRMaterial ResolveMaterial(
            kairo::assets::MaterialAssetHandle asset) const
        {
            const auto metadata = m_Registry.Resolve(asset);
            const auto found = m_Materials.find(asset.ID);
            if (found != m_Materials.end()) return found->second;
            if (metadata.Origin == kairo::assets::AssetOrigin::Builtin) return {};
            throw std::out_of_range(
                "No renderer material is bound for asset ID: " + asset.ID.ToString());
        }

        // Validate the persistent typed texture and its native TextureHandle before
        // registering the association. GPU image ownership remains with
        // RendererRuntime, and asset metadata does not store this process-local
        // handle.
        void BindTexture(kairo::assets::TextureAssetHandle asset,
            kairo::renderer::TextureHandle handle)
        {
            (void)m_Registry.Resolve(asset);
            if (handle == kairo::renderer::InvalidTextureHandle)
                throw std::invalid_argument("A render texture binding requires a valid handle.");
            if (!m_Textures.emplace(asset.ID, handle).second)
                throw std::invalid_argument("A render texture asset is already bound.");
        }

        // Require typed registry identity AND previously uploaded native
        // handle. No fallback for a referenced but unbound source texture.
        [[nodiscard]] kairo::renderer::TextureHandle ResolveTexture(
            kairo::assets::TextureAssetHandle asset) const
        {
            (void)m_Registry.Resolve(asset);
            const auto found = m_Textures.find(asset.ID);
            if (found == m_Textures.end())
                throw std::out_of_range(
                    "No renderer texture is bound for asset ID: " + asset.ID.ToString());
            return found->second;
        }

        // Store a list of already uploaded mesh primitives and material/transform
        // descriptors. Reject empty lists or invalid handles before publishing the
        // bound SceneAsset ID.
        void BindScene(kairo::assets::SceneAssetHandle asset,
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
        // Bind source glTF node/primitive/skin metadata to corresponding uploaded
        // mesh handles. Validate count, node membership and skin identity against the
        // exact parsed artifact to prevent mismatched animation palettes.
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
            // Finish CPU evaluation before publishing the binding. All
            // non-playing instances share this rest snapshot.
            auto rest = EvaluateGltfPose(source, kairo::engine::BuildGltfRestPose(source), primitives);
            if (!m_Scenes.emplace(asset.ID,
                SceneBinding{ std::move(primitives), std::move(source), std::move(rest) }).second)
                throw std::invalid_argument("A render scene asset is already bound.");
        }

        // Borrow a primitive vector from this process-local asset binding.
        // The caller immediately copies primitive values to RenderScene draws;
        // retaining references after binding destruction is invalid.
        [[nodiscard]] const std::vector<ScenePrimitive>& ResolveScene(
            kairo::assets::SceneAssetHandle asset) const
        {
            return ResolveSceneBinding(asset).Primitives;
        }

        // nullptr means this bound scene has only static primitive records;
        // non-null metadata enables glTF joint/rest-pose/clip evaluation.
        // Pointer borrows SceneBinding's optional artifact lifetime.
        [[nodiscard]] const kairo::assets::GltfSceneArtifactData* ResolveGltfSource(
            kairo::assets::SceneAssetHandle asset) const
        {
            const auto& binding = ResolveSceneBinding(asset);
            return binding.GltfSource.has_value() ? &*binding.GltfSource : nullptr;
        }

    private:
        const kairo::assets::AssetRegistry& m_Registry;
        std::unordered_map<kairo::assets::AssetID, kairo::renderer::MeshHandle,
            kairo::assets::AssetIDHash> m_Meshes;
        std::unordered_map<kairo::assets::AssetID, kairo::renderer::PBRMaterial,
            kairo::assets::AssetIDHash> m_Materials;
        std::unordered_map<kairo::assets::AssetID, kairo::renderer::TextureHandle,
            kairo::assets::AssetIDHash> m_Textures;
        std::unordered_map<kairo::assets::AssetID, SceneBinding,
            kairo::assets::AssetIDHash> m_Scenes;

        // Reject empty scene bindings, invalid GPU mesh handles or malformed
        // materials before publishing an authored SceneAssetID mapping.
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

    public:
        // Look up bound SceneAsset by stable ID and check it still resolves as the
        // declared type. The returned reference borrows the binding owner's lifetime;
        // do not cache it after mutating or destroying bindings.
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

    // One validated clip-time/mode override, separate from durable .kscene
    // components and separate from GPU mesh/texture allocations.
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

    // Procedural correction applied AFTER clip sampling and BEFORE building
    // asset-space skin palettes. Caller supplies root-parent-space coordinates.
    struct SceneTwoBoneIK final
    {
        std::array<std::uint32_t,3> Nodes{};
        std::array<double,3> Target{}, Pole{};
        void Validate() const
        {
            kairo::player::detail::ValidateImportedTwoBoneRequest(Nodes,Target,Pole);
        }
    };

    [[nodiscard]] inline std::pair<kairo::engine::GltfAnimationPose,bool> ApplyGltfTwoBoneIK(
        const kairo::assets::GltfSceneArtifactData& source,
        const kairo::engine::GltfAnimationPose& pose,const SceneTwoBoneIK& request)
    {
        return kairo::player::detail::ApplyImportedTwoBone(source,pose,request.Nodes,request.Target,request.Pole,
            [](const auto& trs){return kairo::foundation::math::ToMatrix4(trs);});
    }

    /// Per-instance playback overrides. Animation state is intentionally kept
    /// outside serialized ECS in this first runtime slice so Editor/Player can
    /// drive preview/playback without changing the Scene schema prematurely.
    // Map authored Entity.Value -> clip/time overrides for extraction only.
    // Player advances clocks; Editor preview may supply its own values.
    // Removing an entity should clear its override to avoid stale identity.
    class SceneAnimationOverrides final
    {
        std::unordered_map<std::uint32_t, SceneAnimationPlayback> m_Playback;
        struct IKBinding{ kairo::assets::AssetID Asset; SceneTwoBoneIK Request; };
        std::unordered_map<std::uint32_t,IKBinding> m_IK;

    public:
        // Store validated finite playback time for this authored instance.
        // Extraction uses it to evaluate glTF pose without modifying Scene.
        void Set(kairo::engine::Entity entity, SceneAnimationPlayback playback)
        {
            playback.Validate();
            m_Playback.insert_or_assign(entity.Value, playback);
        }

        void Clear(kairo::engine::Entity entity) noexcept
        {
            m_Playback.erase(entity.Value);
        }

        // Return a BORROWED pointer to optional override; do not retain
        // past map mutation/clear or the SceneAnimationOverrides owner.
        [[nodiscard]] const SceneAnimationPlayback* Find(
            kairo::engine::Entity entity) const noexcept
        {
            const auto found = m_Playback.find(entity.Value);
            return found == m_Playback.end() ? nullptr : &found->second;
        }

        // Keep IK independent from clip selection and bound to the asset
        // identity, so replacing a scene instance cannot reuse old node indices.
        void SetTwoBoneIK(kairo::engine::Entity entity,kairo::assets::AssetID asset,SceneTwoBoneIK request)
        {
            request.Validate();
            m_IK.insert_or_assign(entity.Value,IKBinding{asset,request});
        }
        void ClearTwoBoneIK(kairo::engine::Entity entity) noexcept { m_IK.erase(entity.Value); }
        [[nodiscard]] const SceneTwoBoneIK* FindTwoBoneIK(kairo::engine::Entity entity,
            kairo::assets::AssetID asset) const noexcept
        {
            const auto it=m_IK.find(entity.Value);
            return it!=m_IK.end()&&it->second.Asset==asset?&it->second.Request:nullptr;
        }
        void ForgetMismatchedIK(kairo::engine::Entity entity,kairo::assets::AssetID asset)
        {
            const auto it=m_IK.find(entity.Value);
            if(it!=m_IK.end()&&it->second.Asset!=asset)m_IK.erase(it);
        }
        void RetainTwoBoneIK(const std::unordered_set<std::uint32_t>& present)
        {
            for(auto it=m_IK.begin();it!=m_IK.end();)
                if(!present.contains(it->first))it=m_IK.erase(it);else ++it;
        }

        [[nodiscard]] bool Empty() const noexcept { return m_Playback.empty()&&m_IK.empty(); }
    };

    /// Input: one validated EngineCore light and its world transform.
    /// Output: backend-neutral real-time light data.
    /// Task: apply the single Editor/Player photometric calibration: 10 klux
    /// for directional lights or 100 candela for local lights maps to one
    /// scene-radiance unit. Coordinates stay right-handed with no axis swap.
    // Apply one Editor/Player photometric conversion: 10,000 lux per renderer
    // intensity unit for directional lights, 100 candela per unit for local
    // lights. Use world-space transform for direction and position; validate
    // output before GPU submission.
    [[nodiscard]] inline kairo::renderer::RenderLight MakeRenderLight(
        const kairo::engine::LightComponent& source,
        const kairo::foundation::math::Transformf& world)
    {
        source.Validate();
        kairo::renderer::RenderLight result;
        result.Position = world.Translation;
        result.Color = source.Color;
        result.Range = source.Range;
        result.InnerConeRadians = source.InnerConeRadians;
        result.OuterConeRadians = source.OuterConeRadians;
        result.CastShadows = source.Shadows != kairo::engine::ShadowPolicy::Disabled;
        switch (source.Type)
        {
            case kairo::engine::LightType::Directional:
                result.Type = kairo::renderer::RenderLightType::Directional;
                result.Direction = -world.Forward();
                result.Intensity = source.Intensity / 10'000.0f;
                break;
            case kairo::engine::LightType::Point:
                result.Type = kairo::renderer::RenderLightType::Point;
                result.Direction = world.Forward();
                result.Intensity = source.Intensity / 100.0f;
                break;
            case kairo::engine::LightType::Spot:
                result.Type = kairo::renderer::RenderLightType::Spot;
                result.Direction = world.Forward();
                result.Intensity = source.Intensity / 100.0f;
                break;
            case kairo::engine::LightType::RectangleArea:
                result.Type = kairo::renderer::RenderLightType::RectangleArea;
                result.Direction = world.Forward();
                result.Intensity = source.Intensity / 100.0f;
                result.AreaWidth = source.AreaWidth;
                result.AreaHeight = source.AreaHeight;
                break;
        }
        result.Validate();
        return result;
    }

    // Render extraction is CPU-side; the renderer backend will upload/
    // draw using already-bound native handles. Stable Entity IDs become
    // ObjectIDs for picking, not GPU resource identities.
    /// Input: one EngineCore scene, complete process-local asset bindings, and
    /// a non-zero camera/viewport layer mask.
    /// Output: stable renderer draws, lights, and environment shared by Editor
    /// and Player regardless of the selected graphics API.
    /// Degeneracy: missing bindings, invalid material state, singular transforms,
    /// and an empty layer mask fail before GPU command recording.
    // CPU-only extraction traverses active, layer-visible Scene entities; resolve
    // stable mesh/material/scene asset references, derive world matrices, assign
    // authored Entity.Value as picking ObjectID, and append lights/environment.
    // No GPU upload here.
    [[nodiscard]] inline kairo::renderer::RenderScene BuildRenderScene(
        const kairo::engine::Scene& scene,
        const RenderAssetBindings& assets,
        const SceneAnimationOverrides& animations,
        std::uint64_t renderLayers = kairo::engine::AllRenderLayers)
    {
        if (renderLayers == 0u)
            throw std::invalid_argument("Render extraction requires a non-empty layer mask.");
        kairo::renderer::RenderScene result;
        // One sorted enumeration for all categories; retain the original mesh,
        // scene-instance, light ordering. Cache only hierarchy paths we visit.
        const auto entities = scene.Entities();
        detail::FrameHierarchyCache<kairo::engine::Scene, kairo::engine::Entity,
            kairo::foundation::math::Transformf> hierarchy(scene, entities);
        // Asset registries and bindings must remain stable during extraction.
        // Reuse checked typed lookups for repeated assets; never retain a
        // renderer handle or borrowed scene binding across frame boundaries.
        using AssetID = kairo::assets::AssetID;
        using AssetHash = kairo::assets::AssetIDHash;
        detail::FrameAssetCache<AssetID, kairo::renderer::MeshHandle, AssetHash> meshes;
        detail::FrameAssetCache<AssetID, kairo::renderer::PBRMaterial, AssetHash> materials;
        detail::FrameAssetCache<AssetID, const RenderAssetBindings::SceneBinding*, AssetHash> scenes;
        for (const kairo::engine::Entity entity : entities)
        {
            if (!scene.HasMeshRenderer(entity)) continue;
            const auto& source = scene.MeshRenderer(entity);
            if (!source.Visible || (source.RenderLayers & renderLayers) == 0u) continue;
            const auto& world = hierarchy.Resolve(entity);
            if (!world) continue;
            const auto mesh = meshes.Get(source.MeshAsset.ID,
                [&] { return assets.ResolveMesh(source.MeshAsset); });
            const auto materialAsset = source.MaterialForSlot(0u);
            const auto& material = materials.Get(materialAsset.ID,
                [&] { return assets.ResolveMaterial(materialAsset); });
            result.Add({ .Mesh = mesh,
                .Model = kairo::foundation::math::ToMatrix4(*world),
                .Material = material,
                .ObjectID = entity.Value,
                .CastShadows = source.CastShadows,
                .ReceiveShadows = source.ReceiveShadows });
        }
        for (const kairo::engine::Entity entity : entities)
        {
            if (!scene.HasSceneInstance(entity)) continue;
            const auto& source = scene.SceneInstance(entity);
            if (!source.Visible || (source.RenderLayers & renderLayers) == 0u) continue;
            const auto& transform = hierarchy.Resolve(entity);
            if (!transform) continue;
            const auto world = kairo::foundation::math::ToMatrix4(*transform);
            const auto& binding = *scenes.Get(source.SceneAsset.ID,
                [&] { return &assets.ResolveSceneBinding(source.SceneAsset); });
            const auto& primitives = binding.Primitives;
            const auto* gltf = binding.GltfSource ? &*binding.GltfSource : nullptr;
            const auto* playback = animations.Find(entity);
            const auto* ik=animations.FindTwoBoneIK(entity,source.SceneAsset.ID);
            // A non-glTF multi-primitive scene uses the prebound LocalToAsset transforms.
            // Requesting animation for it is invalid because there is no validated glTF
            // node hierarchy to sample.
            if (gltf == nullptr)
            {
                if (playback != nullptr||ik!=nullptr)
                    throw std::invalid_argument(
                        "Animation playback or IK requires a glTF-aware scene binding.");
                for (const auto& primitive : primitives)
                    result.Add({ .Mesh = primitive.Mesh,
                        .Model = world * primitive.LocalToAsset,
                        .Material = primitive.Material,
                        .ObjectID = entity.Value,
                        .CastShadows = source.CastShadows,
                        .ReceiveShadows = source.ReceiveShadows });
                continue;
            }

            // Rest matrices and skin palettes are owned by the immutable
            // asset binding. Each animated entity evaluates a separate pose;
            // no cross-entity mutable state or frame-to-frame cache.
            std::optional<RenderAssetBindings::EvaluatedGltfPose> animated;
            if (playback != nullptr||ik!=nullptr)
            {
                auto pose=playback?kairo::engine::SampleGltfAnimation(*gltf,
                    playback->ClipIndex,playback->TimeSeconds,playback->TimeMode):
                    kairo::engine::BuildGltfRestPose(*gltf);
                if(ik)pose=ApplyGltfTwoBoneIK(*gltf,pose,*ik).first;
                animated.emplace(RenderAssetBindings::EvaluateGltfPose(*gltf,pose,primitives));
            }
            const auto& evaluated = animated ? *animated : binding.RestPose.value();
            for (const auto& primitive : primitives)
            {
                kairo::renderer::MeshDraw draw;
                draw.Mesh = primitive.Mesh;
                draw.Material = primitive.Material;
                draw.ObjectID = entity.Value;
                draw.CastShadows = source.CastShadows;
                draw.ReceiveShadows = source.ReceiveShadows;
                // Unskinned primitives use outer authored world * evaluated node world.
                // Skinned primitives instead use outer world as Model with a
                // jointWorld*inverseBind palette in asset space; multiplying node world twice
                // would break skinning.
                if (primitive.SkinIndex == kairo::assets::GltfMissingIndex)
                    draw.Model = world * evaluated.World[primitive.NodeIndex];
                else
                {
                    // EngineCore returns jointWorld * inverseBind in imported-
                    // asset space. The outer entity transform belongs on Model;
                    // applying the glTF mesh-node world here would double it.
                    draw.Model = world;
                    draw.Skinning = evaluated.Palettes.at(primitive.SkinIndex);
                }
                result.Add(std::move(draw));
            }
        }
        // Extract renderer lights only for permitted render layers; the shared
        // conversion avoids diverging Editor and Player intensity conventions when
        // using the same authored project.
        std::optional<kairo::engine::Entity> active;
        for (const kairo::engine::Entity entity : entities)
        {
            if (scene.HasLight(entity) && (scene.Light(entity).RenderLayers & renderLayers) != 0u)
                if (const auto& world = hierarchy.Resolve(entity); world)
                    result.AddLight(MakeRenderLight(scene.Light(entity), *world));
            // Match Scene::ActiveEnvironment: enabled, active, highest priority,
            // and lowest stable ID on ties. Environments have no render-layer filter.
            if (scene.HasEnvironment(entity) && scene.Environment(entity).Enabled &&
                (!active || scene.Environment(entity).Priority > scene.Environment(*active).Priority) &&
                scene.IsActiveInHierarchy(entity))
                active = entity;
        }
        if (active.has_value())
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
}
