#include "graphics/D3D11Device.h"

#include <array>

#include "util/Log.h"

namespace overlaydesk {

void D3D11Device::Create() {
    // BGRA support is mandatory: Windows.Graphics.Capture hands us B8G8R8A8 surfaces and
    // DirectComposition composites in BGRA.
    const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    // D3D11_CREATE_DEVICE_SINGLETHREADED is deliberately NOT set, even though every draw
    // this application issues happens on one thread (ADR-0006). A single-threaded device
    // refuses QueryInterface for ID3D11Multithread, and Direct3D11CaptureFramePool::Create
    // asks for exactly that - it fails with E_NOINTERFACE otherwise. Windows.Graphics.
    // Capture drives the frame pool from its own producer thread regardless of which
    // thread the FrameArrived event is dispatched to.

    constexpr std::array kFeatureLevels = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    auto createDevice = [&](UINT creationFlags) -> HRESULT {
        return ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags,
                                   kFeatureLevels.data(), static_cast<UINT>(kFeatureLevels.size()),
                                   D3D11_SDK_VERSION, m_device.put(), nullptr, m_context.put());
    };

    HRESULT hr = E_FAIL;
#if defined(_DEBUG)
    // Try the debug layer first; it is only present when the Graphics Tools optional
    // feature is installed, so a failure here is expected on plain machines.
    hr = createDevice(flags | D3D11_CREATE_DEVICE_DEBUG);
    if (SUCCEEDED(hr)) {
        LogInfo("D3D11: hardware device created with the debug layer.");
    } else {
        LogWarn("D3D11: debug layer unavailable (0x{:08X}); falling back to a normal device.",
                static_cast<uint32_t>(hr));
        m_device = nullptr;
        m_context = nullptr;
    }
#endif

    if (!m_device) {
        hr = createDevice(flags);
    }

    if (FAILED(hr)) {
        // A machine without a usable hardware adapter still deserves a running overlay, so
        // fall back to WARP rather than refusing to start.
        LogWarn("D3D11: hardware device creation failed (0x{:08X}); trying WARP.",
                static_cast<uint32_t>(hr));
        m_device = nullptr;
        m_context = nullptr;
        hr = ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
                                 kFeatureLevels.data(), static_cast<UINT>(kFeatureLevels.size()),
                                 D3D11_SDK_VERSION, m_device.put(), nullptr, m_context.put());
    }

    OS_CHECK_HR(hr);

    m_dxgiDevice = m_device.as<IDXGIDevice>();

    // One frame of queued work is enough for an overlay. The default of 3 adds latency we
    // cannot spend (PERFORMANCE.md ranks low latency second only to stability).
    if (auto dxgiDevice1 = m_dxgiDevice.try_as<IDXGIDevice1>()) {
        dxgiDevice1->SetMaximumFrameLatency(1);
    }

    OS_CHECK_HR(m_dxgiDevice->GetAdapter(m_adapter.put()));

    com_ptr<IDXGIFactory2> factory;
    OS_CHECK_HR(m_adapter->GetParent(winrt::guid_of<IDXGIFactory2>(), factory.put_void()));
    m_dxgiFactory = std::move(factory);

    DXGI_ADAPTER_DESC adapterDesc{};
    if (SUCCEEDED(m_adapter->GetDesc(&adapterDesc))) {
        LogInfo("D3D11: device on '{}' (feature level {:#x}, {} MB dedicated VRAM).",
                winrt::to_string(adapterDesc.Description),
                static_cast<uint32_t>(m_device->GetFeatureLevel()),
                adapterDesc.DedicatedVideoMemory / (1024ull * 1024ull));
    } else {
        LogInfo("D3D11: device created (feature level {:#x}).",
                static_cast<uint32_t>(m_device->GetFeatureLevel()));
    }
}

void D3D11Device::Reset() noexcept {
    if (m_context) {
        m_context->ClearState();
        m_context->Flush();
    }
    m_dxgiFactory = nullptr;
    m_adapter = nullptr;
    m_dxgiDevice = nullptr;
    m_context = nullptr;
    m_device = nullptr;
}

HRESULT D3D11Device::DeviceRemovedReason() const noexcept {
    return m_device ? m_device->GetDeviceRemovedReason() : E_POINTER;
}

void D3D11Device::Trim() const noexcept {
    if (auto dxgiDevice3 = m_dxgiDevice.try_as<IDXGIDevice3>()) {
        dxgiDevice3->Trim();
    }
}

}  // namespace overlaydesk
