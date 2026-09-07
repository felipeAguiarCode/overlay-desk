#include "windowing/WindowTracker.h"

#include "util/Log.h"
#include "util/Win32Helpers.h"
#include "windowing/TargetWindow.h"

namespace overlaydesk {
namespace {

bool SameRect(const RECT& a, const RECT& b) noexcept {
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

int MonitorIndexOf(HWND window) {
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

void WindowTracker::SetTarget(HWND window, Events events) {
    m_target = window;
    m_events = std::move(events);

    if (HasTarget()) {
        m_bounds = GetVisibleWindowBounds(window);
        m_minimized = ::IsIconic(window) != 0;
        m_monitorIndex = MonitorIndexOf(window);
    } else {
        m_bounds = RECT{};
        m_minimized = false;
        m_monitorIndex = 0;
    }
}

void WindowTracker::Clear() noexcept {
    m_target = nullptr;
    m_events = {};
    m_bounds = RECT{};
    m_minimized = false;
    m_monitorIndex = 0;
}

void WindowTracker::Poll() {
    if (m_target == nullptr) {
        return;
    }

    // AT-014: the target going away must not take the application with it.
    if (::IsWindow(m_target) == 0) {
        LogInfo("Tracker: target window closed.");
        const auto closed = m_events.onTargetClosed;
        Clear();
        if (closed) {
            closed();
        }
        return;
    }

    // RNF-004 / AT-015: a minimized source produces no frames, so the app stops asking for
    // them rather than spinning.
    const bool minimized = ::IsIconic(m_target) != 0;
    if (minimized != m_minimized) {
        m_minimized = minimized;
        LogInfo("Tracker: target {}.", minimized ? "minimized" : "restored");
        if (m_events.onMinimizedChanged) {
            m_events.onMinimizedChanged(minimized);
        }
    }

    const RECT bounds = GetVisibleWindowBounds(m_target);
    if (!SameRect(bounds, m_bounds)) {
        m_bounds = bounds;
        if (m_events.onBoundsChanged) {
            m_events.onBoundsChanged(bounds);
        }
    }

    const int monitorIndex = MonitorIndexOf(m_target);
    if (monitorIndex != m_monitorIndex) {
        m_monitorIndex = monitorIndex;
        if (m_events.onMonitorChanged) {
            m_events.onMonitorChanged(monitorIndex);
        }
    }
}

}  // namespace overlaydesk
