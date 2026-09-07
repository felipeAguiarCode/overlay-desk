// Overlay Desk - entry point.
//
// The apartment is initialised single-threaded on purpose: ADR-0006 puts the capture
// callbacks on this thread's DispatcherQueue, which requires an STA and a running message
// loop. Everything downstream - the D3D device, the frame pool, the renderer - assumes the
// same thread.

#include <Windows.h>
#include <winrt/base.h>

#include "Application.h"
#include "util/ComHelpers.h"
#include "util/Log.h"

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
    // Belt and braces alongside the manifest: if the manifest is ever stripped, the
    // process still comes up per-monitor DPI aware rather than silently bitmap-scaled.
    ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    winrt::init_apartment(winrt::apartment_type::single_threaded);

    int exitCode = 0;
    try {
        overlaydesk::Application application;
        if (!application.Initialize(instance)) {
            application.Shutdown();
            winrt::uninit_apartment();
            return 1;
        }
        exitCode = application.Run();
        application.Shutdown();
    } catch (...) {
        // RNF-009: report and exit cleanly rather than letting the runtime terminate us.
        const std::string description = overlaydesk::DescribeCurrentException();
        overlaydesk::LogError("Fatal: {}", description);
        overlaydesk::LogShutdown();
        ::MessageBoxA(nullptr, description.c_str(), "Overlay Desk - unexpected error",
                      MB_ICONERROR | MB_OK);
        exitCode = 1;
    }

    winrt::uninit_apartment();
    return exitCode;
}
