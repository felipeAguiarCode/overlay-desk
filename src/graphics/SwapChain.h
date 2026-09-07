#pragma once

// Two presentation paths share this class:
//
//   Composition - the overlay (ADR-0006). Created with CreateSwapChainForComposition and
//                 handed to DirectComposition, which lets the window carry per-pixel alpha
//                 without a DWM redirection surface. WS_EX_LAYERED is deliberately not
//                 used: flip-model swap chains do not compose correctly with it, and the
//                 redirection bitmap it implies would cost another full-size surface.
//
//   Hwnd        - the ImGui control panel. An ordinary opaque flip-model swap chain.

#include <d3d11_4.h>
#include <dcomp.h>
#include <dxgi1_6.h>

#include "graphics/D3D11Device.h"
#include "util/ComHelpers.h"

namespace overlaydesk {

class SwapChain {
public:
    SwapChain() = default;
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;
    ~SwapChain() { Reset(); }

    void CreateForComposition(const D3D11Device& device, HWND window, UINT width, UINT height);
    void CreateForHwnd(const D3D11Device& device, HWND window, UINT width, UINT height);

    void Reset() noexcept;

    bool IsValid() const noexcept { return static_cast<bool>(m_swapChain); }
    UINT Width() const noexcept { return m_width; }
    UINT Height() const noexcept { return m_height; }

    // No-op when the size is unchanged or degenerate (a minimized window reports 0x0).
    // Returns false when the swap chain could not be resized, which the caller treats as
    // "device is gone, rebuild".
    bool Resize(UINT width, UINT height);

    // Created lazily after Create/Resize so the back buffer is only ever held for as long
    // as it is actually being drawn into.
    ID3D11RenderTargetView* RenderTargetView();

    HRESULT Present(UINT syncInterval) noexcept;

private:
    void ReleaseRenderTarget() noexcept;

    com_ptr<IDXGISwapChain1> m_swapChain;
    com_ptr<ID3D11RenderTargetView> m_renderTargetView;

    // DirectComposition objects, only populated in Composition mode. The visual holds a
    // reference to the swap chain, so all three must be released together.
    com_ptr<IDCompositionDevice> m_compositionDevice;
    com_ptr<IDCompositionTarget> m_compositionTarget;
    com_ptr<IDCompositionVisual> m_compositionVisual;

    ID3D11Device* m_device = nullptr;
    UINT m_width = 0;
    UINT m_height = 0;
};

}  // namespace overlaydesk
