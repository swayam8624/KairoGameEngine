module;

#include <GLFW/glfw3.h>
#include "RuntimeInputGate.hpp"

#include <cstddef>
#include <string_view>

export module Kairo.Player.RuntimeInputBridge;

import Kairo.EngineCore;
import Kairo.Renderer;

export namespace kairo::player
{
    /// Platform adapter for one standalone player window.
    ///
    /// Input: an immutable authored action map and the GLFW-backed renderer
    /// window polled once per rendered frame.
    /// Output: platform-neutral InputState snapshots and named action values.
    /// Task: keep GLFW out of EngineCore gameplay APIs while preserving exact
    /// pressed/released transitions and GLFW's standardized gamepad mapping.
    class RuntimeInputBridge final
    {
    public:
        explicit RuntimeInputBridge(const kairo::engine::InputActionMap& map) noexcept
            : m_Map(map) {}

        /// Poll after Window::PollEvents and before gameplay simulation. Only
        /// keyboard and mouse controls referenced by the map are queried;
        /// standardized gamepad slots are refreshed in full so disconnects
        /// cannot leave stale buttons or axes held.
        void Poll(kairo::renderer::Window& window)
        {
            m_State.BeginFrame();
            GLFWwindow* native = window.NativeHandle();
            const auto [width,height]=window.FramebufferExtent();
            const bool active=glfwGetWindowAttrib(native,GLFW_FOCUSED)==GLFW_TRUE&&
                glfwGetWindowAttrib(native,GLFW_ICONIFIED)!=GLFW_TRUE&&width!=0&&height!=0;
            if(!active)
            {
                m_Keys.Reset();m_MouseButtons.Reset();m_MouseBaseline=false;
                for(auto& pad:m_Pads)pad.Reset();
            }
            // Establish the cursor baseline before applying new held controls,
            // so starting or resuming cannot manufacture a mouse movement.
            if(active)
            {
                double x=0,y=0;
                glfwGetCursorPos(native,&x,&y);
                if(std::isfinite(x)&&std::isfinite(y)&&
                    std::abs(x)<=1e9&&std::abs(y)<=1e9)
                {
                    m_State.SetMousePosition({static_cast<float>(x),static_cast<float>(y)});
                    if(!m_MouseBaseline){m_State.CommitFrame();m_MouseBaseline=true;}
                }
                else m_MouseBaseline=false;
            }
            for (const auto& binding : m_Map.Bindings())
            {
                switch (binding.Device)
                {
                    case kairo::engine::InputBindingDevice::Keyboard:
                        m_State.SetKeyDown(binding.Code,active&&m_Keys.Digital(static_cast<std::size_t>(binding.Code),
                            glfwGetKey(native, binding.Code) == GLFW_PRESS));
                        break;
                    case kairo::engine::InputBindingDevice::MouseButton:
                        m_State.SetMouseButtonDown(binding.Code,active&&m_MouseButtons.Digital(static_cast<std::size_t>(binding.Code),
                            glfwGetMouseButton(native, binding.Code) == GLFW_PRESS));
                        break;
                    case kairo::engine::InputBindingDevice::GamepadButton:
                    case kairo::engine::InputBindingDevice::GamepadAxis:
                        break;
                }
            }

            for (std::size_t slot = 0; slot < kairo::engine::InputState::MaximumGamepads; ++slot)
            {
                GLFWgamepadstate gamepad{};
                const int joystick = GLFW_JOYSTICK_1 + static_cast<int>(slot);
                if (!active || glfwJoystickIsGamepad(joystick) != GLFW_TRUE ||
                    glfwGetGamepadState(joystick, &gamepad) != GLFW_TRUE)
                {
                    m_Pads[slot].Reset();
                    m_State.ClearGamepad(slot);
                    continue;
                }
                for (std::size_t button = 0; button <= GLFW_GAMEPAD_BUTTON_LAST; ++button)
                    m_State.SetGamepadButtonDown(slot, button,
                        m_Pads[slot].Button(button,gamepad.buttons[button] == GLFW_PRESS));
                for (std::size_t axis = 0; axis <= GLFW_GAMEPAD_AXIS_LAST; ++axis)
                    m_State.SetGamepadAxis(slot,axis,m_Pads[slot].Axis(axis,gamepad.axes[axis],
                        axis==GLFW_GAMEPAD_AXIS_LEFT_TRIGGER||axis==GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER));
            }
        }

        [[nodiscard]] bool HasAction(std::string_view name) const noexcept
        {
            return m_Map.FindAction(name) != nullptr;
        }

        [[nodiscard]] kairo::engine::InputActionState Action(
            std::string_view name, std::size_t gamepad = 0u) const
        {
            return m_Map.Evaluate(name, m_State, gamepad);
        }

        [[nodiscard]] const kairo::engine::InputState& RawState() const noexcept { return m_State; }

    private:
        const kairo::engine::InputActionMap& m_Map;
        kairo::engine::InputState m_State;
        detail::RuntimeControlGate<512> m_Keys;
        detail::RuntimeControlGate<8> m_MouseButtons;
        std::array<detail::RuntimeGamepadGate,kairo::engine::InputState::MaximumGamepads> m_Pads;
        bool m_MouseBaseline=false;
    };
}
