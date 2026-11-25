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
                kairo::engine::LocalizationCatalog catalog;
                for(const auto& button:doc)catalog.Set("en",button.ID,button.Label);
                kairo::engine::RuntimeUIScene ui(std::move(catalog));
                for(const auto& button:doc)
                {
                    kairo::engine::RuntimeWidget widget;
                    widget.ID=button.ID;widget.Kind=kairo::engine::RuntimeWidgetKind::Button;
                    widget.Anchors={button.X/1000.,button.Y/1000.,button.W/1000.,button.H/1000.};
                    widget.TextKey=button.ID;widget.AccessibleLabelKey=button.ID;
                    widget.Action=button.Action;widget.Focusable=true;
                    ui.Add(std::move(widget));
                }
                InstallUI(std::move(ui));
            }
        }
        [[nodiscard]] kairo::engine::RuntimeAudioMixer& Audio() noexcept { return m_Audio; }
        [[nodiscard]] const kairo::engine::RuntimeStateSnapshot& State() const noexcept { return m_State; }
        void SetState(kairo::engine::RuntimeStateSnapshot state) { m_State=std::move(state); }
        void InstallUI(kairo::engine::RuntimeUIScene ui) { m_UI=std::make_unique<kairo::engine::RuntimeUIScene>(std::move(ui)); }
        [[nodiscard]] kairo::engine::RuntimeUIScene* UI() noexcept { return m_UI.get(); }
        [[nodiscard]] kairo::engine::AudioMixFrame Step(double deltaSeconds) { return m_Audio.Step(deltaSeconds); }
        void Save(std::filesystem::path relative) const
        { kairo::engine::SaveRuntimeState(ProjectPath(relative),m_State); }
        void Load(std::filesystem::path relative)
        { m_State=kairo::engine::LoadRuntimeState(ProjectPath(relative)); }
        void Record(std::uint64_t tick,std::vector<std::string> actions)
        { m_Replay.Record({tick,std::move(actions),kairo::engine::HashRuntimeState(m_State)}); }
        void Verify(std::size_t frame) const { m_Replay.Verify(frame,m_State); }
        [[nodiscard]] std::size_t ReplayFrames() const noexcept { return m_Replay.Size(); }
    private:
        RuntimeProject& m_Project;
        kairo::engine::RuntimeAudioMixer m_Audio;
        std::unique_ptr<kairo::engine::RuntimeUIScene> m_UI;
        kairo::engine::RuntimeStateSnapshot m_State;
        kairo::engine::DeterministicReplay m_Replay;
        [[nodiscard]] std::filesystem::path ProjectPath(const std::filesystem::path& relative) const
        { return m_Project.Root()/kairo::assets::NormalizeAssetPath(relative); }
    };
}
