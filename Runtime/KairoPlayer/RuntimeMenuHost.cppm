module;

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "RuntimeMenuFocus.hpp"
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
            const auto target=Hit(window,width,height);
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
            Publish(width,height);
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
        // GLFW pointer coordinates are logical window pixels; the renderer
        // layout is physical framebuffer pixels (e.g. Retina 2x).
        [[nodiscard]] std::optional<std::size_t> Hit(GLFWwindow* window,std::uint32_t width,std::uint32_t height) const
        {
            int ww=0,wh=0;glfwGetWindowSize(window,&ww,&wh);
            if(ww<=0||wh<=0)return {};
            double x=0,y=0;glfwGetCursorPos(window,&x,&y);x*=double(width)/ww;y*=double(height)/wh;
            for(std::size_t i=0;i<m_Order.size();++i)
            {
                const auto r=m_UI.Layout(m_Order[i],width,height);
                if(x>=r.X&&x<r.X+r.Width&&y>=r.Y&&y<r.Y+r.Height)return i;
            }
            return {};
        }
        void Clear()
        {
            m_Renderer.SetViewportOverlay({});
            glfwSetWindowTitle(m_Renderer.NativeWindow().NativeHandle(),m_Title.c_str());m_LastTitle.clear();
        }
        // Rectangles are opaque and ordered. A contrasting yellow focus
        // border is independent of the label color. Limit glyph instances
        // to the renderer's 4096-rectangle budget, and fit to each button.
        // Full focused label remains available in the native window title.
        void Publish(std::uint32_t width,std::uint32_t height)
        {
            using kairo::renderer::ViewportOverlayRect;
            std::vector<ViewportOverlayRect> rects;
            const auto add=[&](int x,int y,int w,int h,std::array<float,3> color)
            {
                if(w>0&&h>0)rects.push_back({x,y,static_cast<std::uint32_t>(w),static_cast<std::uint32_t>(h),color});
            };
            const auto glyphBudget=(4096u-2u*m_Order.size())/(35u*m_Order.size());
            for(std::size_t i=0;i<m_Order.size();++i)
            {
                const auto r=m_UI.Layout(m_Order[i],width,height);
                const int x=static_cast<int>(r.X),y=static_cast<int>(r.Y),w=static_cast<int>(r.Width),h=static_cast<int>(r.Height);
                const bool selected=i==m_Focus.Index();
                add(x,y,w,h,selected?std::array<float,3>{1,.75f,.15f}:std::array<float,3>{.45f,.5f,.55f});
                add(x+3,y+3,w-6,h-6,{.035f,.045f,.065f});
                auto label=m_UI.Text(m_Order[i],"en");
                label.resize(std::min(label.size(),glyphBudget));
                if(label.empty()||w<=8||h<=8)continue;
                const int scale=std::min({m_LargeText?6:3,(w-8)/static_cast<int>(label.size()*6),(h-8)/7});
                if(scale<1)continue;
                const int left=x+(w-static_cast<int>(label.size()*6)*scale)/2,top=y+(h-7*scale)/2;
                for(std::size_t c=0;c<label.size();++c)
                {
                    const auto rows=detail::MenuGlyph(label[c]);
                    for(unsigned row=0;row<7;++row)for(unsigned col=0;col<5;++col)
                        if(rows[row]&(1u<<(4-col)))add(left+static_cast<int>(c*6+col)*scale,top+static_cast<int>(row)*scale,scale,scale,{1,1,1});
                }
            }
            m_Renderer.SetViewportOverlay(rects);
            const auto title=m_Title+" | "+m_UI.AccessibleLabel(m_Order[m_Focus.Index()],"en")+" | Tab/Enter, F2 text size, F1 menu";
            if(title!=m_LastTitle){glfwSetWindowTitle(m_Renderer.NativeWindow().NativeHandle(),title.c_str());m_LastTitle=title;}
        }
    };
}
