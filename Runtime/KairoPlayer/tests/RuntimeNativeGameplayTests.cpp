#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Player.RuntimeNativeGameplayBridge;
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
        if (!output) throw std::runtime_error("Native gameplay fixture write failed.");
    }

    class CounterSystem final : public kairo::engine::NativeGameplaySystem
    {
    public:
        static inline int Began = 0;
        static inline int Fixed = 0;
        static inline int Updated = 0;
        double Speed = 0.0;

        void ApplyProperty(std::string_view name,
            const kairo::engine::NativeGameplayValue& value) override
        {
            if (name == "speed") Speed = std::get<double>(value);
        }
        void OnBeginPlay(kairo::engine::NativeGameplayContext& context) override
        {
            ++Began;
            context.AddTag(context.Self(), "native-player-began");
        }
        void OnFixedUpdate(kairo::engine::NativeGameplayContext&) override { ++Fixed; }
        void OnUpdate(kairo::engine::NativeGameplayContext& context) override
        {
            ++Updated;
            if (Speed > 1.0) context.AddTag(context.Self(), "native-player-fast");
        }
    };

    kairo::engine::NativeGameplayRegistry MakeRegistry()
    {
        using namespace kairo::engine;
        NativeGameplayRegistry registry;
        NativeGameplayTypeInfo type;
        type.TypeName = "Counter";
        type.Properties.push_back({ "speed", NativeGameplayPropertyType::Number, 1.0, true, 0.0, 10.0 });
        registry.Register(type, [] { return std::make_unique<CounterSystem>(); });
        return registry;
    }
}

int main()
{
    using namespace kairo::engine;
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "kairo-player-native-gameplay-test";
    fs::remove_all(root);
    fs::create_directories(root / "Scenes");
    fs::create_directories(root / "Config");

    Write(root / "NativeTest.kproject",
        "kairo-project 2\nname \"Native Test\"\nengine-version \"0.1.0\"\n"
        "assets \"Assets.kassets\"\nstartup-scene \"Scenes/Main.kscene\"\n"
        "input-map \"Config/Input.kinput\"\nrendering-profile \"desktop\"\n");
    Write(root / "Assets.kassets", "kairo-assets 1\n");
    Write(root / "Config/Input.kinput", "kairo-input 1\n");

    kairo::assets::AssetRegistry assets;
    Scene scene;
    const Entity actor = scene.CreateEntity("Actor");
    SaveScene(root / "Scenes/Main.kscene", assets, scene);

    NativeGameplayManifest manifest;
    NativeGameplayAttachment attachment;
    attachment.Target = actor;
    attachment.TypeName = "Counter";
    attachment.Properties["speed"] = 2.5;
    manifest.Attachments.push_back(std::move(attachment));
    SaveNativeGameplayManifest(root / DefaultNativeGameplayManifestPath, manifest);

    CounterSystem::Began = CounterSystem::Fixed = CounterSystem::Updated = 0;
    kairo::player::RuntimeProject project(root / "NativeTest.kproject");
    const auto registry = MakeRegistry();
    kairo::player::RuntimeNativeGameplayBridge gameplay(project, registry);
    Require(gameplay.InstanceCount() == 1u, "Player did not instantiate native gameplay manifest attachment.");
    gameplay.BeginPlay();
    Require(CounterSystem::Began == 1, "Player did not dispatch native BeginPlay.");
    Require(project.Scene().HasTag(actor, "native-player-began"), "Native BeginPlay mutation was not synchronized to live scene.");
    gameplay.BeforePhysicsStep(1.0f / 60.0f);
    Require(CounterSystem::Fixed == 1, "Player did not dispatch native FixedUpdate.");
    gameplay.Update(1.0 / 60.0);
    Require(CounterSystem::Updated == 1, "Player did not dispatch native Update.");
    Require(project.Scene().HasTag(actor, "native-player-fast"), "Reflected native property did not affect runtime behavior.");
    gameplay.EndPlay();

    bool unlinkedRejected = false;
    try
    {
        kairo::engine::NativeGameplayRegistry empty;
        kairo::player::RuntimeNativeGameplayBridge missing(project, empty);
        (void)missing;
    }
    catch (const std::out_of_range&) { unlinkedRejected = true; }
    Require(unlinkedRejected, "Player accepted a native gameplay manifest whose type was not linked/registered.");

    fs::remove_all(root);
    return 0;
}
