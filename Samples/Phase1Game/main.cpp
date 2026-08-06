#include <GLFW/glfw3.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Foundation.PhysicsEngine;
import Kairo.Player.RuntimeInputBridge;
import Kairo.Player.RuntimePackaging;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimeRenderBridge;
import Kairo.Renderer;
import Kairo.Sample.Phase1GameState;

namespace
{
    using kairo::engine::Entity;
    using kairo::foundation::math::Vec3d;
    using kairo::foundation::math::Vec3f;

    struct Arguments final
    {
        std::filesystem::path Project = KAIRO_PHASE1_PROJECT_PATH;
        std::optional<std::string> PackageProfile;
        bool Replace = false;
        bool Smoke = false;
    };

    [[nodiscard]] Arguments ParseArguments(int argc, char** argv)
    {
        Arguments result;
        bool projectSeen = false;
        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument = argv[index];
            if (argument == "--smoke") result.Smoke = true;
            else if (argument == "--replace") result.Replace = true;
            else if (argument == "--package")
            {
                if (++index >= argc) throw std::invalid_argument("--package requires a build-profile name.");
                result.PackageProfile = argv[index];
            }
            else if (!argument.empty() && argument.front() == '-')
                throw std::invalid_argument("Unknown Kairo Phase 1 option: " + std::string(argument));
            else if (projectSeen)
                throw std::invalid_argument("Kairo Phase 1 accepts at most one project path.");
            else
            {
                result.Project = argv[index];
                projectSeen = true;
            }
        }
        if (result.Replace && !result.PackageProfile.has_value())
            throw std::invalid_argument("--replace is valid only with --package.");
        if (result.Smoke && result.PackageProfile.has_value())
            throw std::invalid_argument("--smoke and --package are mutually exclusive.");
        return result;
    }

    [[nodiscard]] std::vector<Entity> Tagged(
        const kairo::engine::Scene& scene, std::string_view tag)
    {
        std::vector<Entity> result;
        for (const Entity entity : scene.Entities())
            if (scene.HasTag(entity, tag)) result.push_back(entity);
        return result;
    }

    [[nodiscard]] Entity RequireSingleTagged(
        const kairo::engine::Scene& scene, std::string_view tag)
    {
        const auto entities = Tagged(scene, tag);
        if (entities.size() != 1u)
            throw std::invalid_argument("Phase 1 project requires exactly one entity tagged '" +
                std::string(tag) + "'.");
        return entities.front();
    }

    [[nodiscard]] Vec3f WorldPosition(const kairo::engine::Scene& scene, Entity entity)
    {
        return scene.WorldTransform(entity).Translation;
    }

    [[nodiscard]] Vec3d ToDouble(const Vec3f& value) noexcept
    {
        return { static_cast<double>(value.x), static_cast<double>(value.y),
            static_cast<double>(value.z) };
    }

    void ClearVelocity(kairo::player::RuntimePhysicsBridge& physics, Entity entity)
    {
        if (const auto body = physics.BodyFor(entity); body.has_value())
        {
            auto& state = physics.World().Bodies().at(*body).State;
            state.LinearVelocity = {};
            state.AngularVelocity = {};
        }
    }

    class PlayerDriver final : public kairo::player::RuntimeFixedStepListener
    {
    public:
        PlayerDriver(kairo::player::RuntimePhysicsBridge& physics, Entity player)
            : m_Physics(physics), m_Player(player) {}

        void SetEnabled(bool enabled) noexcept { m_Enabled = enabled; }
        void SetMove(float x, float y) noexcept { m_MoveX = x; m_MoveY = y; }
        void RequestJump() noexcept { m_JumpRequested = true; }

        void BeforePhysicsStep(float fixedDeltaSeconds) override
        {
            if (!m_Enabled)
            {
                m_JumpRequested = false;
                return;
            }
            const Vec3d movement{
                static_cast<double>(m_MoveX * 17.0f * fixedDeltaSeconds),
                0.0,
                static_cast<double>(-m_MoveY * 17.0f * fixedDeltaSeconds) };
            if (std::abs(m_MoveX) > 1.0e-5f || std::abs(m_MoveY) > 1.0e-5f)
                m_Physics.ApplyEntityImpulse(m_Player, movement);
            if (m_JumpRequested)
                m_Physics.ApplyEntityImpulse(m_Player, { 0.0, 2.5, 0.0 });
            m_JumpRequested = false;
        }

    private:
        kairo::player::RuntimePhysicsBridge& m_Physics;
        Entity m_Player;
        float m_MoveX = 0.0f;
        float m_MoveY = 0.0f;
        bool m_Enabled = false;
        bool m_JumpRequested = false;
    };

    void SaveSnapshot(const std::filesystem::path& path,
        const kairo::sample::Phase1Snapshot& snapshot)
    {
        const auto parent = path.parent_path();
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        if (error) throw std::runtime_error("Cannot create Phase 1 save directory: " + error.message());
        const std::filesystem::path temporary = path.string() + ".tmp";
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) throw std::runtime_error("Cannot open Phase 1 temporary save file.");
            const std::string data = kairo::sample::SerializePhase1Snapshot(snapshot);
            output.write(data.data(), static_cast<std::streamsize>(data.size()));
            output.flush();
            if (!output) throw std::runtime_error("Cannot write complete Phase 1 save file.");
        }
        kairo::assets::ReplaceFileAtomically(temporary, path);
    }

    [[nodiscard]] kairo::sample::Phase1Snapshot LoadSnapshot(
        const std::filesystem::path& path)
    {
        std::error_code error;
        const auto bytes = std::filesystem::file_size(path, error);
        if (error || bytes > 4u * 1024u * 1024u)
            throw std::runtime_error("Cannot inspect Phase 1 save file.");
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot open Phase 1 save file.");
        std::string data(static_cast<std::size_t>(bytes), '\0');
        if (!data.empty() && !input.read(data.data(), static_cast<std::streamsize>(data.size())))
            throw std::runtime_error("Cannot read complete Phase 1 save file.");
        return kairo::sample::ParsePhase1Snapshot(data);
    }
}

int main(int argc, char** argv)
{
    try
    {
        const Arguments arguments = ParseArguments(argc, argv);
        kairo::player::RuntimeProject project(arguments.Project);
        auto& scene = project.Scene();
        const Entity player = RequireSingleTagged(scene, "player");
        const Entity goal = RequireSingleTagged(scene, "goal");
        const auto collectibles = Tagged(scene, "collectible");
        const auto hazards = Tagged(scene, "hazard");
        if (collectibles.empty())
            throw std::invalid_argument("Phase 1 project requires at least one collectible.");
        if (hazards.empty())
            throw std::invalid_argument("Phase 1 project requires at least one hazard.");
        if (!scene.HasRigidBody(player) || !scene.HasCollider(player))
            throw std::invalid_argument("The Phase 1 player requires rigid-body and collider components.");

        if (arguments.PackageProfile.has_value())
        {
            const auto package = kairo::player::PackageRuntimeProject(project, {
                *arguments.PackageProfile, argv[0], arguments.Replace });
            std::cout << "Packaged Kairo Phase 1 to " << package.OutputDirectory << "\nManifest: "
                      << package.ManifestPath << '\n';
            return 0;
        }

        kairo::sample::Phase1GameState state(collectibles);
        kairo::player::RuntimePhysicsBridge physics(scene);
        kairo::player::RuntimeInputBridge input(project.InputMap());
        PlayerDriver driver(physics, player);

        std::unordered_map<std::uint32_t, Vec3f> initialPositions;
        initialPositions.emplace(player.Value, WorldPosition(scene, player));
        for (const Entity entity : collectibles)
            initialPositions.emplace(entity.Value, WorldPosition(scene, entity));

        const auto hiddenPosition = [](Entity entity) {
            return Vec3d{ 0.0, -50.0 - static_cast<double>(entity.Value), 0.0 };
        };
        const auto restoreWorld = [&]
        {
            physics.SetEntityPosition(player, ToDouble(initialPositions.at(player.Value)));
            ClearVelocity(physics, player);
            for (const Entity entity : collectibles)
                physics.SetEntityPosition(entity, ToDouble(initialPositions.at(entity.Value)));
        };
        const auto applySnapshot = [&](const kairo::sample::Phase1Snapshot& snapshot)
        {
            state.Restore(snapshot);
            physics.SetEntityPosition(player, ToDouble(snapshot.PlayerPosition));
            ClearVelocity(physics, player);
            for (const Entity entity : collectibles)
            {
                if (state.IsCollected(entity)) physics.SetEntityPosition(entity, hiddenPosition(entity));
                else physics.SetEntityPosition(entity, ToDouble(initialPositions.at(entity.Value)));
            }
        };
        const auto reset = [&]
        {
            state.Reset();
            restoreWorld();
        };

        const std::filesystem::path savePath = project.Root() / ".kairo" / "saves" / "phase1.ksave";
        kairo::renderer::RendererRuntime renderer({
            project.Descriptor().Name + " - Phase 1", 1280u, 720u, true });
        kairo::player::RuntimeRenderBridge renderBridge(renderer, project);
        renderer.SubmitRenderScene(renderBridge.BuildScene());
        renderer.SetCameraPose(renderBridge.CameraPose());
        if (arguments.Smoke)
        {
            state.Start();
            renderer.RequestViewportCapture();
        }

        unsigned smokeFrames = 0u;
        auto previousFrame = std::chrono::steady_clock::now();
        while (!renderer.NativeWindow().ShouldClose())
        {
            renderer.NativeWindow().PollEvents();
            input.Poll(renderer.NativeWindow());
            if (input.Action("Quit").Pressed) renderer.NativeWindow().RequestClose();

            if (input.Action("Start").Pressed)
            {
                if (state.Mode() == kairo::sample::Phase1Mode::Won ||
                    state.Mode() == kairo::sample::Phase1Mode::Lost)
                    reset();
                state.Start();
            }
            if (input.Action("Pause").Pressed) state.TogglePause();
            if (input.Action("Reset").Pressed) reset();
            if (input.Action("Save").Pressed)
            {
                SaveSnapshot(savePath, state.Capture(WorldPosition(scene, player)));
                std::cout << "Saved Phase 1 progress to " << savePath << '\n';
            }
            if (input.Action("Load").Pressed)
            {
                applySnapshot(LoadSnapshot(savePath));
                std::cout << "Loaded Phase 1 progress from " << savePath << '\n';
            }

            const auto move = input.Action("Move").Value;
            driver.SetEnabled(state.IsRunning());
            driver.SetMove(move.X, move.Y);
            if (state.IsRunning() && input.Action("Jump").Pressed) driver.RequestJump();

            const auto currentFrame = std::chrono::steady_clock::now();
            const float elapsedSeconds = std::chrono::duration<float>(
                currentFrame - previousFrame).count();
            previousFrame = currentFrame;

            if (state.IsRunning())
            {
                (void)physics.Advance(elapsedSeconds, &driver);
                for (const auto& contact : physics.ContactEvents())
                {
                    if (contact.Type != kairo::foundation::physics::PhysicsContactEventType::Begin)
                        continue;
                    Entity other;
                    if (contact.EntityA == player) other = contact.EntityB;
                    else if (contact.EntityB == player) other = contact.EntityA;
                    else continue;

                    if (scene.HasTag(other, "collectible") && state.Collect(other))
                    {
                        physics.SetEntityPosition(other, hiddenPosition(other));
                        std::cout << "Collected " << state.CollectedCount() << "/"
                                  << state.CollectibleCount() << '\n';
                    }
                    else if (other == goal && state.ReachGoal())
                        std::cout << "Phase 1 complete.\n";
                    else if (scene.HasTag(other, "hazard"))
                        state.Lose();
                }
                if (WorldPosition(scene, player).y < -5.0f) state.Lose();
            }

            const std::string title = project.Descriptor().Name + " | " + state.StatusLine();
            glfwSetWindowTitle(renderer.NativeWindow().NativeHandle(), title.c_str());
            renderer.SubmitRenderScene(renderBridge.BuildScene());
            renderer.DrawFrame();

            if (arguments.Smoke)
            {
                if (const auto capture = renderer.TakeViewportCapture(); capture.has_value())
                {
                    if (!capture->IsVisuallyNonUniform())
                        throw std::runtime_error("Phase 1 smoke capture is blank or visually uniform.");
                    std::cout << "Phase 1 native smoke passed at " << capture->Width
                              << 'x' << capture->Height << ".\n";
                    return 0;
                }
                if (++smokeFrames > 16u)
                    throw std::runtime_error("Phase 1 smoke capture did not complete within 16 frames.");
            }
        }
        return 0;
    }
    catch (const kairo::renderer::PresentationUnavailableError& error)
    {
        std::cerr << "Kairo Phase 1 native presentation unavailable: " << error.what() << '\n';
        return 77;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Kairo Phase 1: " << error.what() << '\n';
        return 1;
    }
}
