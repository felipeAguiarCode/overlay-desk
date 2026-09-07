#include "capture/CaptureSession.h"

#include <DispatcherQueue.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.System.h>

#include <format>

#include "util/Log.h"
#include "util/Win32Helpers.h"

namespace overlaydesk {
namespace {

namespace wgc = winrt::Windows::Graphics::Capture;
namespace wgdx = winrt::Windows::Graphics::DirectX;
namespace wgdx11 = winrt::Windows::Graphics::DirectX::Direct3D11;

using winrt::Windows::Foundation::Metadata::ApiInformation;
using winrt::Windows::Graphics::SizeInt32;

// PERFORMANCE.md: two buffers. More would only add latency and another full-size surface.
constexpr int32_t kFramePoolBuffers = 2;
constexpr auto kCaptureFormat = wgdx::DirectXPixelFormat::B8G8R8A8UIntNormalized;

// SystemRelativeTime is in 100 ns ticks.
constexpr int64_t kTicksPerSecond = 10'000'000;

bool IsPropertyAvailable(const wchar_t* type, const wchar_t* property) {
    try {
        return ApiInformation::IsPropertyPresent(type, property);
    } catch (...) {
        return false;
    }
}

}  // namespace

// --------------------------------------------------------------------------------------
// Impl - everything that touches the C++/WinRT projection lives here so the header can
// stay a plain Win32/D3D header.
// --------------------------------------------------------------------------------------

struct CaptureSession::Impl {
    explicit Impl(CaptureSession& owner) : owner(owner) {}

    CaptureSession& owner;

    winrt::Windows::System::DispatcherQueueController dispatcherController{nullptr};
    wgdx11::IDirect3DDevice winrtDevice{nullptr};

    wgc::GraphicsCaptureItem item{nullptr};
    wgc::Direct3D11CaptureFramePool framePool{nullptr};
    wgc::GraphicsCaptureSession session{nullptr};

    winrt::event_token frameArrivedToken{};
    winrt::event_token itemClosedToken{};

    SizeInt32 poolSize{0, 0};

    // Rolling one-second window for the reported FPS.
    int64_t fpsWindowStartTicks = 0;
    uint32_t fpsWindowFrames = 0;

    void OnFrameArrived(const wgc::Direct3D11CaptureFramePool& sender);
};

void CaptureSession::Impl::OnFrameArrived(const wgc::Direct3D11CaptureFramePool& sender) {
    // Runs on the message-loop thread. Nothing in here may log per frame or allocate.
    wgc::Direct3D11CaptureFrame frame{nullptr};
    try {
        frame = sender.TryGetNextFrame();
    } catch (...) {
        return;
    }
    if (frame == nullptr) {
        return;
    }

    ++owner.m_stats.framesArrived;
    if (owner.m_stats.framesArrived == 1) {
        // Once per session, not per frame: this single line is the difference between
        // "capture is silently dead" and "capture is running but the source is static".
        LogInfo("Capture: first frame arrived ({}x{}).", frame.ContentSize().Width,
                frame.ContentSize().Height);
    }

    const SizeInt32 contentSize = frame.ContentSize();
    const bool sizeChanged = contentSize.Width != poolSize.Width ||
                             contentSize.Height != poolSize.Height;

    bool rendered = false;

    // RNF-004 / AT-015. A paused session still has to drain the pool, otherwise
    // TryGetNextFrame would stall once every buffer is outstanding.
    if (owner.m_paused) {
        ++owner.m_stats.framesDroppedWhilePaused;
    } else {
        const int64_t nowTicks = frame.SystemRelativeTime().count();
        const int64_t minimumTicks =
            static_cast<int64_t>(owner.m_frameIntervalSeconds * static_cast<double>(kTicksPerSecond));

        // RNF-003. Dropping the frame before touching the GPU is the whole point of the
        // cap: a capped session must cost less, not merely present less.
        const bool tooSoon = minimumTicks > 0 && owner.m_lastRenderedTicks != 0 &&
                             (nowTicks - owner.m_lastRenderedTicks) < minimumTicks;

        if (tooSoon) {
            ++owner.m_stats.framesDroppedByPacing;
        } else {
            try {
                auto access = frame.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
                com_ptr<ID3D11Texture2D> texture;
                if (SUCCEEDED(access->GetInterface(winrt::guid_of<ID3D11Texture2D>(),
                                                   texture.put_void()))) {
                    if (ID3D11ShaderResourceView* view = owner.ViewForTexture(texture.get())) {
                        D3D11_TEXTURE2D_DESC desc{};
                        texture->GetDesc(&desc);

                        SourceGeometry geometry;
                        geometry.contentWidth = static_cast<uint32_t>(contentSize.Width);
                        geometry.contentHeight = static_cast<uint32_t>(contentSize.Height);
                        geometry.textureWidth = desc.Width;
                        geometry.textureHeight = desc.Height;

                        owner.m_stats.sourceWidth = geometry.contentWidth;
                        owner.m_stats.sourceHeight = geometry.contentHeight;

                        owner.m_lastFrameView.copy_from(view);
                        owner.m_lastFrameGeometry = geometry;

                        if (owner.m_callbacks.onFrame) {
                            owner.m_callbacks.onFrame(view, geometry);
                        }

                        owner.m_lastRenderedTicks = nowTicks;
                        ++owner.m_stats.framesRendered;
                        rendered = true;
                        if (owner.m_stats.framesRendered == 1) {
                            LogInfo("Capture: first frame rendered.");
                        }
                    }
                }
            } catch (...) {
                // RNF-009: a bad frame must not take the process down. The next one
                // usually lands fine; a persistent failure shows up as a stalled FPS
                // counter in the control panel.
            }
        }

        if (rendered) {
            ++fpsWindowFrames;
            if (fpsWindowStartTicks == 0) {
                fpsWindowStartTicks = owner.m_lastRenderedTicks;
            } else {
                const int64_t elapsed = owner.m_lastRenderedTicks - fpsWindowStartTicks;
                if (elapsed >= kTicksPerSecond) {
                    owner.m_stats.renderFps = static_cast<float>(
                        static_cast<double>(fpsWindowFrames) * kTicksPerSecond /
                        static_cast<double>(elapsed));
                    fpsWindowStartTicks = owner.m_lastRenderedTicks;
                    fpsWindowFrames = 0;
                }
            }
        }
    }

    // Hand the buffer back before doing anything else with the pool.
    frame.Close();

    // ARCHITECTURE.md section 5, source resize: the pool must be rebuilt at the new size.
    // Doing it after the frame is closed avoids recreating the pool while one of its
    // surfaces is still outstanding.
    if (sizeChanged) {
        poolSize = contentSize;
        owner.InvalidateViewCache();
        try {
            framePool.Recreate(winrtDevice, kCaptureFormat, kFramePoolBuffers, contentSize);
            LogInfo("Capture: source resized to {}x{}; frame pool recreated.", contentSize.Width,
                    contentSize.Height);
        } catch (...) {
            LogError("Capture: frame pool recreate failed: {}", DescribeCurrentException());
        }
    }
}

// --------------------------------------------------------------------------------------
// CaptureSession
// --------------------------------------------------------------------------------------

CaptureSession::CaptureSession() = default;

CaptureSession::~CaptureSession() {
    Shutdown();
}

bool CaptureSession::IsSupported() noexcept {
    try {
        return wgc::GraphicsCaptureSession::IsSupported();
    } catch (...) {
        return false;
    }
}

bool CaptureSession::Initialize(const D3D11Device& device) {
    if (m_impl) {
        return true;
    }

    m_device = &device;
    m_impl = std::make_unique<Impl>(*this);

    try {
        // ADR-0006: a DispatcherQueue on this thread is what makes FrameArrived fire on
        // the message loop instead of a thread-pool thread.
        DispatcherQueueOptions options{};
        options.dwSize = sizeof(options);
        options.threadType = DQTYPE_THREAD_CURRENT;
        // DQTAT_COM_NONE is required with DQTYPE_THREAD_CURRENT: the apartment has already
        // been initialised by main, and asking the controller to initialise one again is
        // documented as invalid.
        options.apartmentType = DQTAT_COM_NONE;

        winrt::check_hresult(::CreateDispatcherQueueController(
            options, reinterpret_cast<ABI::Windows::System::IDispatcherQueueController**>(
                         winrt::put_abi(m_impl->dispatcherController))));

        // The WinRT view of the same D3D device the renderer uses. Sharing it is what
        // keeps the captured texture directly usable, with no cross-device copy.
        winrt::com_ptr<::IInspectable> inspectable;
        winrt::check_hresult(
            ::CreateDirect3D11DeviceFromDXGIDevice(device.DxgiDevice(), inspectable.put()));
        m_impl->winrtDevice = inspectable.as<wgdx11::IDirect3DDevice>();

        LogInfo("Capture: subsystem initialised.");
        return true;
    } catch (...) {
        m_lastError = DescribeCurrentException();
        LogError("Capture: initialisation failed: {}", m_lastError);
        m_impl.reset();
        return false;
    }
}

void CaptureSession::Shutdown() noexcept {
    Stop();
    if (m_impl) {
        m_impl->winrtDevice = nullptr;
        // The DispatcherQueueController belongs to this thread and is released with it.
        m_impl->dispatcherController = nullptr;
        m_impl.reset();
    }
    m_device = nullptr;
}

bool CaptureSession::Start(HWND target, Callbacks callbacks) {
    if (!m_impl || m_device == nullptr) {
        m_lastError = "Capture subsystem is not initialised.";
        return false;
    }
    if (target == nullptr || ::IsWindow(target) == 0) {
        m_lastError = "The selected window no longer exists.";
        return false;
    }

    Stop();

    if (!IsSupported()) {
        m_lastError = "Windows.Graphics.Capture is not available on this system.";
        LogError("Capture: {}", m_lastError);
        return false;
    }

    m_callbacks = std::move(callbacks);
    m_target = target;
    m_lastError.clear();

    // Named so a failure says which step broke instead of just handing back an HRESULT.
    // Capture bring-up has several distinct ways to fail and they need different fixes.
    const char* stage = "activation factory";

    try {
        // Interop is the only way to turn an HWND into a GraphicsCaptureItem without
        // showing the system picker.
        auto factory = winrt::get_activation_factory<wgc::GraphicsCaptureItem>();
        auto interop = factory.as<::IGraphicsCaptureItemInterop>();

        stage = "GraphicsCaptureItem for window";
        winrt::check_hresult(interop->CreateForWindow(
            target, winrt::guid_of<wgc::GraphicsCaptureItem>(),
            reinterpret_cast<void**>(winrt::put_abi(m_impl->item))));

        stage = "frame pool";
        m_impl->poolSize = m_impl->item.Size();
        if (m_impl->poolSize.Width <= 0 || m_impl->poolSize.Height <= 0) {
            m_impl->poolSize = SizeInt32{1, 1};
        }

        m_impl->framePool = wgc::Direct3D11CaptureFramePool::Create(
            m_impl->winrtDevice, kCaptureFormat, kFramePoolBuffers, m_impl->poolSize);

        stage = "capture session";
        m_impl->session = m_impl->framePool.CreateCaptureSession(m_impl->item);

        // Both of these are newer than the base API, so they are probed rather than
        // assumed, and a refusal is not fatal - it only means a slightly noisier capture.
        if (IsPropertyAvailable(L"Windows.Graphics.Capture.GraphicsCaptureSession",
                                L"IsCursorCaptureEnabled")) {
            try {
                m_impl->session.IsCursorCaptureEnabled(false);
            } catch (...) {
            }
        }
        if (IsPropertyAvailable(L"Windows.Graphics.Capture.GraphicsCaptureSession",
                                L"IsBorderRequired")) {
            try {
                m_impl->session.IsBorderRequired(false);
            } catch (...) {
                // Unpackaged apps may not be allowed to suppress the capture border.
            }
        }

        stage = "event handlers";
        m_impl->frameArrivedToken = m_impl->framePool.FrameArrived(
            [this](const wgc::Direct3D11CaptureFramePool& sender,
                   const winrt::Windows::Foundation::IInspectable&) {
                m_impl->OnFrameArrived(sender);
            });

        m_impl->itemClosedToken = m_impl->item.Closed(
            [this](const wgc::GraphicsCaptureItem&,
                   const winrt::Windows::Foundation::IInspectable&) {
                LogInfo("Capture: capture item closed by the system.");
                auto closed = m_callbacks.onClosed;
                Stop();
                if (closed) {
                    closed();
                }
            });

        stage = "StartCapture";
        m_impl->session.StartCapture();
        m_running = true;
        ResetStats();

        LogInfo("Capture: started on '{}' ({}x{}).",
                Utf8FromWide(GetWindowTitleText(target)), m_impl->poolSize.Width,
                m_impl->poolSize.Height);
        return true;
    } catch (...) {
        m_lastError = std::format("{} ({})", DescribeCurrentException(), stage);
        LogError("Capture: start failed at stage '{}': {}", stage, DescribeCurrentException());
        Stop();
        return false;
    }
}

void CaptureSession::Stop() noexcept {
    if (!m_impl) {
        m_running = false;
        m_target = nullptr;
        return;
    }

    try {
        if (m_impl->item != nullptr && m_impl->itemClosedToken.value != 0) {
            m_impl->item.Closed(m_impl->itemClosedToken);
        }
        if (m_impl->framePool != nullptr && m_impl->frameArrivedToken.value != 0) {
            m_impl->framePool.FrameArrived(m_impl->frameArrivedToken);
        }
        if (m_impl->session != nullptr) {
            m_impl->session.Close();
        }
        if (m_impl->framePool != nullptr) {
            m_impl->framePool.Close();
        }
    } catch (...) {
        // Closing a session whose target already died throws; there is nothing left to do
        // about it and the resources are released either way.
    }

    m_impl->itemClosedToken = {};
    m_impl->frameArrivedToken = {};
    m_impl->session = nullptr;
    m_impl->framePool = nullptr;
    m_impl->item = nullptr;
    m_impl->poolSize = SizeInt32{0, 0};
    m_impl->fpsWindowStartTicks = 0;
    m_impl->fpsWindowFrames = 0;

    InvalidateViewCache();

    if (m_running) {
        LogInfo("Capture: stopped.");
    }
    m_running = false;
    m_paused = false;
    m_target = nullptr;
    m_lastRenderedTicks = 0;
    m_callbacks = {};
}

void CaptureSession::ResetStats() noexcept {
    m_stats = CaptureStats{};
    m_lastRenderedTicks = 0;
    if (m_impl) {
        m_impl->fpsWindowStartTicks = 0;
        m_impl->fpsWindowFrames = 0;
    }
}

ID3D11ShaderResourceView* CaptureSession::ViewForTexture(ID3D11Texture2D* texture) {
    if (texture == nullptr || m_device == nullptr) {
        return nullptr;
    }

    for (SourceView& entry : m_viewCache) {
        if (entry.texture == texture) {
            return entry.view.get();
        }
    }

    com_ptr<ID3D11ShaderResourceView> view;
    if (FAILED(m_device->Device()->CreateShaderResourceView(texture, nullptr, view.put()))) {
        return nullptr;
    }

    for (SourceView& entry : m_viewCache) {
        if (entry.texture == nullptr) {
            entry.texture = texture;
            entry.view = view;
            return entry.view.get();
        }
    }

    // The cache is sized for the frame pool, so a full cache means the pool rotated more
    // textures than expected. Recycle slot zero rather than growing without bound.
    m_viewCache[0].texture = texture;
    m_viewCache[0].view = view;
    return m_viewCache[0].view.get();
}

void CaptureSession::InvalidateViewCache() noexcept {
    m_lastFrameView = nullptr;
    m_lastFrameGeometry = SourceGeometry{};
    for (SourceView& entry : m_viewCache) {
        entry.texture = nullptr;
        entry.view = nullptr;
    }
}

}  // namespace overlaydesk
