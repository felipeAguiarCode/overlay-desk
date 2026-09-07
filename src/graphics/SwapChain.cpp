#include "graphics/SwapChain.h"

#include "util/Log.h"

namespace overlaydesk {
namespace {

// PERFORMANCE.md: 1080p BGRA is ~8 MB per buffer. Two is the minimum a flip-model swap
// chain accepts and all an overlay needs.
constexpr UINT kBufferCount = 2;

constexpr UINT ClampExtent(UINT value) noexcept {
    return value > 0 ? value : 1;
}

}  // namespace

void SwapChain::CreateForComposition(const D3D11Device& device, HWND window, UINT width,
                                     UINT height) {
    Reset();

    m_device = device.Device();
    m_width = ClampExtent(width);
    m_height = ClampExtent(height);

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = kBufferCount;
    // Composition swap chains must stretch; DXGI rejects any other scaling mode here.
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    HRESULT hr = device.DxgiFactory()->CreateSwapChainForComposition(device.Device(), &desc, nullptr,
                                                                     m_swapChain.put());
    if (FAILED(hr)) {
        // FLIP_DISCARD needs Windows 10; FLIP_SEQUENTIAL is the older contract and is all
        // a two-buffer overlay actually requires.
        LogWarn("SwapChain: FLIP_DISCARD composition swap chain refused (0x{:08X}); retrying "
                "with FLIP_SEQUENTIAL.",
                static_cast<uint32_t>(hr));
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        hr = device.DxgiFactory()->CreateSwapChainForComposition(device.Device(), &desc, nullptr,
                                                                 m_swapChain.put());
    }
    OS_CHECK_HR(hr);

    OS_CHECK_HR(::DCompositionCreateDevice(device.DxgiDevice(),
                                           winrt::guid_of<IDCompositionDevice>(),
                                           m_compositionDevice.put_void()));
    OS_CHECK_HR(m_compositionDevice->CreateTargetForHwnd(window, /*topmost=*/TRUE,
                                                         m_compositionTarget.put()));
    OS_CHECK_HR(m_compositionDevice->CreateVisual(m_compositionVisual.put()));
    OS_CHECK_HR(m_compositionVisual->SetContent(m_swapChain.get()));
    OS_CHECK_HR(m_compositionTarget->SetRoot(m_compositionVisual.get()));
    OS_CHECK_HR(m_compositionDevice->Commit());

    LogInfo("SwapChain: composition swap chain created ({}x{}, premultiplied alpha).", m_width,
            m_height);
}

void SwapChain::CreateForHwnd(const D3D11Device& device, HWND window, UINT width, UINT height) {
    Reset();

    m_device = device.Device();
    m_width = ClampExtent(width);
    m_height = ClampExtent(height);

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = kBufferCount;
    desc.Scaling = DXGI_SCALING_NONE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    OS_CHECK_HR(device.DxgiFactory()->CreateSwapChainForHwnd(device.Device(), window, &desc, nullptr,
                                                             nullptr, m_swapChain.put()));

    // The control panel has no business going exclusive fullscreen on Alt+Enter.
    device.DxgiFactory()->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);

    LogInfo("SwapChain: control panel swap chain created ({}x{}).", m_width, m_height);
}

void SwapChain::Reset() noexcept {
    ReleaseRenderTarget();

    if (m_compositionTarget) {
        m_compositionTarget->SetRoot(nullptr);
    }
    if (m_compositionVisual) {
        m_compositionVisual->SetContent(nullptr);
    }
    if (m_compositionDevice) {
        m_compositionDevice->Commit();
    }

    m_compositionVisual = nullptr;
    m_compositionTarget = nullptr;
    m_compositionDevice = nullptr;
    m_swapChain = nullptr;
    m_device = nullptr;
    m_width = 0;
    m_height = 0;
}

void SwapChain::ReleaseRenderTarget() noexcept {
    m_renderTargetView = nullptr;
}

bool SwapChain::Resize(UINT width, UINT height) {
    if (!m_swapChain) {
        return false;
    }

    width = ClampExtent(width);
    height = ClampExtent(height);
    if (width == m_width && height == m_height) {
        return true;
    }

    // ResizeBuffers fails outright while anything still references a back buffer, so the
    // render target view has to go first. This is exactly the path AT-007 exercises.
    ReleaseRenderTarget();

    const HRESULT hr = m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        LogError("SwapChain: ResizeBuffers({}, {}) failed: 0x{:08X}", width, height,
                 static_cast<uint32_t>(hr));
        return false;
    }

    m_width = width;
    m_height = height;
    return true;
}

ID3D11RenderTargetView* SwapChain::RenderTargetView() {
    if (m_renderTargetView) {
        return m_renderTargetView.get();
    }
    if (!m_swapChain || m_device == nullptr) {
        return nullptr;
    }

    com_ptr<ID3D11Texture2D> backBuffer;
    if (FAILED(m_swapChain->GetBuffer(0, winrt::guid_of<ID3D11Texture2D>(), backBuffer.put_void()))) {
        return nullptr;
    }
    if (FAILED(m_device->CreateRenderTargetView(backBuffer.get(), nullptr,
                                                m_renderTargetView.put()))) {
        return nullptr;
    }
    return m_renderTargetView.get();
}

HRESULT SwapChain::Present(UINT syncInterval) noexcept {
    if (!m_swapChain) {
        return DXGI_ERROR_INVALID_CALL;
    }
    return m_swapChain->Present(syncInterval, 0);
}

}  // namespace overlaydesk
