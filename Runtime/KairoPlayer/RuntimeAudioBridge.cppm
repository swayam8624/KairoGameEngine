module;

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

export module Kairo.Player.RuntimeAudioBridge;

import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Player.RuntimeProject;

export namespace kairo::player
{
    struct RuntimeAudioImport final
    {
        std::shared_ptr<const kairo::engine::AudioClip> Clip;
        kairo::assets::DerivedDataKey CacheKey;
        bool CacheHit = false;
    };

    [[nodiscard]] inline RuntimeAudioImport ImportRuntimeAudioClip(
        const std::filesystem::path& projectRoot,
        kairo::assets::AssetID asset,
        const kairo::assets::AssetRegistry& registry,
        kairo::assets::ImportDatabase& imports,
        const kairo::assets::DerivedDataCache& cache)
    {
        const auto metadata = registry.At(asset);
        kairo::assets::WavAudioImporter importer;
        if (metadata.Type != kairo::assets::AssetType::Audio ||
            metadata.Origin != kairo::assets::AssetOrigin::SourceFile ||
            metadata.Importer != importer.Identifier())
            throw std::invalid_argument(
                "Runtime audio import requires a source Audio asset using kairo.audio.wav.");

        kairo::assets::ImportRecord record{
            metadata.ID,
            metadata.Path,
            importer.Identifier(),
            importer.Version(),
            kairo::assets::CanonicalWavImportSettings({}),
            {},
            1u
        };
        auto outcome = kairo::assets::ImportSourceAsset(projectRoot,
            std::move(record), importer, registry, imports, cache);
        auto artifact = kairo::assets::ParseAudioDerivedArtifact(outcome.Artifact);

        auto clip = std::make_shared<kairo::engine::AudioClip>();
        clip->SampleRate = artifact.SampleRate;
        clip->Channels = artifact.Channels;
        clip->Samples = std::move(artifact.Samples);
        clip->Validate();
        return { std::move(clip), outcome.Key, outcome.CacheHit };
    }

    /// Runtime-owned audio asset/mixer boundary. Source decoding and the derived
    /// cache stay KairoAssets-owned; authored scene synchronization is layered on
    /// top by RuntimeSceneAudioBridge. Native output backends can consume Mix()
    /// without taking ownership of voices or scene policy.
    class RuntimeAudioBridge final
    {
    public:
        RuntimeAudioBridge(const std::filesystem::path& projectRoot,
            const kairo::assets::AssetRegistry& registry,
            std::uint32_t outputSampleRate = 48'000u,
            std::size_t maximumVoices = 128u)
            : m_ProjectRoot(projectRoot),
              m_Registry(registry),
              m_Cache(projectRoot / ".kairo" / "derived-data"),
              m_OutputSampleRate(outputSampleRate),
              m_Mixer(outputSampleRate, maximumVoices)
        {
            if (projectRoot.empty())
                throw std::invalid_argument("Runtime audio bridge requires a project root.");
            m_Buses.emplace("master");
            LoadAssets();
        }

        RuntimeAudioBridge(const RuntimeProject& project,
            std::uint32_t outputSampleRate = 48'000u,
            std::size_t maximumVoices = 128u)
            : RuntimeAudioBridge(project.Root(), project.Assets(),
                outputSampleRate, maximumVoices) {}

        [[nodiscard]] bool HasClip(kairo::assets::AssetID asset) const noexcept
        {
            return m_Clips.contains(asset);
        }

        [[nodiscard]] std::size_t LoadedClipCount() const noexcept
        {
            return m_Clips.size();
        }

        [[nodiscard]] std::size_t CacheHitCount() const noexcept
        {
            return m_CacheHits;
        }

        [[nodiscard]] std::uint32_t OutputSampleRate() const noexcept
        {
            return m_OutputSampleRate;
        }

        [[nodiscard]] const kairo::engine::AudioClip& Clip(
            kairo::assets::AssetID asset) const
        {
            return *RequireClip(asset);
        }

        [[nodiscard]] kairo::engine::AudioVoiceHandle Play(
            kairo::assets::AssetID asset,
            kairo::engine::AudioVoiceSettings settings = {})
        {
            return m_Mixer.Play(RequireClip(asset), std::move(settings));
        }

        bool Stop(kairo::engine::AudioVoiceHandle voice) noexcept
        {
            return m_Mixer.Stop(voice);
        }

        [[nodiscard]] bool IsPlaying(
            kairo::engine::AudioVoiceHandle voice) const noexcept
        {
            return m_Mixer.IsPlaying(voice);
        }

        void SetPaused(kairo::engine::AudioVoiceHandle voice, bool paused)
        {
            m_Mixer.SetPaused(voice, paused);
        }

        void SetVoicePosition(kairo::engine::AudioVoiceHandle voice,
            kairo::engine::AudioVec3 position)
        {
            m_Mixer.SetVoicePosition(voice, position);
        }

        void SetVoiceGain(kairo::engine::AudioVoiceHandle voice, double gain)
        {
            m_Mixer.SetVoiceGain(voice, gain);
        }

        void SetListener(kairo::engine::AudioListener listener)
        {
            m_Mixer.SetListener(std::move(listener));
        }

        [[nodiscard]] const kairo::engine::AudioListener& Listener() const noexcept
        {
            return m_Mixer.Listener();
        }

        [[nodiscard]] bool HasBus(std::string_view name) const
        {
            return m_Buses.contains(std::string(name));
        }

        void DefineBus(std::string name, kairo::engine::AudioBusState state = {})
        {
            const std::string key = name;
            m_Mixer.DefineBus(std::move(name), state);
            m_Buses.emplace(key);
        }

        /// Ensures authored bus names are playable even before a project-level
        /// bus graph/editor lands. New buses inherit neutral gain/mute state.
        /// Existing buses, including the built-in master bus, are untouched.
        bool EnsureBus(std::string_view name)
        {
            if (name.empty())
                throw std::invalid_argument("Runtime audio bus name cannot be empty.");
            const std::string key(name);
            if (m_Buses.contains(key)) return false;
            m_Mixer.DefineBus(key, {});
            m_Buses.emplace(key);
            return true;
        }

        void SetBusState(std::string_view name, kairo::engine::AudioBusState state)
        {
            m_Mixer.SetBusState(name, state);
        }

        [[nodiscard]] std::vector<float> Mix(std::size_t frameCount)
        {
            return m_Mixer.Mix(frameCount);
        }

        [[nodiscard]] std::size_t ActiveVoiceCount() const noexcept
        {
            return m_Mixer.ActiveVoiceCount();
        }

        [[nodiscard]] const kairo::engine::AudioMixStats& Stats() const noexcept
        {
            return m_Mixer.Stats();
        }

    private:
        [[nodiscard]] std::shared_ptr<const kairo::engine::AudioClip> RequireClip(
            kairo::assets::AssetID asset) const
        {
            const auto found = m_Clips.find(asset);
            if (found == m_Clips.end())
                throw std::out_of_range(
                    "No runtime audio clip is bound for asset ID: " + asset.ToString());
            return found->second;
        }

        void LoadAssets()
        {
            for (const auto& metadata : m_Registry.Snapshot())
            {
                if (metadata.Type != kairo::assets::AssetType::Audio) continue;
                if (metadata.Origin != kairo::assets::AssetOrigin::SourceFile)
                    throw std::invalid_argument(
                        "Runtime Audio assets must currently originate from source files: " +
                        metadata.ID.ToString());
                const auto imported = ImportRuntimeAudioClip(m_ProjectRoot,
                    metadata.ID, m_Registry, m_Imports, m_Cache);
                if (!m_Clips.emplace(metadata.ID, imported.Clip).second)
                    throw std::logic_error("Runtime audio asset was bound more than once.");
                if (imported.CacheHit) ++m_CacheHits;
            }
        }

        std::filesystem::path m_ProjectRoot;
        const kairo::assets::AssetRegistry& m_Registry;
        kairo::assets::ImportDatabase m_Imports;
        kairo::assets::DerivedDataCache m_Cache;
        std::uint32_t m_OutputSampleRate = 48'000u;
        kairo::engine::AudioMixer m_Mixer;
        std::unordered_map<kairo::assets::AssetID,
            std::shared_ptr<const kairo::engine::AudioClip>,
            kairo::assets::AssetIDHash> m_Clips;
        std::unordered_set<std::string> m_Buses;
        std::size_t m_CacheHits = 0u;
    };
}
