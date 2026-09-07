#include "graphics/Renderer.h"

#include "util/Log.h"

namespace overlaydesk {

void Renderer::Create(const D3D11Device& device, HWND overlayWindow, uint32_t width,
                      uint32_t height) {
    Reset();

    m_device = &device;
    m_swapChain.CreateForComposition(device, overlayWindow, width, height);
    m_resources.Create(device.Device());
    m_shaders.CreateFromEmbedded(device.Device());

    LogInfo("Renderer: overlay renderer ready ({}x{}).", m_swapChain.Width(), m_swapChain.Height());
}

void Renderer::Reset() noexcept {
    m_shaders.Reset();
    m_resources.Reset();
    m_swapChain.Reset();
    m_device = nullptr;
}

bool Renderer::Resize(uint32_t width, uint32_t height) {
    return m_swapChain.Resize(width, height);
}

bool Renderer::Render(const RenderFrame& frame) {
    if (!IsValid() || m_device == nullptr || frame.settings == nullptr) {
        return true;
    }

    ID3D11RenderTargetView* renderTarget = m_swapChain.RenderTargetView();
    if (renderTarget == nullptr) {
        return false;
    }

    ID3D11DeviceContext* context = m_device->Context();
    const uint32_t width = m_swapChain.Width();
    const uint32_t height = m_swapChain.Height();

    const ShaderConstants constants =
        BuildShaderConstants(*frame.settings, width, height, frame.sourceGeometry,
                             frame.timeSeconds, frame.source != nullptr, frame.editMode,
                             frame.editBorderThickness);
    m_resources.UpdateConstants(context, constants);

    // Fully transparent, not black: everything the shader chooses not to cover - the
    // letterbox bars in particular - must let the window underneath show through.
    constexpr float kTransparent[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    context->ClearRenderTargetView(renderTarget, kTransparent);

    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);
    context->RSSetState(m_resources.RasterizerState());

    // The fullscreen triangle comes from SV_VertexID, so there is nothing to bind on the
    // input assembler.
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(m_shaders.VertexShader(), nullptr, 0);
    context->PSSetShader(m_shaders.PixelShader(), nullptr, 0);

    ID3D11Buffer* constantBuffers[] = {m_resources.ConstantBuffer()};
    context->PSSetConstantBuffers(0, 1, constantBuffers);

    ID3D11SamplerState* samplers[] = {m_resources.Sampler()};
    context->PSSetSamplers(0, 1, samplers);

    ID3D11ShaderResourceView* sources[] = {frame.source};
    context->PSSetShaderResources(0, 1, sources);

    ID3D11RenderTargetView* renderTargets[] = {renderTarget};
    context->OMSetRenderTargets(1, renderTargets, nullptr);
    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFFu);

    context->Draw(3, 0);

    // Unbind before returning. The SRV wraps a texture the capture frame pool owns and
    // will recycle the moment we close the frame; leaving it bound would keep a reference
    // alive across the pool's rotation.
    ID3D11ShaderResourceView* noSource[] = {nullptr};
    context->PSSetShaderResources(0, 1, noSource);
    ID3D11RenderTargetView* noTargets[] = {nullptr};
    context->OMSetRenderTargets(1, noTargets, nullptr);

    // No vsync: DirectComposition schedules the actual screen update, and blocking here
    // would stall the message loop that the capture callback runs on.
    const HRESULT hr = m_swapChain.Present(0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        LogError("Renderer: device removed during Present (0x{:08X}).", static_cast<uint32_t>(hr));
        return false;
    }
    if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) {
        LogError("Renderer: Present failed (0x{:08X}).", static_cast<uint32_t>(hr));
        return false;
    }

    return true;
}

}  // namespace overlaydesk
