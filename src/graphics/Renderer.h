#pragma once

// The overlay renderer. One draw call, one shader, no intermediate render targets
// (ADR-0003), and no resource creation once Create has run (PERFORMANCE.md section 4).

#include <d3d11_4.h>

#include "core/AppState.h"
#include "graphics/D3D11Device.h"
#include "graphics/RenderResources.h"
#include "graphics/ShaderManager.h"
#include "graphics/SwapChain.h"

namespace overlaydesk {

struct RenderFrame {
    const AppSettings* settings = nullptr;

    // Null while no capture is running. The shader then paints the idle backdrop instead
    // of sampling, so the overlay stays visible and positionable.
    ID3D11ShaderResourceView* source = nullptr;
    SourceGeometry sourceGeometry;

    bool editMode = false;
    float timeSeconds = 0.0f;
    float editBorderThickness = 4.0f;
};

class Renderer {
public:
    Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void Create(const D3D11Device& device, HWND overlayWindow, uint32_t width, uint32_t height);
    void Reset() noexcept;

    bool IsValid() const noexcept { return m_swapChain.IsValid() && m_shaders.IsValid(); }

    bool Resize(uint32_t width, uint32_t height);

    uint32_t Width() const noexcept { return m_swapChain.Width(); }
    uint32_t Height() const noexcept { return m_swapChain.Height(); }

    // Returns false when presentation failed in a way that means the device is gone; the
    // caller tears the graphics stack down and rebuilds rather than retrying per frame.
    bool Render(const RenderFrame& frame);

    ShaderManager& Shaders() noexcept { return m_shaders; }

private:
    const D3D11Device* m_device = nullptr;
    SwapChain m_swapChain;
    RenderResources m_resources;
    ShaderManager m_shaders;
};

}  // namespace overlaydesk
