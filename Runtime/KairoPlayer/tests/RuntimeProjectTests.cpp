#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

import Kairo.Player.RuntimeProject;

namespace
{
    void Write(const std::filesystem::path& path, const char* text)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << text;
        if (!output) throw std::runtime_error("Fixture write failed.");
    }

    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() / "kairo-player-runtime-tests";
    std::filesystem::remove_all(root);
    try
    {
        Write(root / "Project.kproject",
            "kairo-project 2\nname \"Runtime Test\"\nengine-version \"0.1.0\"\n"
            "assets \"Assets.kassets\"\nstartup-scene \"Scenes/Main.kscene\"\n"
            "input-map \"Config/Input.kinput\"\nrendering-profile \"desktop\"\n"
            "build-profile \"Development\" development \"Build/Development\"\n");
        Write(root / "Assets.kassets", "kairo-assets 1\n");
        Write(root / "Scenes/Main.kscene",
            "kairo-scene 2\nentity 1 \"Root\"\nenabled true\nlayer 0\n"
            "transform 0 0 0 0 0 0 1 1 1 1\nend\n");

        kairo::player::RuntimeProject project(root / "Project.kproject");
        Require(project.Descriptor().Name == "Runtime Test", "Project descriptor did not load.");
        Require(project.Assets().Size() == 0u, "Empty asset manifest changed during load.");
        Require(project.Scene().Size() == 1u, "Startup scene did not load.");

        std::filesystem::remove(root / "Scenes/Main.kscene");
        bool missingRejected = false;
        try { kairo::player::RuntimeProject invalid(root / "Project.kproject"); }
        catch (const std::invalid_argument&) { missingRejected = true; }
        Require(missingRejected, "Missing startup scene was accepted.");

        bool extensionRejected = false;
        Write(root / "Project.txt", "not a project\n");
        try { (void)kairo::player::ResolveProjectDescriptorPath(root / "Project.txt"); }
        catch (const std::invalid_argument&) { extensionRejected = true; }
        Require(extensionRejected, "Non-kproject descriptor was accepted.");
        std::filesystem::remove_all(root);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::filesystem::remove_all(root);
        std::cerr << "KairoPlayer runtime test: " << error.what() << '\n';
        return 1;
    }
}
