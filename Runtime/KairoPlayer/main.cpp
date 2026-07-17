#include <exception>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimeRenderBridge;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Renderer;

namespace
{
    struct Arguments final
    {
        std::filesystem::path Project;
        bool ValidateOnly = false;
        bool SmokeTest = false;
    };

    [[nodiscard]] Arguments ParseArguments(int count, char** values)
    {
        if (count < 2) throw std::invalid_argument("Usage: KairoPlayer <Project.kproject> [--validate|--smoke]");
        Arguments result;
        result.Project = values[1];
        for (int index = 2; index < count; ++index)
        {
            const std::string_view option = values[index];
            if (option == "--validate") result.ValidateOnly = true;
            else if (option == "--smoke") result.SmokeTest = true;
            else throw std::invalid_argument("Unknown KairoPlayer option: " + std::string(option));
        }
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
        if (arguments.ValidateOnly) return 0;

        kairo::renderer::RendererRuntime renderer({
            project.Descriptor().Name + " - KairoPlayer", 1280u, 720u, true });
        kairo::player::RuntimeRenderBridge bridge(renderer, project);
        kairo::player::RuntimePhysicsBridge physics(project.Scene());
        renderer.SubmitRenderScene(bridge.BuildScene());
        renderer.SetCameraPose(bridge.CameraPose());
        if (arguments.SmokeTest) renderer.RequestViewportCapture();
        unsigned smokeFrames = 0u;
        auto previousFrame = std::chrono::steady_clock::now();
        while (!renderer.NativeWindow().ShouldClose())
        {
            renderer.NativeWindow().PollEvents();
            const auto currentFrame = std::chrono::steady_clock::now();
            const float elapsedSeconds = std::chrono::duration<float>(currentFrame - previousFrame).count();
            previousFrame = currentFrame;
            (void)physics.Advance(elapsedSeconds);
            renderer.SubmitRenderScene(bridge.BuildScene());
            renderer.DrawFrame();
            if (arguments.SmokeTest)
            {
                if (const auto capture = renderer.TakeViewportCapture(); capture.has_value())
                {
                    if (!capture->IsVisuallyNonUniform())
                        throw std::runtime_error("Native smoke capture is blank or visually uniform.");
                    std::cout << "Native viewport smoke passed at " << capture->Width
                              << 'x' << capture->Height << ".\n";
                    return 0;
                }
                if (++smokeFrames > 16u)
                    throw std::runtime_error("Native smoke capture did not complete within 16 frames.");
            }
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "KairoPlayer: " << error.what() << '\n';
        return 1;
    }
}
