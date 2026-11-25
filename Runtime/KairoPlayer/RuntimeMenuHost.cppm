module;

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "RuntimeMenuPresentation.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include <stdexcept>

export module Kairo.Player.RuntimeMenuHost;

import Kairo.EngineCore.ShippingRuntime;
import Kairo.Renderer;

export namespace kairo::player
{
    // The existing shipping scene owns authored layout, labels and actions.
    // This native host owns only transient focus/input and overlay geometry.
    // No menu is installed on projects without Config/Menu.kmenu.
    class RuntimeMenuHost
    {
    public:
        RuntimeMenuHost(kairo::engine::RuntimeUIScene& ui,kairo::renderer::RendererRuntime& renderer,std::string title)
            :m_UI(ui),m_Renderer(renderer),m_Order(ui.FocusOrder()),m_Focus(m_Order.size()),m_Title(std::move(title))
        {
            if(!renderer.SupportsViewportOverlay())throw std::runtime_error("Authored menu requires a native viewport overlay backend (Vulkan or Metal).");
        }
        [[nodiscard]] bool Active() const noexcept{return m_Active;}
        // Returns true when this frame belongs to the menu, even when a
        // resume action closes it. The game must not consume the same input.
        bool Poll(std::uint32_t width,std::uint32_t height)
        {
            auto* window=m_Renderer.NativeWindow().NativeHandle();
            const bool focused=glfwGetWindowAttrib(window,GLFW_FOCUSED)==GLFW_TRUE&&width&&height;
            if(!focused)
            {
                ResetEdges();m_Toggle.Reset();return m_Active;
            }
            const auto key=[&](int code){return glfwGetKey(window,code)==GLFW_PRESS;};
            if(m_Toggle.Press(key(GLFW_KEY_F1)))
            {
                m_Active=!m_Active;ResetEdges();
                if(!m_Active){Clear();return true;}
            }
            if(!m_Active)return false;
            const bool back=key(GLFW_KEY_LEFT_SHIFT)||key(GLFW_KEY_RIGHT_SHIFT);
            if(m_Tab.Press(key(GLFW_KEY_TAB))){m_Focus.Move(back);m_Focus.CancelPointer();}
            if(m_Up.Press(key(GLFW_KEY_UP))){m_Focus.Move(true);m_Focus.CancelPointer();}
            if(m_Down.Press(key(GLFW_KEY_DOWN))){m_Focus.Move(false);m_Focus.CancelPointer();}
            if(m_Scale.Press(key(GLFW_KEY_F2)))m_LargeText=!m_LargeText;
            // Build one set of physical boxes for both pointer and presentation.
            const auto buttons=VisualButtons(width,height);
            const auto target=Hit(window,width,height,buttons);
            const bool down=glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
            if(m_Mouse.Press(down)){m_Focus.PointerDown(target);m_PointerHeld=true;}
            bool activate=m_Confirm.Press(key(GLFW_KEY_ENTER)||key(GLFW_KEY_SPACE));
            if(!down&&m_PointerHeld)
            {
                m_PointerHeld=false;
                if(m_Focus.PointerUp(target)){m_Focus.Select(*target);activate=true;}
            }
            if(activate)
            {
                const auto action=m_UI.Activate(m_Order[m_Focus.Index()]);
                if(action=="quit"){m_Renderer.NativeWindow().RequestClose();return true;}
                if(action=="resume"){m_Active=false;Clear();return true;}
            }
            Publish(buttons);
            return true;
        }
    private:
        kairo::engine::RuntimeUIScene& m_UI;
        kairo::renderer::RendererRuntime& m_Renderer;
        std::vector<std::string> m_Order;
        detail::RuntimeMenuFocus m_Focus;
        std::string m_Title,m_LastTitle;
        detail::MenuEdge m_Tab,m_Up,m_Down,m_Confirm,m_Mouse,m_Toggle,m_Scale;
        bool m_Active=true,m_PointerHeld=false,m_LargeText=false;
        void ResetEdges() noexcept
        {
            m_Tab.Reset();m_Up.Reset();m_Down.Reset();m_Confirm.Reset();m_Mouse.Reset();m_Scale.Reset();
            m_Focus.CancelPointer();m_PointerHeld=false;
        }
        // Layout remains owned by the shipping model. Quantize at the renderer
        // boundary once per frame, with the same boxes used for hit testing.
        [[nodiscard]] std::vector<detail::MenuVisualButton> VisualButtons(std::uint32_t width,std::uint32_t height) const
        {
            std::vector<detail::MenuVisualButton> buttons;buttons.reserve(m_Order.size());
            for(const auto& id:m_Order)
            {
                const auto r=m_UI.Layout(id,width,height);
                buttons.push_back({detail::QuantizeMenuBox(r.X,r.Y,r.Width,r.Height,width,height),m_UI.Text(id,"en")});
            }
            return buttons;
        }
        // GLFW pointer coordinates are logical window pixels; the renderer
        // layout is physical framebuffer pixels (e.g. Retina 2x).
        [[nodiscard]] static std::optional<std::size_t> Hit(GLFWwindow* window,std::uint32_t width,std::uint32_t height,
            std::span<const detail::MenuVisualButton> buttons)
        {
            int ww=0,wh=0;glfwGetWindowSize(window,&ww,&wh);
            if(ww<=0||wh<=0)return {};
            double x=0,y=0;glfwGetCursorPos(window,&x,&y);x*=double(width)/ww;y*=double(height)/wh;
            return detail::HitMenu(buttons,x,y);
        }
        void Clear()
        {
            m_Renderer.SetViewportOverlay({});
            glfwSetWindowTitle(m_Renderer.NativeWindow().NativeHandle(),m_Title.c_str());m_LastTitle.clear();
        }
        // Translate tested portable overlay values into renderer-owned
        // rectangles; no additional geometry policy lives in this host.
        void Publish(std::span<const detail::MenuVisualButton> buttons)
        {
            const auto overlay=detail::BuildMenuOverlay(buttons,m_Focus.Index(),m_LargeText);
            std::vector<kairo::renderer::ViewportOverlayRect> rects;rects.reserve(overlay.size());
            for(const auto& rect:overlay)
            {
                const auto& r=rect.Box;
                rects.push_back({r.X,r.Y,static_cast<std::uint32_t>(r.W),static_cast<std::uint32_t>(r.H),rect.Color});
            }
            m_Renderer.SetViewportOverlay(rects);
            const auto title=m_Title+" | "+m_UI.AccessibleLabel(m_Order[m_Focus.Index()],"en")+" | Tab/Enter, F2 text size, F1 menu";
            if(title!=m_LastTitle){glfwSetWindowTitle(m_Renderer.NativeWindow().NativeHandle(),title.c_str());m_LastTitle=title;}
        }
    };
}
