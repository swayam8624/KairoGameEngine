#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Player.RuntimeAudioBridge;
import Kairo.Player.RuntimeSceneAudioBridge;

namespace assets = kairo::assets;
namespace engine = kairo::engine;
namespace player = kairo::player;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    void RequireNear(double actual, double expected, double tolerance,
        const char* message)
    {
        if (std::abs(actual - expected) > tolerance)
            throw std::runtime_error(message);
    }

    void PushText(std::vector<std::byte>& bytes, const char* text,
        std::size_t count)
    {
        for (std::size_t index = 0u; index < count; ++index)
            bytes.push_back(std::byte{ static_cast<unsigned char>(text[index]) });
    }

    void PushU16(std::vector<std::byte>& bytes, std::uint16_t value)
    {
        bytes.push_back(std::byte{ static_cast<std::uint8_t>(value) });
        bytes.push_back(std::byte{ static_cast<std::uint8_t>(value >> 8u) });
    }

    void PushU32(std::vector<std::byte>& bytes, std::uint32_t value)
    {
        for (unsigned shift = 0u; shift < 32u; shift += 8u)
            bytes.push_back(std::byte{
                static_cast<std::uint8_t>(value >> shift) });
    }

    [[nodiscard]] std::vector<std::byte> MakePcm16Wav(
        std::span<const std::int16_t> samples)
    {
        constexpr std::uint16_t channels = 1u;
        constexpr std::uint32_t sampleRate = 48'000u;
        const std::uint32_t dataBytes =
            static_cast<std::uint32_t>(samples.size() * 2u);
        std::vector<std::byte> bytes;
        bytes.reserve(44u + dataBytes);
        PushText(bytes, "RIFF", 4u);
        PushU32(bytes, 36u + dataBytes);
        PushText(bytes, "WAVE", 4u);
        PushText(bytes, "fmt ", 4u);
        PushU32(bytes, 16u);
        PushU16(bytes, 1u);
        PushU16(bytes, channels);
        PushU32(bytes, sampleRate);
        PushU32(bytes, sampleRate * channels * 2u);
        PushU16(bytes, channels * 2u);
        PushU16(bytes, 16u);
        PushText(bytes, "data", 4u);
        PushU32(bytes, dataBytes);
        for (const std::int16_t sample : samples)
            PushU16(bytes, static_cast<std::uint16_t>(sample));
        return bytes;
    }

    void WriteBytes(const std::filesystem::path& path,
        std::span<const std::byte> bytes)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output || (!bytes.empty() && !output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()))))
            throw std::runtime_error("Unable to write scene-audio fixture.");
    }
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() /
        ("kairo-player-scene-audio-" + assets::GenerateAssetID().ToString());
    try
    {
        const auto clipID = assets::AssetID::Parse(
            "22222222-2222-4222-8222-222222222222");
        const std::vector<std::int16_t> samples{
            0, 12'000, -12'000, 20'000, -20'000, 8'000, -8'000, 0
        };
        WriteBytes(root / "Audio/engine.wav", MakePcm16Wav(samples));

        assets::AssetRegistry registry;
        assets::AssetMetadata metadata;
        metadata.ID = clipID;
        metadata.Type = assets::AssetType::Audio;
        metadata.Origin = assets::AssetOrigin::SourceFile;
        metadata.Path = "Audio/engine.wav";
        metadata.Importer = "kairo.audio.wav";
        metadata.Revision = 1u;
        registry.Insert(std::move(metadata));

        engine::Scene scene;
        const auto emitterEntity = scene.CreateEntityWithID({ 1u }, "Engine");
        const auto listenerEntity = scene.CreateEntityWithID({ 2u }, "Camera");
        scene.Transform(emitterEntity).Local.Translation = { 1.0f, 0.0f, 0.0f };
        scene.Transform(listenerEntity).Local.Translation = { 2.0f, 3.0f, 4.0f };

        engine::AudioEmitterComponent emitter;
        emitter.Clip = { clipID };
        emitter.PlayOnStart = true;
        emitter.Loop = false;
        emitter.Spatial = false;
        emitter.Gain = 0.5;
        emitter.Bus = "vehicles";
        scene.SetAudioEmitter(emitterEntity, emitter);

        engine::AudioListenerComponent listener;
        listener.Primary = true;
        scene.SetAudioListener(listenerEntity, listener);

        player::RuntimeAudioBridge audio(root, registry, 48'000u, 8u);
        player::RuntimeSceneAudioBridge sceneAudio(scene, audio);
        sceneAudio.BeginPlay();

        Require(sceneAudio.Running(),
            "Scene audio did not enter its play lifecycle.");
        Require(sceneAudio.PlayingEmitterCount() == 1u,
            "PlayOnStart emitter did not become a runtime voice.");
        Require(audio.HasBus("vehicles"),
            "Authored audio bus was not materialized at runtime.");
        Require(sceneAudio.VoiceFor(emitterEntity).has_value(),
            "Runtime scene audio lost its emitter-to-voice mapping.");
        RequireNear(audio.Listener().Position.X, 2.0, 1.0e-6,
            "Listener world X did not propagate to the mixer.");
        RequireNear(audio.Listener().Position.Y, 3.0, 1.0e-6,
            "Listener world Y did not propagate to the mixer.");
        RequireNear(audio.Listener().Position.Z, 4.0, 1.0e-6,
            "Listener world Z did not propagate to the mixer.");

        const auto firstFrames = sceneAudio.Advance(2.0 / 48'000.0);
        Require(firstFrames.size() == 4u,
            "Scene audio Advance did not return interleaved stereo frames.");

        // Consume the rest of the one-shot. It must not auto-restart every frame
        // merely because PlayOnStart remains authored true.
        (void)sceneAudio.Advance(0.01);
        Require(sceneAudio.PlayingEmitterCount() == 0u,
            "Completed one-shot scene voice remained active.");
        (void)sceneAudio.Advance(0.0);
        Require(sceneAudio.PlayingEmitterCount() == 0u,
            "Completed PlayOnStart voice restarted without an activation edge.");

        // A disable/re-enable transition is a new active interval and therefore
        // legitimately replays a PlayOnStart source.
        scene.SetEnabled(emitterEntity, false);
        (void)sceneAudio.Advance(0.0);
        scene.SetEnabled(emitterEntity, true);
        (void)sceneAudio.Advance(0.0);
        Require(sceneAudio.PlayingEmitterCount() == 1u,
            "Re-enabled emitter did not replay on its new active interval.");

        Require(sceneAudio.Stop(emitterEntity),
            "Manual scene-audio stop did not stop the live voice.");
        (void)sceneAudio.Advance(0.0);
        Require(sceneAudio.PlayingEmitterCount() == 0u,
            "Manual Stop was immediately undone by PlayOnStart.");

        const auto manual = sceneAudio.Play(emitterEntity);
        Require(manual != engine::InvalidAudioVoice && audio.IsPlaying(manual),
            "Manual scene-audio Play did not create a live voice.");

        bool invalidDeltaRejected = false;
        try { (void)sceneAudio.Advance(-0.1); }
        catch (const std::invalid_argument&) { invalidDeltaRejected = true; }
        Require(invalidDeltaRejected,
            "Scene audio accepted a negative frame delta.");

        sceneAudio.EndPlay();
        Require(!sceneAudio.Running() && audio.ActiveVoiceCount() == 0u,
            "Scene audio EndPlay leaked live voices.");

        std::filesystem::remove_all(root);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::filesystem::remove_all(root);
        std::cerr << "KairoPlayer scene audio bridge test: " << error.what() << '\n';
        return 1;
    }
}
