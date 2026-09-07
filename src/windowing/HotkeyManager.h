#pragma once

// Owns the RegisterHotKey side of the global shortcuts. The bindings themselves are settings
// (core/Hotkeys.h); this turns them into registrations on one window and reports which ones
// Windows accepted.
//
// A refused registration is normal, not an error: another application registered the same
// chord first, and the only correct response is to tell the user which shortcut is unavailable
// and carry on. Everything the shortcuts do is also reachable from the panel.

#include <Windows.h>

#include "core/Hotkeys.h"

namespace overlaydesk {

class HotkeyManager {
public:
    HotkeyManager() = default;
    HotkeyManager(const HotkeyManager&) = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;
    ~HotkeyManager() { Clear(); }

    // Registers every bound action on `window`, replacing whatever was registered before.
    // Safe to call repeatedly - the UI calls it after every rebind.
    void Apply(HWND window, const HotkeySettings& settings);
    void Clear() noexcept;

    // The action a WM_HOTKEY id belongs to, or Count when the id is not one of ours.
    HotkeyAction ActionForId(int id) const noexcept;

    bool Registered(HotkeyAction action) const noexcept;
    // Bound in settings, but Windows refused it - almost always another application holding
    // the same chord.
    bool Unavailable(HotkeyAction action) const noexcept;

private:
    static int IdFor(HotkeyAction action) noexcept;

    HWND m_window = nullptr;
    bool m_registered[kHotkeyActionCount]{};
    bool m_unavailable[kHotkeyActionCount]{};
};

}  // namespace overlaydesk
