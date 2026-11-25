module;

#include "RuntimeMenuDocument.hpp"
#include "CraftSaveFile.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module Kairo.Player.RuntimeShippingBridge;

import Kairo.Assets;
import Kairo.EngineCore.ShippingRuntime;
import Kairo.Player.RuntimeProject;

export namespace kairo::player
{
    /// Task: bind the shipping contracts to one loaded project while device,
    /// renderer, and network transports remain replaceable host adapters.
    class RuntimeShippingBridge final
    {
    public:
        // Construct once per loaded project; the project must outlive this bridge.
        // Initial snapshot values are metadata, not a complete scene save.
        explicit RuntimeShippingBridge(RuntimeProject& project) : m_Project(project)
        {
            m_State.Values["project.name"]=project.Descriptor().Name;
            m_State.Values["scene.entities"]=static_cast<std::int64_t>(project.Scene().Size());
            // Optional authored UI is validated even during --validate.
            const auto path=ProjectPath("Config/Menu.kmenu");
            if(std::filesystem::exists(path))
            {
                const auto bytes=detail::ReadCraftSaveFile(path,detail::MaximumMenuBytes);
                const auto doc=detail::ParseRuntimeMenu(std::string_view(
                    reinterpret_cast<const char*>(bytes.data()),bytes.size()));
                // Phase 1 builds the localization map before constructing the scene.
                // Every stable widget ID resolves both display and accessible labels.
                kairo::engine::LocalizationCatalog catalog;
                for(const auto& button:doc)catalog.Set("en",button.ID,button.Label);
                kairo::engine::RuntimeUIScene ui(std::move(catalog));
                for(const auto& button:doc)
                {
                    kairo::engine::RuntimeWidget widget;
                    widget.ID=button.ID;widget.Kind=kairo::engine::RuntimeWidgetKind::Button;
                    // Convert integer thousandths to normalized layout fractions.
                    // EngineCore Layout later resolves these against physical pixels.
                    widget.Anchors={button.X/1000.,button.Y/1000.,button.W/1000.,button.H/1000.};
                    widget.TextKey=button.ID;widget.AccessibleLabelKey=button.ID;
                    widget.Action=button.Action;widget.Focusable=true;
                    ui.Add(std::move(widget));
                }
                InstallUI(std::move(ui));
            }
        }
        // Borrow the portable mixer; no native device or PCM queue is owned here.
        [[nodiscard]] kairo::engine::RuntimeAudioMixer& Audio() noexcept { return m_Audio; }
        // Read-only typed shipping state; not the full RuntimeProject scene.
        [[nodiscard]] const kairo::engine::RuntimeStateSnapshot& State() const noexcept { return m_State; }
        // Replace the complete snapshot by value. Callers own semantic validation.
        void SetState(kairo::engine::RuntimeStateSnapshot state) { m_State=std::move(state); }
        // Own a new scene. Any existing UI() borrower must be retired first;
        // RuntimeMenuHost is constructed only after initial installation.
        void InstallUI(kairo::engine::RuntimeUIScene ui) { m_UI=std::make_unique<kairo::engine::RuntimeUIScene>(std::move(ui)); }
        // Nullable borrowed pointer, valid until replacement or bridge destruction.
        [[nodiscard]] kairo::engine::RuntimeUIScene* UI() noexcept { return m_UI.get(); }
        // Advance shipping mixer levels by seconds. Device playback is handled
        // by RuntimeAudioBridge/RuntimeSceneAudioBridge, not this return value.
        [[nodiscard]] kairo::engine::AudioMixFrame Step(double deltaSeconds) { return m_Audio.Step(deltaSeconds); }
        // Persist the typed snapshot at a project-relative path using EngineCore's
        // versioned state format; this does not persist all physics entities.
        void Save(std::filesystem::path relative) const
        { kairo::engine::SaveRuntimeState(ProjectPath(relative),m_State); }
        // Load and validate before assignment; parsing exceptions leave state intact.
        void Load(std::filesystem::path relative)
        { m_State=kairo::engine::LoadRuntimeState(ProjectPath(relative)); }
        // Replay journal stores input action names and a hash of the current
        // state, not a full copy of every frame or a network packet.
        void Record(std::uint64_t tick,std::vector<std::string> actions)
        { m_Replay.Record({tick,std::move(actions),kairo::engine::HashRuntimeState(m_State)}); }
        // Compare current state with a recorded frame's expected hash.
        void Verify(std::size_t frame) const { m_Replay.Verify(frame,m_State); }
        // Number of recorded entries, independent of renderer frame count.
        [[nodiscard]] std::size_t ReplayFrames() const noexcept { return m_Replay.Size(); }
    private:
        RuntimeProject& m_Project;
        kairo::engine::RuntimeAudioMixer m_Audio;
        std::unique_ptr<kairo::engine::RuntimeUIScene> m_UI;
        kairo::engine::RuntimeStateSnapshot m_State;
        kairo::engine::DeterministicReplay m_Replay;
        // Normalize the authored relative asset path before resolving it against
        // the project root. This is lexical validation, not an OS sandbox.
        [[nodiscard]] std::filesystem::path ProjectPath(const std::filesystem::path& relative) const
        { return m_Project.Root()/kairo::assets::NormalizeAssetPath(relative); }
    };
}
