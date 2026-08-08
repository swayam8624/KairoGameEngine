module;

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

export module Kairo.Runtime.RenderBridge.SceneSnapshot;

import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.RayTracer;

export namespace kairo::runtime::renderbridge
{
    namespace engine = kairo::engine;
    namespace assets = kairo::assets;
    namespace math = kairo::foundation::math;
    namespace ray = kairo::foundation::raytracer;

    enum class DiagnosticSeverity : std::uint8_t { Info, Warning, Error };

    struct SceneConversionDiagnostic final
    {
        DiagnosticSeverity Severity = DiagnosticSeverity::Info;
        std::optional<engine::Entity> Entity;
        std::string Code;
        std::string Message;
    };

    struct OfflineResolvedSubmesh final
    {
        ray::Material Material;
        ray::TriangleMesh Mesh;
        std::optional<ray::Texture2D> AlbedoTexture;
    };

    struct OfflineResolvedGeometry final
    {
        std::vector<OfflineResolvedSubmesh> Submeshes;
        std::vector<std::string> Warnings;
    };

    /// Asset resolution is injected by the host. The bridge owns scene semantics,
    /// but it never guesses where cooked assets live on disk.
    struct OfflineSceneAssetResolver final
    {
        std::function<OfflineResolvedGeometry(
            const engine::MeshRendererComponent&,
            const math::Transformf&)> ResolveMeshRenderer;

        std::function<OfflineResolvedGeometry(
            const engine::SceneInstanceComponent&,
            const math::Transformf&)> ResolveSceneInstance;

        std::function<ray::Texture2D(assets::TextureAssetHandle)> ResolveTexture;
    };

    struct OfflineSceneSettings final
    {
        std::uint32_t Width = 1280u;
        std::uint32_t Height = 720u;
        std::uint32_t MaxDepth = 6u;
        std::uint32_t SampleSeed = 0u;
        ray::RenderMode Mode = ray::RenderMode::PBR;
        ray::AccelerationMode Acceleration = ray::AccelerationMode::BVHSAH;

        void Validate() const
        {
            if (Width == 0u || Height == 0u)
                throw std::invalid_argument("Offline scene dimensions must be non-zero.");
            if (Width > 32768u || Height > 32768u)
                throw std::invalid_argument("Offline scene dimensions exceed the ray tracer safety limit.");
            if (MaxDepth > 64u)
                throw std::invalid_argument("Offline scene max depth exceeds the ray tracer safety limit.");
        }
    };

    struct SceneConversionResult final
    {
        ray::Scene Snapshot;
        std::vector<SceneConversionDiagnostic> Diagnostics;

        [[nodiscard]] bool Supported() const noexcept
        {
            for (const auto& diagnostic : Diagnostics)
                if (diagnostic.Severity == DiagnosticSeverity::Error) return false;
            return true;
        }
    };

    namespace detail
    {
        [[nodiscard]] inline ray::Color3f ToColor(const math::Vec3f& value) noexcept
        {
            return { value.x, value.y, value.z };
        }

        inline void AddDiagnostic(SceneConversionResult& result,
            DiagnosticSeverity severity,
            std::optional<engine::Entity> entity,
            std::string code,
            std::string message)
        {
            result.Diagnostics.push_back({ severity, entity, std::move(code), std::move(message) });
        }

        inline void AppendGeometry(SceneConversionResult& result,
            engine::Entity entity,
            OfflineResolvedGeometry geometry)
        {
            for (auto& warning : geometry.Warnings)
                AddDiagnostic(result, DiagnosticSeverity::Warning, entity,
                    "asset-resolution-warning", std::move(warning));

            for (auto& submesh : geometry.Submeshes)
            {
                if (submesh.Mesh.Triangles.empty())
                {
                    AddDiagnostic(result, DiagnosticSeverity::Warning, entity,
                        "empty-submesh", "Resolved submesh contains no triangles and was skipped.");
                    continue;
                }

                if (submesh.AlbedoTexture.has_value())
                {
                    submesh.Material.AlbedoTextureIndex = result.Snapshot.AddTexture(
                        std::move(*submesh.AlbedoTexture));
                }
                else
                {
                    submesh.Material.AlbedoTextureIndex = std::numeric_limits<std::uint32_t>::max();
                }

                const std::uint32_t materialIndex = result.Snapshot.AddMaterial(
                    std::move(submesh.Material));
                for (const auto& triangle : submesh.Mesh.Triangles)
                    result.Snapshot.AddTriangle(triangle, materialIndex);
            }
        }
    }

    /// Converts one immutable EngineCore scene snapshot into the ray tracer's
    /// current scene contract. Unsupported semantics are always diagnosed.
    [[nodiscard]] inline SceneConversionResult ConvertSceneSnapshot(
        const engine::Scene& scene,
        const OfflineSceneAssetResolver& resolver,
        const OfflineSceneSettings& settings = {})
    {
        settings.Validate();
        SceneConversionResult result;
        result.Snapshot.Settings.Width = settings.Width;
        result.Snapshot.Settings.Height = settings.Height;
        result.Snapshot.Settings.SamplesPerPixel = 1u;
        result.Snapshot.Settings.MaxDepth = settings.MaxDepth;
        result.Snapshot.Settings.SampleSeed = settings.SampleSeed;
        result.Snapshot.Settings.Mode = settings.Mode;
        result.Snapshot.Settings.Acceleration = settings.Acceleration;

        const auto primary = scene.PrimaryCamera();
        std::optional<engine::Entity> cameraEntity = primary;
        if (!cameraEntity.has_value())
        {
            const auto cameras = scene.CameraEntities();
            if (!cameras.empty())
            {
                cameraEntity = cameras.front();
                detail::AddDiagnostic(result, DiagnosticSeverity::Warning, *cameraEntity,
                    "camera-fallback", "Scene has no primary camera; the lowest stable camera entity was selected.");
            }
        }

        if (!cameraEntity.has_value())
        {
            detail::AddDiagnostic(result, DiagnosticSeverity::Error, std::nullopt,
                "missing-camera", "Offline rendering requires an authored camera.");
        }
        else
        {
            const auto& camera = scene.Camera(*cameraEntity);
            const math::Transformf world = scene.WorldTransform(*cameraEntity);
            if (camera.Projection == engine::CameraProjection::Orthographic)
            {
                detail::AddDiagnostic(result, DiagnosticSeverity::Error, *cameraEntity,
                    "orthographic-camera-unsupported",
                    "The CPU ray tracer currently supports perspective cameras only.");
            }
            else
            {
                constexpr float radiansToDegrees = 57.29577951308232f;
                const float aspect = static_cast<float>(settings.Width) /
                    static_cast<float>(settings.Height);
                result.Snapshot.MainCamera = ray::Camera::LookAt(
                    world.Translation,
                    world.Translation + world.Forward(),
                    world.Up(),
                    camera.VerticalFovRadians * radiansToDegrees,
                    aspect);
                result.Snapshot.Settings.DepthNear = camera.NearPlane;
                result.Snapshot.Settings.DepthFar = camera.FarPlane;
            }
        }

        if (const auto environmentEntity = scene.ActiveEnvironment(); environmentEntity.has_value())
        {
            const auto& environment = scene.Environment(*environmentEntity);
            result.Snapshot.Settings.Background = detail::ToColor(environment.BackgroundColor);
            result.Snapshot.Environment.Enabled = true;
            result.Snapshot.Environment.Color = detail::ToColor(environment.BackgroundColor);
            result.Snapshot.Environment.Intensity = environment.EnvironmentIntensity;

            if (environment.EnvironmentTexture.has_value())
            {
                if (!resolver.ResolveTexture)
                {
                    detail::AddDiagnostic(result, DiagnosticSeverity::Error, *environmentEntity,
                        "environment-texture-resolver-missing",
                        "The scene uses an environment texture but the host supplied no texture resolver.");
                }
                else
                {
                    try
                    {
                        result.Snapshot.Environment.TextureIndex = result.Snapshot.AddTexture(
                            resolver.ResolveTexture(*environment.EnvironmentTexture));
                    }
                    catch (const std::exception& error)
                    {
                        detail::AddDiagnostic(result, DiagnosticSeverity::Error, *environmentEntity,
                            "environment-texture-resolution-failed", error.what());
                    }
                }
            }

            result.Snapshot.Settings.FogColor = detail::ToColor(environment.FogColor);
            result.Snapshot.Settings.FogDensity = environment.FogDensity;
            result.Snapshot.Settings.FogNear = environment.FogNear;
            result.Snapshot.Settings.FogFar = environment.FogFar;
            switch (environment.Fog)
            {
                case engine::FogMode::Disabled:
                    result.Snapshot.Settings.Fog = ray::FogMode::Disabled;
                    break;
                case engine::FogMode::Linear:
                    result.Snapshot.Settings.Fog = ray::FogMode::Linear;
                    break;
                case engine::FogMode::Exponential:
                    result.Snapshot.Settings.Fog = ray::FogMode::Exponential;
                    break;
            }
        }

        for (const engine::Entity entity : scene.LightEntities())
        {
            const auto& light = scene.Light(entity);
            const math::Transformf world = scene.WorldTransform(entity);
            switch (light.Type)
            {
                case engine::LightType::Point:
                    result.Snapshot.Lights.push_back({
                        world.Translation,
                        detail::ToColor(light.Color),
                        light.Intensity });
                    break;
                case engine::LightType::RectangleArea:
                    result.Snapshot.AreaLights.push_back({
                        world.Translation,
                        world.Right() * light.AreaWidth,
                        world.Up() * light.AreaHeight,
                        detail::ToColor(light.Color),
                        light.Intensity,
                        8u });
                    break;
                case engine::LightType::Directional:
                {
                    ray::DirectionalLight directional;
                    directional.Direction = world.Forward();
                    directional.Color = detail::ToColor(light.Color);
                    directional.Illuminance = light.Intensity;
                    result.Snapshot.DirectionalLights.push_back(directional);
                    break;
                }
                case engine::LightType::Spot:
                {
                    ray::SpotLight spot;
                    spot.Position = world.Translation;
                    spot.Direction = world.Forward();
                    spot.Color = detail::ToColor(light.Color);
                    spot.Intensity = light.Intensity;
                    spot.Range = light.Range;
                    spot.InnerConeCosine = std::cos(light.InnerConeRadians);
                    spot.OuterConeCosine = std::cos(light.OuterConeRadians);
                    result.Snapshot.SpotLights.push_back(spot);
                    break;
                }
            }
        }

        for (const engine::Entity entity : scene.Entities())
        {
            if (!scene.IsActiveInHierarchy(entity)) continue;

            if (scene.HasMeshRenderer(entity) && scene.MeshRenderer(entity).Visible)
            {
                if (!resolver.ResolveMeshRenderer)
                {
                    detail::AddDiagnostic(result, DiagnosticSeverity::Error, entity,
                        "mesh-resolver-missing",
                        "Scene contains a mesh renderer but the host supplied no cooked-mesh resolver.");
                }
                else
                {
                    try
                    {
                        detail::AppendGeometry(result, entity,
                            resolver.ResolveMeshRenderer(scene.MeshRenderer(entity), scene.WorldTransform(entity)));
                    }
                    catch (const std::exception& error)
                    {
                        detail::AddDiagnostic(result, DiagnosticSeverity::Error, entity,
                            "mesh-resolution-failed", error.what());
                    }
                }
            }

            if (scene.HasSceneInstance(entity) && scene.SceneInstance(entity).Visible)
            {
                if (!resolver.ResolveSceneInstance)
                {
                    detail::AddDiagnostic(result, DiagnosticSeverity::Error, entity,
                        "scene-instance-resolver-missing",
                        "Scene contains an imported scene instance but the host supplied no scene-instance resolver.");
                }
                else
                {
                    try
                    {
                        detail::AppendGeometry(result, entity,
                            resolver.ResolveSceneInstance(scene.SceneInstance(entity), scene.WorldTransform(entity)));
                    }
                    catch (const std::exception& error)
                    {
                        detail::AddDiagnostic(result, DiagnosticSeverity::Error, entity,
                            "scene-instance-resolution-failed", error.what());
                    }
                }
            }
        }

        if (result.Supported())
            result.Snapshot.BuildAcceleration();
        return result;
    }
}
