#pragma once

// ARCHITECTURE.md section 2: one primary device for the whole process. The overlay swap
// chain, the control panel swap chain and the capture frame pool all share it, which is
// what keeps the captured texture usable without a cross-device copy.

#include <d3d11_4.h>
#include <dxgi1_6.h>

#include "util/ComHelpers.h"

namespace overlaydesk {

class D3D11Device {
public:
    D3D11Device() = default;
    D3D11Device(const D3D11Device&) = delete;
    D3D11Device& operator=(const D3D11Device&) = delete;

    // Throws winrt::hresult_error if no suitable device can be created.
    void Create();
    void Reset() noexcept;

    ID3D11Device* Device() const noexcept { return m_device.get(); }
    ID3D11DeviceContext* Context() const noexcept { return m_context.get(); }
    IDXGIDevice* DxgiDevice() const noexcept { return m_dxgiDevice.get(); }
    IDXGIFactory2* DxgiFactory() const noexcept { return m_dxgiFactory.get(); }

    bool IsValid() const noexcept { return static_cast<bool>(m_device); }

    // Returns the device-removed reason, or S_OK while the device is healthy. Callers use
    // this to decide whether to tear down and rebuild instead of looping on failures.
    HRESULT DeviceRemovedReason() const noexcept;

    // Frees any deferred deletions the runtime is still holding. Called on pause, never
    // per frame.
    void Trim() const noexcept;

private:
    com_ptr<ID3D11Device> m_device;
    com_ptr<ID3D11DeviceContext> m_context;
    com_ptr<IDXGIDevice> m_dxgiDevice;
    com_ptr<IDXGIAdapter> m_adapter;
    com_ptr<IDXGIFactory2> m_dxgiFactory;
};

}  // namespace overlaydesk
