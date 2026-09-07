#include "windowing/TargetWindow.h"

#include <algorithm>

#include "util/Log.h"
#include "util/Win32Helpers.h"

namespace overlaydesk {
namespace {

BOOL CALLBACK EnumProc(HWND window, LPARAM userData) {
    if (IsCaptureCandidate(window)) {
        auto* results = reinterpret_cast<std::vector<TargetWindowInfo>*>(userData);
        results->push_back(DescribeWindow(window));
    }
    return TRUE;
}

int MonitorIndexForWindow(HWND window) {
    const HMONITOR monitor = ::MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    const auto monitors = EnumerateMonitors();
    for (size_t i = 0; i < monitors.size(); ++i) {
        if (monitors[i].handle == monitor) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

}  // namespace

bool IsCaptureCandidate(HWND window) {
    if (window == nullptr || ::IsWindow(window) == 0) {
        return false;
    }
    if (::IsWindowVisible(window) == 0) {
        return false;
    }
    // Only top-level windows can be handed to GraphicsCaptureItem.
    if (::GetAncestor(window, GA_ROOT) != window) {
        return false;
    }
    if (::GetWindowTextLengthW(window) == 0) {
        return false;
    }

    const LONG_PTR exStyle = ::GetWindowLongPtrW(window, GWL_EXSTYLE);
    if ((exStyle & WS_EX_TOOLWINDOW) != 0) {
        return false;
    }

    // Suspended UWP apps stay "visible" while the shell keeps them cloaked. They enumerate
    // as perfectly good targets and then capture as a frozen surface, so filter them out
    // before the user can pick one.
    if (IsWindowCloaked(window)) {
        return false;
    }

    // Never offer our own windows as a capture source: capturing the overlay that is
    // displaying the capture is an infinite mirror.
    DWORD processId = 0;
    ::GetWindowThreadProcessId(window, &processId);
    if (processId == ::GetCurrentProcessId()) {
        return false;
    }

    return true;
}

TargetWindowInfo DescribeWindow(HWND window) {
    TargetWindowInfo info;
    if (window == nullptr || ::IsWindow(window) == 0) {
        return info;
    }

    info.window = window;
    ::GetWindowThreadProcessId(window, &info.processId);
    info.title = GetWindowTitleText(window);
    info.executable = GetWindowProcessImageName(window);
    info.bounds = GetVisibleWindowBounds(window);
    info.visible = ::IsWindowVisible(window) != 0;
    info.minimized = ::IsIconic(window) != 0;
    info.monitorIndex = MonitorIndexForWindow(window);
    return info;
}

std::vector<TargetWindowInfo> EnumerateCaptureTargets() {
    std::vector<TargetWindowInfo> results;
    results.reserve(32);
    ::EnumWindows(&EnumProc, reinterpret_cast<LPARAM>(&results));

    std::sort(results.begin(), results.end(),
              [](const TargetWindowInfo& a, const TargetWindowInfo& b) {
                  if (a.executable != b.executable) {
                      return a.executable < b.executable;
                  }
                  return a.title < b.title;
              });
    return results;
}

HWND FindWindowByDescription(const std::wstring& title, const std::wstring& executable) {
    if (title.empty() && executable.empty()) {
        return nullptr;
    }

    HWND fallback = nullptr;
    for (const TargetWindowInfo& candidate : EnumerateCaptureTargets()) {
        if (!executable.empty() && EqualsIgnoreCase(candidate.executable, executable)) {
            if (candidate.title == title) {
                return candidate.window;
            }
            // Same program, different document or ROM loaded. Good enough if nothing
            // matches exactly.
            if (fallback == nullptr) {
                fallback = candidate.window;
            }
        } else if (executable.empty() && candidate.title == title) {
            return candidate.window;
        }
    }
    return fallback;
}

}  // namespace overlaydesk
