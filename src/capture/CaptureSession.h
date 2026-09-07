#pragma once

// Windows.Graphics.Capture, wired for the threading model in ADR-0006.
//
// The frame pool is created with Direct3D11CaptureFramePool::Create rather than
// CreateFreeThreaded, so FrameArrived is dispatched onto the thread that owns the
// DispatcherQueue - our Win32 message loop. That means the render happens inside the
// callback, on the same thread that owns the immediate context: no cross-thread handoff,
// no lock around the device context, and above all no copy of the captured texture.
//
// PERFORMANCE.md forbids readback, staging textures and per-frame allocations. The only
// per-frame work here is a small array lookup for a cached shader resource view.

#include <Windows.h>
#include <d3d11_4.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "core/AppState.h"
#include "graphics/D3D11Device.h"
#include "graphics/RenderResources.h"
#include "util/ComHelpers.h"

namespace overlaydesk {

class CaptureSession {
public:
    struct Callbacks {
        // Called on the message-loop thread with a shader resource view over the live
        // frame-pool texture. The view is only valid for the duration of the call.
        std::function<void(ID3D11ShaderResourceView* source, const SourceGeometry& geometry)>
            onFrame;
        // AT-014: the capture item was closed, usually because the target window died.
        std::function<void()> onClosed;
    };

    // Both are defined in the .cpp: Impl is incomplete here, so the unique_ptr member
    // cannot have its constructor or destructor instantiated in another translation unit.
    CaptureSession();
    ~CaptureSession();
    CaptureSession(const CaptureSession&) = delete;
    CaptureSession& operator=(const CaptureSession&) = delete;

    // False on Windows builds without Windows.Graphics.Capture, or when the feature is
    // disabled by policy. Checked before offering the user a target (RNF-009).
    static bool IsSupported() noexcept;

    // Creates the DispatcherQueue for this thread and the WinRT view of the D3D device.
    // Must run on the thread that owns the message loop, before any Start.
    bool Initialize(const D3D11Device& device);
    void Shutdown() noexcept;

    bool Start(HWND target, Callbacks callbacks);
    void Stop() noexcept;

    bool IsRunning() const noexcept { return m_running; }
    HWND Target() const noexcept { return m_target; }

    // RNF-004: while paused, arriving frames are closed immediately and never rendered.
    void SetPaused(bool paused) noexcept { m_paused = paused; }
    bool IsPaused() const noexcept { return m_paused; }

    // RNF-003: minimum seconds between rendered frames. 0 renders every frame that lands.
    void SetFrameInterval(double seconds) noexcept { m_frameIntervalSeconds = seconds; }

    // The shader resource view for the most recently delivered frame, or null before the
    // first one. The frame-pool texture behind it is not overwritten until the source
    // produces a new frame, so re-rendering from it while the source is static shows the
    // correct picture - and costs nothing, since there is no copy and no extra texture.
    // Used to repaint edit-mode chrome over a frozen source (AT-006).
    ID3D11ShaderResourceView* LastFrameView() const noexcept { return m_lastFrameView.get(); }
    const SourceGeometry& LastFrameGeometry() const noexcept { return m_lastFrameGeometry; }

    const CaptureStats& Stats() const noexcept { return m_stats; }
    void ResetStats() noexcept;

    const std::string& LastError() const noexcept { return m_lastError; }

private:
    // Frame-pool textures rotate through a handful of allocations, so a fixed-size cache
    // removes shader resource view creation from the frame path without ever growing.
    // Each entry owns its view, and the view owns the texture, which is why comparing the
    // raw texture pointer is safe.
    struct SourceView {
        ID3D11Texture2D* texture = nullptr;
        com_ptr<ID3D11ShaderResourceView> view;
    };
    static constexpr size_t kSourceViewCacheSize = 4;

    ID3D11ShaderResourceView* ViewForTexture(ID3D11Texture2D* texture);
    void InvalidateViewCache() noexcept;

    // Defined in the .cpp so this header stays free of C++/WinRT projection headers.
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    const D3D11Device* m_device = nullptr;
    HWND m_target = nullptr;
    Callbacks m_callbacks;

    std::array<SourceView, kSourceViewCacheSize> m_viewCache;
    com_ptr<ID3D11ShaderResourceView> m_lastFrameView;
    SourceGeometry m_lastFrameGeometry;

    bool m_running = false;
    bool m_paused = false;
    double m_frameIntervalSeconds = 1.0 / 60.0;
    int64_t m_lastRenderedTicks = 0;

    CaptureStats m_stats;
    std::string m_lastError;
};

}  // namespace overlaydesk
