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

namespace assets = kairo::assets;
namespace engine = kairo::engine;
namespace player = kairo::player;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    void RequireNear(float actual, float expected, float tolerance,
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
        std::uint16_t channels, std::uint32_t sampleRate,
        std::span<const std::int16_t> samples)
    {
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
        const auto blockAlign = static_cast<std::uint16_t>(channels * 2u);
        PushU32(bytes, sampleRate * blockAlign);
        PushU16(bytes, blockAlign);
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
            throw std::runtime_error("Unable to write runtime audio fixture.");
    }
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() /
        ("kairo-player-audio-" + assets::GenerateAssetID().ToString());
    try
    {
        const auto id = assets::AssetID::Parse(
            "11111111-1111-4111-8111-111111111111");
        const std::vector<std::int16_t> samples{
            0, 16'384, -16'384, 32'767
        };
        WriteBytes(root / "Audio/tone.wav",
            MakePcm16Wav(1u, 48'000u, samples));

        assets::AssetRegistry registry;
        assets::AssetMetadata metadata;
        metadata.ID = id;
        metadata.Type = assets::AssetType::Audio;
        metadata.Origin = assets::AssetOrigin::SourceFile;
        metadata.Path = "Audio/tone.wav";
        metadata.Importer = "kairo.audio.wav";
        metadata.Revision = 1u;
        registry.Insert(std::move(metadata));

        player::RuntimeAudioBridge bridge(root, registry, 48'000u, 8u);
        Require(bridge.LoadedClipCount() == 1u,
            "Runtime audio bridge did not load the registered WAV asset.");
        Require(bridge.CacheHitCount() == 0u,
            "First runtime audio import unexpectedly reported a cache hit.");
        Require(bridge.HasClip(id),
            "Runtime audio asset lookup lost the imported clip.");
        const auto& clip = bridge.Clip(id);
        Require(clip.SampleRate == 48'000u && clip.Channels == 1u,
            "Runtime audio clip format does not match the source artifact.");
        Require(clip.FrameCount() == 4u,
            "Runtime audio clip frame count is incorrect.");

        const auto voice = bridge.Play(id);
        Require(voice != engine::InvalidAudioVoice,
            "Runtime audio bridge failed to create a voice.");
        const auto mixed = bridge.Mix(4u);
        Require(mixed.size() == 8u,
            "Runtime audio mixer did not return interleaved stereo frames.");
        RequireNear(mixed[0], 0.0f, 1.0e-6f,
            "Mono audio first left sample is incorrect.");
        RequireNear(mixed[1], 0.0f, 1.0e-6f,
            "Mono audio first right sample is incorrect.");
        RequireNear(mixed[2], 0.5f, 1.0e-5f,
            "Mono audio was not duplicated into the left channel.");
        RequireNear(mixed[3], 0.5f, 1.0e-5f,
            "Mono audio was not duplicated into the right channel.");
        RequireNear(mixed[4], -0.5f, 1.0e-5f,
            "Runtime PCM conversion changed the negative sample.");
        (void)bridge.Mix(1u);
        Require(bridge.ActiveVoiceCount() == 0u,
            "Completed one-shot voice remained active.");

        bool missingRejected = false;
        try { (void)bridge.Play(assets::GenerateAssetID()); }
        catch (const std::out_of_range&) { missingRejected = true; }
        Require(missingRejected,
            "Runtime audio bridge accepted an unbound asset ID.");

        player::RuntimeAudioBridge cached(root, registry, 48'000u, 8u);
        Require(cached.CacheHitCount() == 1u,
            "Runtime audio bridge did not reuse the derived PCM cache entry.");

        std::filesystem::remove_all(root);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::filesystem::remove_all(root);
        std::cerr << "KairoPlayer audio bridge test: " << error.what() << '\n';
        return 1;
    }
}
