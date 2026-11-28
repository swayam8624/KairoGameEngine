#include <stdexcept>
#include <string_view>

import Kairo.Renderer.GraphicsBackend;
#include "../OverlayBackend.hpp"

using namespace kairo::renderer;
using kairo::player::detail::SelectOverlayBackend;

int main()
{
    const GraphicsBackendAvailability all{true, true, true, true};
    using B = GraphicsBackend;
    using P = HostPlatform;
    if (SelectOverlayBackend(B::Automatic, all, P::Windows) != B::Vulkan) return 1;
    if (SelectOverlayBackend(B::Automatic, all, P::MacOS) != B::Metal) return 2;
    if (SelectOverlayBackend(B::Automatic, all, P::Linux) != B::Vulkan) return 3;
    if (SelectOverlayBackend(B::Automatic, {true, false, true, true}, P::MacOS) != B::Vulkan) return 4;
    if (SelectOverlayBackend(B::Vulkan, all, P::MacOS) != B::Vulkan) return 5;
    if (SelectOverlayBackend(B::Metal, all, P::MacOS) != B::Metal) return 6;
    // The regular 3D preference must not change.
    if (SelectGraphicsBackend(B::Automatic, all, P::Windows) != B::Direct3D12) return 7;
    for (const auto backend : {B::Direct3D12, B::OpenGL})
    {
        try { (void)SelectOverlayBackend(backend, all, P::Windows); return 8; }
        catch (const std::invalid_argument& error)
        {
            if (std::string_view(error.what()).find("2D viewport overlays") == std::string_view::npos) return 9;
        }
    }
    try { (void)SelectOverlayBackend(B::Metal, all, P::Windows); return 10; }
    catch (const std::invalid_argument&) {}
    try { (void)SelectOverlayBackend(B::Vulkan, {false, true, true, true}, P::MacOS); return 11; }
    catch (const std::runtime_error&) {}
    try { (void)SelectOverlayBackend(B::Automatic, {false, false, true, true}, P::Windows); return 12; }
    catch (const std::runtime_error&) {}
    return 0;
}
