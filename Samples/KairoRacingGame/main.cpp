#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.Math.Quaternion;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Player.RuntimeInputBridge;
import Kairo.Player.RuntimePackaging;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimeRenderBridge;
import Kairo.Renderer;

namespace
{
    using kairo::engine::Entity;
    using kairo::foundation::math::Quatf;
    using kairo::foundation::math::Vec3d;
    using kairo::foundation::math::Vec3f;

    struct Arguments final
    {
        std::filesystem::path Project = KAIRO_RACING_PROJECT_PATH;
        std::optional<std::string> PackageProfile;
        bool Replace = false;
        bool Smoke = false;
        kairo::renderer::GraphicsBackend Backend =
            kairo::renderer::GraphicsBackend::Automatic;
    };

    [[nodiscard]] Arguments ParseArguments(int argc, char** argv)
    {
        Arguments result;
        bool projectSeen = false;
        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument = argv[index];
            if (argument == "--smoke")
                result.Smoke = true;
            else if (argument == "--replace")
                result.Replace = true;
            else if (argument == "--package")
            {
                if (++index >= argc)
                    throw std::invalid_argument("--package requires a build-profile name.");
                result.PackageProfile = argv[index];
            }
            else if (argument == "--renderer")
            {
                if (++index >= argc)
                    throw std::invalid_argument(
                        "--renderer requires auto, vulkan, metal, d3d12, or opengl.");
                result.Backend = kairo::renderer::ParseGraphicsBackend(argv[index]);
            }
            else if (!argument.empty() && argument.front() == '-')
                throw std::invalid_argument(
                    "Unknown KAIRO Racing option: " + std::string(argument));
            else if (projectSeen)
                throw std::invalid_argument(
                    "KAIRO Racing accepts at most one project path.");
            else
            {
                result.Project = argv[index];
                projectSeen = true;
            }
        }
        if (result.Replace && !result.PackageProfile)
            throw std::invalid_argument("--replace requires --package.");
        if (result.Smoke && result.PackageProfile)
            throw std::invalid_argument("--smoke and --package are mutually exclusive.");
        return result;
    }

    [[nodiscard]] Entity RequireTagged(
        const kairo::engine::Scene& scene, std::string_view tag)
    {
        std::optional<Entity> found;
        for (const Entity entity : scene.Entities())
        {
            if (!scene.HasTag(entity, tag)) continue;
            if (found)
                throw std::invalid_argument(
                    "Expected exactly one entity tagged '" + std::string(tag) + "'.");
            found = entity;
        }
        if (!found)
            throw std::invalid_argument(
                "Missing required entity tagged '" + std::string(tag) + "'.");
        return *found;
    }

    [[nodiscard]] Quatf StartRotation() noexcept
    {
        // Upstream pmndrs/racing-game: rotation.y = PI / 2 + 0.35.
        return { 0.0f, 0.81941986f, 0.0f, 0.57319377f };
    }

    class ArcadeCarDriver final : public kairo::player::RuntimeFixedStepListener
    {
    public:
        ArcadeCarDriver(kairo::player::RuntimePhysicsBridge& physics, Entity car)
            : m_Physics(physics), m_Car(car)
        {
        }

        void SetDrive(float steering, float throttle) noexcept
        {
            m_Steering = std::clamp(steering, -1.0f, 1.0f);
            m_Throttle = std::clamp(throttle, -1.0f, 1.0f);
        }

        void SetBrake(bool brake) noexcept { m_Brake = brake; }

        [[nodiscard]] float SpeedMetresPerSecond() const
        {
            const auto body = m_Physics.BodyFor(m_Car);
            if (!body) return 0.0f;
            return m_Physics.World().Bodies().at(*body).State.LinearVelocity.Length();
        }

        void BeforePhysicsStep(float fixedDeltaSeconds) override
        {
            const auto bodyID = m_Physics.BodyFor(m_Car);
            if (!bodyID)
                throw std::runtime_error("KAIRO Racing car has no runtime physics body.");

            auto& state = m_Physics.World().Bodies().at(*bodyID).State;
            Vec3f forward = kairo::foundation::math::Forward(state.Rotation);
            forward.y = 0.0f;
            forward = kairo::foundation::math::SafeNormalize(
                forward, Vec3f::Forward());

            Vec3f right = kairo::foundation::math::Right(state.Rotation);
            right.y = 0.0f;
            right = kairo::foundation::math::SafeNormalize(
                right, Vec3f::Right());

            const float forwardSpeed =
                kairo::foundation::math::Dot(state.LinearVelocity, forward);
            const float absoluteForwardSpeed = std::abs(forwardSpeed);

            // Upstream reference uses force=1800 and maxSpeed=88. The KAIRO
            // body has different mass semantics, so V1 maps those values onto
            // bounded fixed-step impulses rather than pretending the units match.
            const float speedFactor =
                std::clamp(1.0f - absoluteForwardSpeed / 88.0f, 0.10f, 1.0f);
            const float driveStrength = m_Throttle >= 0.0f ? 24.0f : 14.0f;
            const float impulse = m_Throttle * driveStrength * speedFactor;

            if (std::abs(m_Throttle) > 1.0e-4f)
            {
                m_Physics.ApplyEntityImpulse(m_Car, {
                    static_cast<double>(forward.x * impulse * fixedDeltaSeconds * 60.0f),
                    0.0,
                    static_cast<double>(forward.z * impulse * fixedDeltaSeconds * 60.0f)
                });
            }

            // Remove most sideways velocity to produce an arcade tire-grip
            // approximation until KAIRO has a native raycast-vehicle model.
            const float lateralSpeed =
                kairo::foundation::math::Dot(state.LinearVelocity, right);
            const float grip = std::clamp(fixedDeltaSeconds * 7.5f, 0.0f, 1.0f);
            state.LinearVelocity.x -= right.x * lateralSpeed * grip;
            state.LinearVelocity.z -= right.z * lateralSpeed * grip;

            if (m_Brake)
            {
                const float braking =
                    std::max(0.0f, 1.0f - fixedDeltaSeconds * 5.0f);
                state.LinearVelocity.x *= braking;
                state.LinearVelocity.z *= braking;
            }

            const float steeringAuthority =
                std::clamp(absoluteForwardSpeed * 0.035f + 0.30f, 0.30f, 1.85f);
            const float reverse = forwardSpeed < -0.25f ? -1.0f : 1.0f;
            state.AngularVelocity.y = m_Steering * steeringAuthority * reverse;

            // V1 is intentionally arcade-stable rather than a fake suspension model.
            state.AngularVelocity.x *= 0.15f;
            state.AngularVelocity.z *= 0.15f;
        }

    private:
        kairo::player::RuntimePhysicsBridge& m_Physics;
        Entity m_Car;
        float m_Steering = 0.0f;
        float m_Throttle = 0.0f;
        bool m_Brake = false;
    };

    void ResetCar(
        kairo::engine::Scene& scene,
        kairo::player::RuntimePhysicsBridge& physics,
        Entity car)
    {
        physics.SetEntityPosition(car, { -110.0, 0.75, 220.0 });
        const auto bodyID = physics.BodyFor(car);
        if (!bodyID)
            throw std::runtime_error("Cannot reset KAIRO Racing car without a physics body.");

        auto& state = physics.World().Bodies().at(*bodyID).State;
        state.Rotation = StartRotation();
        state.LinearVelocity = {};
        state.AngularVelocity = {};
        scene.Transform(car).Local.Rotation = StartRotation();
    }

    void UpdateRaceCamera(
        kairo::engine::Scene& scene,
        kairo::player::RuntimePhysicsBridge& physics,
        Entity car,
        Entity camera,
        bool overview)
    {
        const auto bodyID = physics.BodyFor(car);
        if (!bodyID) return;

        const auto& state = physics.World().Bodies().at(*bodyID).State;
        Vec3f forward = kairo::foundation::math::Forward(state.Rotation);
        forward.y = 0.0f;
        forward = kairo::foundation::math::SafeNormalize(
            forward, Vec3f::Forward());

        auto& cameraTransform = scene.Transform(camera).Local;
        const Vec3f target = {
            state.Position.x,
            state.Position.y + 0.8f,
            state.Position.z
        };

        if (overview)
        {
            cameraTransform.Translation = {
                state.Position.x,
                state.Position.y + 70.0f,
                state.Position.z + 28.0f
            };
        }
        else
        {
            cameraTransform.Translation = {
                state.Position.x - forward.x * 10.0f,
                state.Position.y + 4.5f,
                state.Position.z - forward.z * 10.0f
            };
        }

        cameraTransform.Rotation = kairo::foundation::math::LookRotation(
            target - cameraTransform.Translation, Vec3f::Up());
    }

    [[nodiscard]] Entity OtherEntity(
        const kairo::player::RuntimeContactEvent& contact, Entity car)
    {
        if (contact.EntityA == car) return contact.EntityB;
        if (contact.EntityB == car) return contact.EntityA;
        return {};
    }
}

int main(int argc, char** argv)
{
    try
    {
        const Arguments arguments = ParseArguments(argc, argv);
        kairo::player::RuntimeProject project(arguments.Project);
        auto& scene = project.Scene();

        const Entity track = RequireTagged(scene, "track");
        const Entity car = RequireTagged(scene, "player");
        const Entity camera = RequireTagged(scene, "race-camera");
        const Entity start = RequireTagged(scene, "start");
        const Entity checkpoint = RequireTagged(scene, "checkpoint");
        const Entity finish = RequireTagged(scene, "finish");

        if (!scene.HasRigidBody(car) || !scene.HasCollider(car))
            throw std::invalid_argument(
                "KAIRO Racing player car requires a rigid body and collider.");

        if (arguments.PackageProfile)
        {
            const auto package = kairo::player::PackageRuntimeProject(project, {
                *arguments.PackageProfile, argv[0], arguments.Replace });
            std::cout << "Packaged KAIRO Racing to " << package.OutputDirectory
                      << "\nManifest: " << package.ManifestPath << '\n';
            return 0;
        }

        kairo::player::RuntimePhysicsBridge physics(scene);
        kairo::player::RuntimeInputBridge input(project.InputMap());
        ArcadeCarDriver driver(physics, car);

        kairo::renderer::RendererRuntime renderer({
            project.Descriptor().Name, 1600u, 900u, true, arguments.Backend });
        kairo::player::RuntimeRenderBridge renderBridge(renderer, project);

        const auto initialRenderScene = renderBridge.BuildScene();
        const auto trackDraws = static_cast<std::size_t>(std::count_if(
            initialRenderScene.Draws().begin(),
            initialRenderScene.Draws().end(),
            [track](const kairo::renderer::MeshDraw& draw)
            {
                return draw.ObjectID == track.Value;
            }));
        if (trackDraws == 0u)
            throw std::runtime_error(
                "PMNDRS track imported but produced zero KAIRO render draws.");

        std::cout
            << "KAIRO Racing render extraction: "
            << initialRenderScene.Draws().size()
            << " total draws, "
            << trackDraws
            << " track draws, "
            << initialRenderScene.Lights().size()
            << " lights.\n";

        renderer.SubmitRenderScene(initialRenderScene);
        renderer.SetCameraPose(renderBridge.CameraPose());
        ResetCar(scene, physics, car);

        bool overviewCamera = false;
        bool checkpointPassed = false;
        std::uint32_t lap = 0u;
        auto lapStart = std::chrono::steady_clock::now();
        auto previousFrame = std::chrono::steady_clock::now();
        unsigned smokeFrames = 0u;

        if (arguments.Smoke) renderer.RequestViewportCapture();

        while (!renderer.NativeWindow().ShouldClose())
        {
            renderer.NativeWindow().PollEvents();
            input.Poll(renderer.NativeWindow());

            if (input.Action("Quit").Pressed)
                renderer.NativeWindow().RequestClose();

            const auto drive = input.Action("Drive").Value;
            driver.SetDrive(drive.X, drive.Y);
            driver.SetBrake(input.Action("Brake").Down);

            if (input.Action("Reset").Pressed)
            {
                ResetCar(scene, physics, car);
                checkpointPassed = false;
                lapStart = std::chrono::steady_clock::now();
            }

            if (input.Action("Camera").Pressed)
            {
                overviewCamera = !overviewCamera;
                std::cout
                    << "KAIRO Racing camera: "
                    << (overviewCamera ? "overview" : "chase")
                    << "\n";
            }

            const auto currentFrame = std::chrono::steady_clock::now();
            const float elapsedSeconds =
                std::chrono::duration<float>(currentFrame - previousFrame).count();
            previousFrame = currentFrame;

            (void)physics.Advance(elapsedSeconds, &driver);

            for (const auto& contact : physics.ContactEvents())
            {
                if (contact.Type !=
                    kairo::foundation::physics::PhysicsContactEventType::Begin)
                    continue;

                const Entity other = OtherEntity(contact, car);
                if (!other) continue;

                if (other == start)
                {
                    lapStart = std::chrono::steady_clock::now();
                    checkpointPassed = false;
                    std::cout << "KAIRO Racing: timer started.\n";
                }
                else if (other == checkpoint)
                {
                    checkpointPassed = true;
                    std::cout << "KAIRO Racing: checkpoint.\n";
                }
                else if (other == finish && checkpointPassed)
                {
                    ++lap;
                    const auto completed = std::chrono::steady_clock::now();
                    const double seconds =
                        std::chrono::duration<double>(completed - lapStart).count();
                    std::cout << "KAIRO Racing: lap " << lap << " completed in "
                              << std::fixed << std::setprecision(3)
                              << seconds << " seconds.\n";
                    checkpointPassed = false;
                    lapStart = completed;
                }
            }

            if (scene.WorldTransform(car).Translation.y < -20.0f)
                ResetCar(scene, physics, car);

            UpdateRaceCamera(scene, physics, car, camera, overviewCamera);
            renderer.SetCameraPose(renderBridge.CameraPose());
            renderer.SubmitRenderScene(renderBridge.BuildScene());

            std::ostringstream title;
            title << project.Descriptor().Name << " | "
                  << std::fixed << std::setprecision(0)
                  << driver.SpeedMetresPerSecond() * 3.6f << " km/h"
                  << " | Lap " << (lap + 1u)
                  << " | "
                  << (checkpointPassed ? "Checkpoint OK" : "Checkpoint pending")
                  << " | "
                  << (overviewCamera ? "Overview" : "Chase");
            glfwSetWindowTitle(
                renderer.NativeWindow().NativeHandle(), title.str().c_str());

            renderer.DrawFrame();

            if (arguments.Smoke)
            {
                if (const auto capture = renderer.TakeViewportCapture(); capture)
                {
                    if (!capture->IsVisuallyNonUniform())
                        throw std::runtime_error(
                            "KAIRO Racing smoke frame was blank or visually uniform.");
                    std::cout << "KAIRO Racing native smoke passed at "
                              << capture->Width << 'x' << capture->Height << ".\n";
                    return 0;
                }
                if (++smokeFrames > 24u)
                    throw std::runtime_error(
                        "KAIRO Racing smoke capture did not complete within 24 frames.");
            }
        }
        return 0;
    }
    catch (const kairo::renderer::PresentationUnavailableError& error)
    {
        std::cerr << "KAIRO Racing presentation unavailable: "
                  << error.what() << '\n';
        return 77;
    }
    catch (const std::exception& error)
    {
        std::cerr << "KAIRO Racing: " << error.what() << '\n';
        return 1;
    }
}
