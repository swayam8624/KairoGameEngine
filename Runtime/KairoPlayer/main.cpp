// TEACHING MAP: Player is the composition root, not a monolithic engine.
// Parse CLI and validate portable RuntimeProject before native device creation;
// then construct physics, audio, input, gameplay/logic and RendererRuntime
// adapters from the same authoritative EngineCore scene.
//
// Frame order: poll OS/window input, capture named actions, advance bounded
// fixed physics substeps (controller/logic hooks), deliver contact events,
// update gameplay/animation/audio, extract RenderScene, submit camera and
// draw the native frame. RuntimePhysicsBridge maps authored Entity to BodyID;
// RealtimeSceneBridge maps AssetID to transient renderer handles.
//
// --validate-only skips native window creation. --smoke requests a bounded
// capture and tests nonuniform pixels, not complete graphics correctness or
// mathematical certification of physics.

#include "RuntimeFrameClock.hpp"
#include "RuntimeCollectibleRun.hpp"
#include "RuntimeProximityTriggers.hpp"
#include "CraftSaveFile.hpp"
#include <algorithm>
#include <vector>
#include "RuntimeNativeAudioOutput.hpp"
#include <memory>
#include <exception>
#include <thread>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <stdexcept>
#include <string_view>

import Kairo.EngineCore.InputMap;
import Kairo.EngineCore.Entity;
import Kairo.Foundation.Math;
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
import Kairo.Player.RuntimeMenuHost;
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

// Calls the same fixed time step on controllers, logic and native gameplay hooks; timing must
// agree with the physics bridge.
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

// Build runtime services from one validated project. The exception
// boundary reports errors; each owner remains responsible for RAII cleanup.
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
        std::unique_ptr<kairo::player::detail::RuntimeNativeAudioOutput> audioOutput;
        if(!arguments.SmokeTest&&audio.LoadedClipCount()!=0)
        {
            try
            {
                audioOutput=std::make_unique<kairo::player::detail::RuntimeNativeAudioOutput>(audio.OutputSampleRate());
                if(!audioOutput->Enabled())
                {
                    std::cerr<<"Native audio output unavailable on this host; using headless mixer.\n";
                    audioOutput.reset();
                }
            }
            catch(const std::exception& error)
            {
                std::cerr<<"Native audio output unavailable: "<<error.what()<<"; using headless mixer.\n";
            }
        }
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
        // Opt-in objectives use ordinary authored visual leaf entities. The
        // narrow contract does not disable live physics or logic objects.
        kairo::player::detail::RuntimeCollectibleRun collectibleRun;
        for(const auto entity:project.Scene().Entities())
            if(project.Scene().IsActiveInHierarchy(entity)&&project.Scene().HasTag(entity,"kairo.collectible"))
            {
                if(project.Scene().HasRigidBody(entity)||project.Scene().HasCollider(entity)||
                    project.Scene().HasLogic(entity)||!project.Scene().Children(entity).empty()||
                    (!project.Scene().HasMeshRenderer(entity)&&!project.Scene().HasSceneInstance(entity)))
                    throw std::invalid_argument("Collectibles must be leaf visual-only entities.");
                const auto p=project.Scene().WorldTransform(entity).Translation;
                collectibleRun.Add(entity.Value,{p.x,p.y,p.z});
            }
        if(collectibleRun.Total()&&!playerController.PrimaryController())
            throw std::invalid_argument("Collectible objective requires a player controller.");
        if(collectibleRun.Total())
            std::cout<<"  collectible goals: "<<collectibleRun.Total()<<'\n';
        // An optional data-only trigger document binds to explicitly opted-in
        // static visual root leaves. No physics bodies or gameplay owners are
        // silently enabled/disabled by the visual trigger adapter.
        std::optional<kairo::player::detail::RuntimeProximityTriggers> triggers;
        if(std::filesystem::exists(project.Root()/"Config/Triggers.ktrigger"))
        {
            const auto path=project.RequireProjectFile("Config/Triggers.ktrigger","trigger document");
            const auto bytes=kairo::player::detail::ReadCraftSaveFile(path,kairo::player::detail::MaximumTriggerBytes);
            const std::string document(bytes.begin(),bytes.end());
            const auto rules=kairo::player::detail::ParseTriggerRules(document);
            for(const auto& rule:rules)
            {
                const kairo::engine::Entity entity{rule.Target};
                const auto& scene=project.Scene();
                if(!scene.Contains(entity)||!scene.HasTag(entity,"kairo.trigger-visual")||
                    scene.HasTag(entity,"kairo.collectible")||scene.HasTag(entity,"kairo.player-controller")||
                    scene.Parent(entity)||!scene.Children(entity).empty()||
                    scene.HasRigidBody(entity)||scene.HasCollider(entity)||scene.HasLogic(entity)||
                    scene.HasCamera(entity)||scene.HasLight(entity)||scene.HasEnvironment(entity)||
                    scene.HasAudioEmitter(entity)||scene.HasAudioListener(entity)||
                    (!scene.HasMeshRenderer(entity)&&!scene.HasSceneInstance(entity)))
                    throw std::invalid_argument("Trigger target must be an opted-in visual-only root leaf.");
            }
            triggers.emplace(document);
            if(triggers->Size()&&!playerController.PrimaryController())
                throw std::invalid_argument("Proximity triggers require a player controller.");
            std::cout<<"  proximity triggers: "<<triggers->Size()<<'\n';
        }
        // Headless validation stops before graphics initialization.
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
        // An authored menu is optional. The same shipping scene is used by
        // the native host for layout, labels and command activation.
        std::unique_ptr<kairo::player::RuntimeMenuHost> menu;
        if(auto* ui=shipping.UI())
            menu=std::make_unique<kairo::player::RuntimeMenuHost>(*ui,renderer,project.Descriptor().Name+" - KairoPlayer");

        // Camera follow is an authored opt-in, not a hidden global behaviour.
        // An ordinary imported or hand-authored camera remains completely
        // under the game's control unless tagged "kairo.player-follow-camera".
        const auto controlledEntity = playerController.PrimaryController();
        const auto primaryCamera = project.Scene().PrimaryCamera();
        const bool followPlayer = controlledEntity.has_value() &&
            primaryCamera.has_value() &&
            project.Scene().HasTag(*primaryCamera, "kairo.player-follow-camera");
        const auto followAnchor = controlledEntity.has_value()
            ? project.Scene().WorldTransform(*controlledEntity).Translation
            : kairo::foundation::math::Vec3f{};
        const auto activeCameraPose = [&]()
        {
            auto pose = bridge.CameraPose();
            if (followPlayer)
            {
                const auto displacement = playerController.CameraFollowDelta(followAnchor);
                pose.Position += displacement;
                pose.Target += displacement;
            }
            return pose;
        };

        RuntimeFixedStepFanout fixedSteps(playerController, logic, nativeGameplay);
        logic.BeginPlay();
        nativeGameplay.BeginPlay();
        sceneAudio.BeginPlay();
        renderer.SubmitRenderScene(bridge.BuildScene());
        renderer.SetCameraPose(activeCameraPose());
        bool captureRequested=false;
        std::uint32_t viewportWidth=0,viewportHeight=0;
        unsigned smokeFrames = 0u;
        std::size_t publishedScore=static_cast<std::size_t>(-1);
        unsigned overlayWidth=0,overlayHeight=0;
        const auto clockOrigin=std::chrono::steady_clock::now();
        kairo::player::detail::RuntimeFrameClock frameClock;
        while (!renderer.NativeWindow().ShouldClose())
        {
            renderer.NativeWindow().PollEvents();
            if(renderer.NativeWindow().ShouldClose())break;
            const auto [frameWidth,frameHeight]=renderer.NativeWindow().FramebufferExtent();
            const bool drawable=frameWidth!=0&&frameHeight!=0;
            const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-clockOrigin).count();
            // The offscreen target must match physical framebuffer pixels,
            // including Retina scale and resize, before menu hit/draw layout.
            if(drawable&&(frameWidth!=viewportWidth||frameHeight!=viewportHeight))
            {
                renderer.ResizeViewport(frameWidth,frameHeight);
                viewportWidth=frameWidth;viewportHeight=frameHeight;
            }
            const bool menuFrame=menu&&menu->Poll(frameWidth,frameHeight);
            // The modal menu exclusively owns the overlay until it closes.
            // Invalidate the HUD cache so the next gameplay frame restores it.
            if(menuFrame)publishedScore=static_cast<std::size_t>(-1);
            // Menu owns this frame, including its closing action. Neutralize
            // held controls and queued jump so the next game frame cannot
            // replay menu input or a pre-menu pending physics event.
            input.Poll(renderer.NativeWindow(),!menuFrame);
            if(menuFrame)playerController.ClearInput();
            const float elapsedSeconds=static_cast<float>(frameClock.Sample(elapsed,drawable&&!menuFrame));
            if(!drawable)
            {
                if(arguments.SmokeTest)throw std::runtime_error("Native smoke window has no drawable framebuffer.");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            // Menu frames pause simulation, and activation cannot leak into
            // the gameplay frame. Both branches use the common draw/capture.
            if(!menuFrame)
            {
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
            (void)physics.Advance(elapsedSeconds, &fixedSteps);
            logic.DispatchContacts();
            nativeGameplay.Update(static_cast<double>(elapsedSeconds));
            production.Step(static_cast<double>(elapsedSeconds));
            (void)shipping.Step(static_cast<double>(elapsedSeconds));
            bridge.StepAnimations(elapsedSeconds);
            // Menu frames never advance proximity state. Apply explicit
            // show/hide events in ascending rule order before scene extraction.
            if(triggers&&controlledEntity&&project.Scene().Contains(*controlledEntity))
            {
                const auto p=project.Scene().WorldTransform(*controlledEntity).Translation;
                for(const auto& event:triggers->Step({p.x,p.y,p.z}))
                {
                    const kairo::engine::Entity target{event.Target};
                    if(!project.Scene().Contains(target))continue;
                    project.Scene().SetEnabled(target,event.Enable);
                    std::cout<<"Trigger "<<event.ID<<": "<<(event.Enable?"show ":"hide ")<<event.Target<<'\n';
                }
            }
            if(collectibleRun.Total()&&controlledEntity&&project.Scene().Contains(*controlledEntity))
            {
                const auto p=project.Scene().WorldTransform(*controlledEntity).Translation;
                const auto collected=collectibleRun.Step({p.x,p.y,p.z},elapsedSeconds);
                for(auto id:collected)project.Scene().SetEnabled(kairo::engine::Entity{id},false);
                if(!collected.empty())
                {
                    std::cout<<"Beacons "<<collectibleRun.Score()<<"/"<<collectibleRun.Total()<<'\n';
                    if(collectibleRun.Won())std::cout<<"All beacons collected in "<<collectibleRun.Elapsed()<<" seconds!\n";
                }
                // The menu branch never executes this HUD publication.
                if(renderer.SupportsViewportOverlay()&&
                    (publishedScore!=collectibleRun.Score()||overlayWidth!=frameWidth||overlayHeight!=frameHeight))
                {
                    const auto width=std::min(300u,frameWidth/2u);
                    std::vector<kairo::renderer::ViewportOverlayRect> rects;
                    if(frameWidth>=24u&&width&&frameHeight>36u)
                    {
                        rects.push_back({12u,12u,width,16u,{.08f,.12f,.18f}});
                        const auto progress=static_cast<unsigned>(width*collectibleRun.Score()/collectibleRun.Total());
                        if(progress)rects.push_back({12u,12u,progress,16u,{.18f,.85f,.45f}});
                    }
                    renderer.SetViewportOverlay(rects);
                    publishedScore=collectibleRun.Score();overlayWidth=frameWidth;overlayHeight=frameHeight;
                }
            }

            // The game thread owns mixing and scene state. The native callback
            // consumes only bounded stereo PCM; it never touches the scene.
            const auto pcm=sceneAudio.Advance(static_cast<double>(elapsedSeconds));
            if(audioOutput)
            {
                if(audioOutput->Failed())
                {
                    std::cerr<<"Native audio callback failed; continuing with headless mixer.\n";
                    audioOutput.reset();
                }
                else (void)audioOutput->Push(pcm);
            }

            }
            renderer.SubmitRenderScene(bridge.BuildScene());
            renderer.SetCameraPose(activeCameraPose());
            if(arguments.SmokeTest&&!captureRequested)
            {
                renderer.RequestViewportCapture();captureRequested=true;
            }
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
        if(arguments.SmokeTest)throw std::runtime_error("Native smoke closed before capture.");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "KairoPlayer: " << error.what() << '\n';
        return 1;
    }
}
