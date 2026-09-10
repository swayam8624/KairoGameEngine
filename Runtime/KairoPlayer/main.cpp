#include <exception>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <stdexcept>
#include <string_view>

import Kairo.EngineCore.InputMap;
import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimeRenderBridge;
import Kairo.Player.RuntimeAudioBridge;
import Kairo.Player.RuntimeSceneAudioBridge;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeCharacterMotorBridge;
import Kairo.Player.RuntimePlayerControllerBridge;
import Kairo.Player.RuntimeInputBridge;
import Kairo.Player.RuntimeLogicBridge;
import Kairo.Player.RuntimeNativeGameplayBridge;
import Kairo.Player.RuntimeProductionSystemsBridge;
import Kairo.Player.RuntimeShippingBridge;
import Kairo.Player.RuntimePackaging;
import Kairo.Renderer;

namespace
{
    struct Arguments final
    {
        std::filesystem::path Project;
        bool ValidateOnly = false;
        bool SmokeTest = false;
        bool ReplacePackage = false;
        std::optional<std::string> PackageProfile;
        std::optional<kairo::renderer::GraphicsBackend> BackendOverride;
    };

    class RuntimeFixedStepFanout final : public kairo::player::RuntimeFixedStepListener
    {
    public:
        RuntimeFixedStepFanout(kairo::player::RuntimePlayerControllerBridge& playerController,
            kairo::player::RuntimeLogicBridge& logic,
            kairo::player::RuntimeNativeGameplayBridge& native)
            : m_PlayerController(playerController), m_Logic(logic), m_Native(native) {}

        void BeforePhysicsStep(float fixedDeltaSeconds) override
        {
            m_PlayerController.BeforePhysicsStep(fixedDeltaSeconds);
            m_Logic.BeforePhysicsStep(fixedDeltaSeconds);
            m_Native.BeforePhysicsStep(fixedDeltaSeconds);
        }

    private:
        kairo::player::RuntimePlayerControllerBridge& m_PlayerController;
        kairo::player::RuntimeLogicBridge& m_Logic;
        kairo::player::RuntimeNativeGameplayBridge& m_Native;
    };

    [[nodiscard]] Arguments ParseArguments(int count, char** values)
    {
        if (count < 2)
            throw std::invalid_argument(
                "Usage: KairoPlayer <Project.kproject> [--renderer auto|vulkan|metal|d3d12|opengl] "
                "[--validate|--smoke|--package <profile> [--replace]]");
        Arguments result;
        result.Project = values[1];
        for (int index = 2; index < count; ++index)
        {
            const std::string_view option = values[index];
            if (option == "--validate") result.ValidateOnly = true;
            else if (option == "--smoke") result.SmokeTest = true;
            else if (option == "--package")
            {
                if (result.PackageProfile.has_value())
                    throw std::invalid_argument("--package may be specified only once.");
                if (++index >= count) throw std::invalid_argument("--package requires an exact build-profile name.");
                result.PackageProfile = values[index];
            }
            else if (option == "--replace") result.ReplacePackage = true;
            else if (option == "--renderer")
            {
                if (++index >= count)
                    throw std::invalid_argument(
                        "--renderer requires auto, vulkan, metal, d3d12, or opengl.");
                result.BackendOverride =
                    kairo::renderer::ParseGraphicsBackend(values[index]);
            }
            else throw std::invalid_argument("Unknown KairoPlayer option: " + std::string(option));
        }
        const unsigned operationCount = static_cast<unsigned>(result.ValidateOnly) +
            static_cast<unsigned>(result.SmokeTest) + static_cast<unsigned>(result.PackageProfile.has_value());
        if (operationCount > 1u)
            throw std::invalid_argument("--validate, --smoke, and --package are mutually exclusive operations.");
        if (result.ReplacePackage && !result.PackageProfile.has_value())
            throw std::invalid_argument("--replace is valid only with --package.");
        return result;
    }
}

int main(int argc, char** argv)
{
    try
    {
        const Arguments arguments = ParseArguments(argc, argv);
        kairo::player::RuntimeProject project(arguments.Project);
        std::cout << "Loaded " << project.Descriptor().Name << "\n"
                  << "  assets: " << project.Assets().Size() << "\n"
                  << "  entities: " << project.Scene().Size() << "\n"
                  << "  startup scene: " << project.Descriptor().StartupScene.generic_string() << '\n';
        kairo::player::RuntimePhysicsBridge physics(project.Scene());
        kairo::player::RuntimeAudioBridge audio(project);
        kairo::player::RuntimeSceneAudioBridge sceneAudio(project.Scene(), audio);
        kairo::player::RuntimeInputBridge input(project.InputMap());
        kairo::player::RuntimeCharacterMotorBridge characterMotor(project.Scene(), physics);
        kairo::player::RuntimePlayerControllerBridge playerController(
            project.Scene(), characterMotor);
        if (playerController.ControllerCount() != 0u && !input.HasAction("Move"))
            throw std::invalid_argument(
                "A kairo.player-controller entity requires a project input action named 'Move'.");
        kairo::player::RuntimeLogicBridge logic(project, physics);
        kairo::player::RuntimeNativeGameplayBridge nativeGameplay(
            project, kairo::player::PlayerNativeGameplayRegistry());
        kairo::player::RuntimeProductionSystemsBridge production(project);
        kairo::player::RuntimeShippingBridge shipping(project);
        std::cout << "  audio clips: " << audio.LoadedClipCount() << '\n'
                  << "  player controllers: " << playerController.ControllerCount() << '\n'
                  << "  native behaviours: " << nativeGameplay.InstanceCount() << '\n'
                  << "  production systems: " << (production.Enabled() ? "enabled" : "disabled") << '\n';
        if (arguments.ValidateOnly) return 0;
        if (arguments.PackageProfile.has_value())
        {
            const auto package = kairo::player::PackageRuntimeProject(project,
                { *arguments.PackageProfile, argv[0], arguments.ReplacePackage });
            std::cout << "Packaged " << package.ProjectFileCount << " project files ("
                      << package.ProjectByteCount << " bytes) to "
                      << package.OutputDirectory << '\n';
            return 0;
        }

        const kairo::renderer::GraphicsBackend backend =
            arguments.BackendOverride.value_or(
                kairo::renderer::ParseGraphicsBackend(
                    project.Descriptor().GraphicsBackend));
        kairo::renderer::RendererRuntime renderer({
            project.Descriptor().Name + " - KairoPlayer", 1280u, 720u, true,
            backend });
        kairo::player::RuntimeRenderBridge bridge(renderer, project);
        RuntimeFixedStepFanout fixedSteps(playerController, logic, nativeGameplay);
        logic.BeginPlay();
        nativeGameplay.BeginPlay();
        sceneAudio.BeginPlay();
        renderer.SubmitRenderScene(bridge.BuildScene());
        renderer.SetCameraPose(bridge.CameraPose());
        if (arguments.SmokeTest) renderer.RequestViewportCapture();
        unsigned smokeFrames = 0u;
        auto previousFrame = std::chrono::steady_clock::now();
        while (!renderer.NativeWindow().ShouldClose())
        {
            renderer.NativeWindow().PollEvents();
            input.Poll(renderer.NativeWindow());
            if (input.HasAction("Quit") && input.Action("Quit").Pressed)
                renderer.NativeWindow().RequestClose();

            if (playerController.ControllerCount() != 0u)
            {
                const auto jump = input.HasAction("Jump")
                    ? input.Action("Jump") : kairo::engine::InputActionState{};
                playerController.CaptureInput(input.Action("Move"), jump);
            }
            for (const auto& action : project.InputMap().Actions())
                logic.DispatchInput(action.Name, input.Action(action.Name));
            const auto currentFrame = std::chrono::steady_clock::now();
            const float elapsedSeconds = std::chrono::duration<float>(currentFrame - previousFrame).count();
            previousFrame = currentFrame;
            (void)physics.Advance(elapsedSeconds, &fixedSteps);
            logic.DispatchContacts();
            nativeGameplay.Update(static_cast<double>(elapsedSeconds));
            production.Step(static_cast<double>(elapsedSeconds));
            (void)shipping.Step(static_cast<double>(elapsedSeconds));
            bridge.StepAnimations(elapsedSeconds);

            // This is currently the deterministic/headless output sink: it keeps
            // authored voices, one-shots and spatial state advancing today while
            // preserving the returned stereo PCM boundary for native device output.
            (void)sceneAudio.Advance(static_cast<double>(elapsedSeconds));

            renderer.SubmitRenderScene(bridge.BuildScene());
            renderer.SetCameraPose(bridge.CameraPose());
            renderer.DrawFrame();
            if (arguments.SmokeTest)
            {
                if (const auto capture = renderer.TakeViewportCapture(); capture.has_value())
                {
                    if (!capture->IsVisuallyNonUniform())
                        throw std::runtime_error("Native smoke capture is blank or visually uniform.");
                    std::cout << "Native viewport smoke passed at " << capture->Width
                              << 'x' << capture->Height << ".\n";
                    sceneAudio.EndPlay();
                    nativeGameplay.EndPlay();
                    return 0;
                }
                if (++smokeFrames > 16u)
                    throw std::runtime_error("Native smoke capture did not complete within 16 frames.");
            }
        }
        sceneAudio.EndPlay();
        nativeGameplay.EndPlay();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "KairoPlayer: " << error.what() << '\n';
        return 1;
    }
}
