#include <filesystem>
#include <fstream>
#include <stdexcept>

import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Player.RuntimeProductionSystemsBridge;
import Kairo.Player.RuntimeShippingBridge;
import Kairo.Player.RuntimeProject;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    void Write(const std::filesystem::path& path, const char* text)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << text;
        if (!output) throw std::runtime_error("Production runtime fixture write failed.");
    }
}

int main()
{
    using namespace kairo::engine;
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "kairo-player-production-runtime-test";
    fs::remove_all(root);
    fs::create_directories(root / "Scenes");
    fs::create_directories(root / "Config");

    Write(root / "ProductionTest.kproject",
        "kairo-project 2\nname \"Production Test\"\nengine-version \"0.1.0\"\n"
        "assets \"Assets.kassets\"\nstartup-scene \"Scenes/Main.kscene\"\n"
        "input-map \"Config/Input.kinput\"\nrendering-profile \"desktop\"\n"
        "build-profile \"Development\" development \"Build/Development\"\n");
    Write(root / "Assets.kassets", "kairo-assets 1\n");
    Write(root / "Config/Input.kinput", "kairo-input 1\n");
    kairo::assets::AssetRegistry assets;
    Scene scene;
    const Entity cameraEntity = scene.CreateEntity("Camera");
    CameraComponent camera;
    camera.Primary = true;
    scene.SetCamera(cameraEntity, camera);
    scene.Transform(cameraEntity).Local.Translation = { 64.0f, 0.0f, 0.0f };
    SaveScene(root / "Scenes/Main.kscene", scene, assets);

    ProductionSystemsManifest manifest;
    manifest.Particles = ProductionParticleDescriptor{ 32u };
    manifest.Fluid = ProductionFluidDescriptor{ 8u, 8u, 0.05 };
    manifest.Streaming = ProductionStreamingDescriptor{ 32.0, 1 };
    SaveProductionSystemsManifest(root / DefaultProductionSystemsManifestPath, manifest);

    kairo::player::RuntimeProject project(root / "ProductionTest.kproject");
    kairo::player::RuntimeProductionSystemsBridge bridge(project);
    Require(bridge.Enabled(), "Player did not load production systems manifest.");
    Require(bridge.Runtime() != nullptr, "Player production runtime is missing.");
    bridge.Runtime()->EmitParticle({ {}, { 1.0, 0.0, 0.0 }, 1.0 });
    bridge.Step(0.1);
    Require(bridge.Runtime()->Particles()->Particles().size() == 1u,
        "Player production particle system did not advance.");
    Require(bridge.Runtime()->Streaming()->Loaded().size() == 9u,
        "Player production streaming system did not follow the authored camera cell.");
    const auto profile = bridge.Profile();
    Require(profile.has_value() && profile->Frames == 1u,
        "Player production runtime did not publish profiling counters.");

    kairo::player::RuntimeShippingBridge shipping(project);
    shipping.Audio().AddBus({"Effects",0.5,false});
    AudioVoice voice; voice.Bus="Effects"; voice.DurationSeconds=1.0;
    (void)shipping.Audio().Play(voice);
    Require(shipping.Step(0.25).ActiveVoices==1u,"Player shipping audio did not advance.");
    LocalizationCatalog catalog; catalog.Set("en","continue","Continue"); catalog.Set("en","continue-label","Continue game");
    RuntimeUIScene ui(std::move(catalog)); RuntimeWidget button; button.ID="continue"; button.Kind=RuntimeWidgetKind::Button;
    button.TextKey="continue"; button.AccessibleLabelKey="continue-label"; button.Action="game.continue"; button.Focusable=true; ui.Add(button);
    shipping.InstallUI(std::move(ui));
    Require(shipping.UI()->Activate("continue")=="game.continue","Player shipping UI action did not resolve.");
    shipping.Save("Saves/slot-1.ksave");
    const auto savedHash=HashRuntimeState(shipping.State());
    shipping.Record(1u,{"continue"});
    shipping.Load("Saves/slot-1.ksave");
    Require(HashRuntimeState(shipping.State())==savedHash,"Player save state did not round trip.");
    shipping.Verify(0u);
    Require(shipping.ReplayFrames()==1u,"Player replay frame was not retained.");

    fs::remove_all(root);
    return 0;
}
