#pragma once

// Include after importing Kairo.Renderer. Both authored 2D hosts depend on
// the viewport-overlay contract, which is currently Metal/Vulkan only.
namespace kairo::player::detail
{
    [[nodiscard]] inline kairo::renderer::GraphicsBackend SelectOverlayBackend(
        kairo::renderer::GraphicsBackend requested,
        kairo::renderer::GraphicsBackendAvailability available,
        kairo::renderer::HostPlatform platform)
    {
        using namespace kairo::renderer;
        if (requested == GraphicsBackend::Direct3D12 || requested == GraphicsBackend::OpenGL)
            throw std::invalid_argument("2D viewport overlays require Metal or Vulkan; use --renderer auto, metal, or vulkan.");
        // Filter the native-first policy before selecting, rather than
        // constructing an incompatible renderer and failing after startup.
        available.Direct3D12 = false;
        available.OpenGL = false;
        return SelectGraphicsBackend(requested, available, platform);
    }
}
