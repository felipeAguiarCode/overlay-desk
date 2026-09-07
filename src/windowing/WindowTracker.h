#pragma once

// ARCHITECTURE.md: watches the target for destruction, resize, move, minimize and monitor
// changes.
//
// Deliberately a poll rather than a WinEvent hook: a hook would put us in another
// process's event stream for information we only need a few times a second, and Poll()
// runs on a ~250 ms timer, well outside the frame path. Auto-follow is explicitly a
// post-MVP feature, so nothing here moves the overlay on its own - it only reports.

#include <Windows.h>

#include <functional>

namespace overlaydesk {

class WindowTracker {
public:
    struct Events {
        std::function<void()> onTargetClosed;
        std::function<void(bool minimized)> onMinimizedChanged;
        std::function<void(const RECT& bounds)> onBoundsChanged;
        std::function<void(int monitorIndex)> onMonitorChanged;
    };

    void SetTarget(HWND window, Events events);
    void Clear() noexcept;

    HWND Target() const noexcept { return m_target; }
    bool HasTarget() const noexcept { return m_target != nullptr && ::IsWindow(m_target) != 0; }
    bool IsMinimized() const noexcept { return m_minimized; }
    RECT Bounds() const noexcept { return m_bounds; }

    // Cheap enough to call from a timer; does nothing without a target.
    void Poll();

private:
    HWND m_target = nullptr;
    Events m_events;
    RECT m_bounds{};
    bool m_minimized = false;
    int m_monitorIndex = 0;
};

}  // namespace overlaydesk
