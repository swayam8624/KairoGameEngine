module;

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

export module Kairo.Player.RuntimeSceneAudioBridge;

import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Player.RuntimeAudioBridge;

export namespace kairo::player
{
    struct RuntimeAudioVoicePolicy final
    {
        kairo::assets::AssetID Clip;
        bool Loop = false;
        bool Spatial = true;
        double Pitch = 1.0;
        kairo::engine::AudioDistanceModel DistanceModel =
            kairo::engine::AudioDistanceModel::Inverse;
        double MinDistance = 1.0;
        double MaxDistance = 100.0;
        double Rolloff = 1.0;
        std::string Bus = "master";
        std::int32_t Priority = 0;

        friend bool operator==(const RuntimeAudioVoicePolicy&,
            const RuntimeAudioVoicePolicy&) = default;
    };

    /// Converts authored EngineCore audio components into live mixer voices.
    ///
    /// The bridge deliberately owns no PCM or native device state. RuntimeAudioBridge
    /// owns decoded clips/mixing, Scene owns authored emitter/listener state, and a
    /// future native output adapter only needs to consume the stereo frames returned
    /// by Advance(). This keeps scene lifecycle, mixing policy, and device I/O from
    /// becoming one platform-specific subsystem.
    class RuntimeSceneAudioBridge final
    {
        struct EmitterState final
        {
            kairo::engine::AudioVoiceHandle Voice{};
            RuntimeAudioVoicePolicy Policy{};
            bool PolicyInitialized = false;
            bool WasActive = false;
            bool AutoStarted = false;
        };

    public:
        RuntimeSceneAudioBridge(kairo::engine::Scene& scene,
            RuntimeAudioBridge& audio) noexcept
            : m_Scene(scene), m_Audio(audio) {}

        RuntimeSceneAudioBridge(const RuntimeSceneAudioBridge&) = delete;
        RuntimeSceneAudioBridge& operator=(const RuntimeSceneAudioBridge&) = delete;

        void BeginPlay()
        {
            if (m_Running)
                throw std::logic_error("Runtime scene audio is already running.");
            m_Running = true;
            m_FrameRemainder = 0.0;
            SynchronizeScene();
        }

        /// Synchronizes listener/emitter transforms and advances the canonical CPU
        /// mixer by elapsed wall-clock time. Returning frames rather than writing to
        /// a device makes this the stable handoff for CoreAudio/WASAPI/ALSA adapters.
        [[nodiscard]] std::vector<float> Advance(double elapsedSeconds)
        {
            if (!m_Running)
                throw std::logic_error(
                    "Runtime scene audio must BeginPlay before Advance.");
            if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0 ||
                elapsedSeconds > 1.0)
                throw std::invalid_argument(
                    "Runtime scene audio elapsed time must be finite and within one second.");

            SynchronizeScene();
            m_FrameRemainder += elapsedSeconds *
                static_cast<double>(m_Audio.OutputSampleRate());
            const auto frameCount = static_cast<std::size_t>(
                std::floor(m_FrameRemainder));
            m_FrameRemainder -= static_cast<double>(frameCount);
            if (frameCount == 0u) return {};
            return m_Audio.Mix(frameCount);
        }

        void EndPlay() noexcept
        {
            for (auto& [entity, state] : m_Emitters)
            {
                (void)entity;
                StopVoice(state);
            }
            m_Emitters.clear();
            m_Running = false;
            m_FrameRemainder = 0.0;
            try { m_Audio.SetListener({}); }
            catch (...) {}
        }

        [[nodiscard]] bool Running() const noexcept { return m_Running; }

        [[nodiscard]] std::optional<kairo::engine::AudioVoiceHandle> VoiceFor(
            kairo::engine::Entity entity) const noexcept
        {
            const auto found = m_Emitters.find(entity.Value);
            if (found == m_Emitters.end() ||
                found->second.Voice == kairo::engine::InvalidAudioVoice ||
                !m_Audio.IsPlaying(found->second.Voice))
                return std::nullopt;
            return found->second.Voice;
        }

        [[nodiscard]] std::size_t PlayingEmitterCount() const noexcept
        {
            std::size_t count = 0u;
            for (const auto& [entity, state] : m_Emitters)
            {
                (void)entity;
                if (state.Voice != kairo::engine::InvalidAudioVoice &&
                    m_Audio.IsPlaying(state.Voice))
                    ++count;
            }
            return count;
        }

        /// Explicit gameplay/editor trigger. Manual Stop() latches the current
        /// active interval so PlayOnStart does not immediately restart the voice.
        [[nodiscard]] kairo::engine::AudioVoiceHandle Play(
            kairo::engine::Entity entity)
        {
            RequirePlayableEmitter(entity);
            auto& state = m_Emitters[entity.Value];
            StopVoice(state);
            const auto& emitter = m_Scene.AudioEmitter(entity);
            state.Policy = MakePolicy(emitter);
            state.PolicyInitialized = true;
            state.WasActive = true;
            state.AutoStarted = true;
            state.Voice = StartVoice(entity, emitter);
            return state.Voice;
        }

        bool Stop(kairo::engine::Entity entity) noexcept
        {
            const auto found = m_Emitters.find(entity.Value);
            if (found == m_Emitters.end()) return false;
            const bool stopped = StopVoice(found->second);
            found->second.AutoStarted = true;
            return stopped;
        }

        void SynchronizeScene()
        {
            SynchronizeListener();
            SynchronizeEmitters();
        }

    private:
        kairo::engine::Scene& m_Scene;
        RuntimeAudioBridge& m_Audio;
        std::unordered_map<std::uint32_t, EmitterState> m_Emitters;
        bool m_Running = false;
        double m_FrameRemainder = 0.0;

        [[nodiscard]] static kairo::engine::AudioVec3 ToAudioVec3(
            const kairo::foundation::math::Vector3f& value) noexcept
        {
            return {
                static_cast<double>(value.x),
                static_cast<double>(value.y),
                static_cast<double>(value.z)
            };
        }

        [[nodiscard]] static RuntimeAudioVoicePolicy MakePolicy(
            const kairo::engine::AudioEmitterComponent& emitter)
        {
            emitter.Validate();
            return {
                emitter.Clip.ID,
                emitter.Loop,
                emitter.Spatial,
                emitter.Pitch,
                emitter.Attenuation.Model,
                emitter.Attenuation.MinDistance,
                emitter.Attenuation.MaxDistance,
                emitter.Attenuation.Rolloff,
                emitter.Bus,
                emitter.Priority
            };
        }

        void SynchronizeListener()
        {
            const auto active = m_Scene.ActiveAudioListener();
            if (!active.has_value())
            {
                m_Audio.SetListener({});
                return;
            }

            const auto world = m_Scene.WorldTransform(*active);
            const auto forward = kairo::foundation::math::Forward(world.Rotation);
            const auto up = kairo::foundation::math::Up(world.Rotation);
            kairo::engine::AudioListener listener;
            listener.Position = ToAudioVec3(world.Translation);
            listener.Forward = ToAudioVec3(forward);
            listener.Up = ToAudioVec3(up);
            m_Audio.SetListener(listener);
        }

        void SynchronizeEmitters()
        {
            std::unordered_set<std::uint32_t> present;
            for (const auto entity : m_Scene.Entities())
            {
                if (!m_Scene.HasAudioEmitter(entity)) continue;
                present.emplace(entity.Value);
                SynchronizeEmitter(entity);
            }

            for (auto iterator = m_Emitters.begin(); iterator != m_Emitters.end();)
            {
                if (present.contains(iterator->first))
                {
                    ++iterator;
                    continue;
                }
                StopVoice(iterator->second);
                iterator = m_Emitters.erase(iterator);
            }
        }

        void SynchronizeEmitter(kairo::engine::Entity entity)
        {
            auto& state = m_Emitters[entity.Value];
            const auto& emitter = m_Scene.AudioEmitter(entity);
            const bool active = emitter.Enabled && m_Scene.IsActiveInHierarchy(entity);

            if (!active)
            {
                StopVoice(state);
                state.WasActive = false;
                state.AutoStarted = false;
                return;
            }

            if (!state.WasActive)
                state.AutoStarted = false;

            const auto policy = MakePolicy(emitter);
            if (state.PolicyInitialized && policy != state.Policy)
            {
                StopVoice(state);
                state.AutoStarted = false;
            }
            state.Policy = policy;
            state.PolicyInitialized = true;

            if (state.Voice != kairo::engine::InvalidAudioVoice &&
                !m_Audio.IsPlaying(state.Voice))
                state.Voice = kairo::engine::InvalidAudioVoice;

            if (state.Voice == kairo::engine::InvalidAudioVoice &&
                emitter.PlayOnStart && !state.AutoStarted)
            {
                state.Voice = StartVoice(entity, emitter);
                // A priority-rejected start is still one auto-start attempt for
                // this active interval; otherwise every frame would retry/steal.
                state.AutoStarted = true;
            }

            if (state.Voice != kairo::engine::InvalidAudioVoice &&
                m_Audio.IsPlaying(state.Voice))
            {
                m_Audio.SetVoicePosition(state.Voice,
                    ToAudioVec3(m_Scene.WorldTransform(entity).Translation));
                m_Audio.SetVoiceGain(state.Voice, emitter.Gain);
            }
            state.WasActive = true;
        }

        [[nodiscard]] kairo::engine::AudioVoiceHandle StartVoice(
            kairo::engine::Entity entity,
            const kairo::engine::AudioEmitterComponent& emitter)
        {
            m_Audio.EnsureBus(emitter.Bus);
            const auto position = ToAudioVec3(
                m_Scene.WorldTransform(entity).Translation);
            return m_Audio.Play(emitter.Clip.ID,
                emitter.VoiceSettings(position));
        }

        void RequirePlayableEmitter(kairo::engine::Entity entity) const
        {
            if (!m_Scene.Contains(entity) || !m_Scene.HasAudioEmitter(entity))
                throw std::invalid_argument(
                    "Runtime scene audio Play requires an authored emitter entity.");
            const auto& emitter = m_Scene.AudioEmitter(entity);
            if (!emitter.Enabled || !m_Scene.IsActiveInHierarchy(entity))
                throw std::invalid_argument(
                    "Runtime scene audio cannot play a disabled emitter entity.");
        }

        bool StopVoice(EmitterState& state) noexcept
        {
            if (state.Voice == kairo::engine::InvalidAudioVoice) return false;
            const auto voice = state.Voice;
            state.Voice = kairo::engine::InvalidAudioVoice;
            return m_Audio.Stop(voice);
        }
    };
}
