#pragma once

// ADR-0002: the overlay is an independent top-level HWND. It is never a child of the
// target, never injected into it, and keeps working when the target moves, resizes or
// dies.
//
// ADR-0006 fixes how it is composed: WS_POPUP | WS_THICKFRAME with WM_NCCALCSIZE swallowing
// the frame, WS_EX_NOREDIRECTIONBITMAP so DWM allocates no redirection surface, and
// DirectComposition doing the actual composition. Global opacity is applied by the pixel
// shader, not by SetLayeredWindowAttributes.

#include <Windows.h>

#include <functional>

#include "core/Settings.h"

namespace overlaydesk {

// What the user picked from the overlay's quick menu. `None` covers both dismissal and a
// menu that could not be shown at all.
enum class QuickMenuCommand {
    None = 0,
    ShowControlPanel,
    StopOverlay,
    Quit,
};

class OverlayWindow {
public:
    struct Callbacks {
        // Client area changed. The renderer resizes its swap chain from here.
        std::function<void(uint32_t width, uint32_t height)> onResized;
        // Position or size settled after a user drag; the app persists the new geometry.
        std::function<void()> onGeometryChanged;
        // Something needs repainting while no capture frame is due - edit-mode chrome
        // animation, a DPI change, a settings tweak.
        std::function<void()> onRedrawNeeded;
        // Escape pressed while the overlay has focus, which in practice means Edit Mode -
        // Play Mode carries WS_EX_NOACTIVATE and never sees a keystroke, so the quick menu
        // is also reachable through a global shortcut.
        std::function<void()> onEscape;
    };

    OverlayWindow() = default;
    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;
    ~OverlayWindow() { Destroy(); }

    // `settings` must outlive the window: the overlay reads it on every state change and
    // writes back the geometry the user drags out.
    void Create(HINSTANCE instance, OverlaySettings& settings, Callbacks callbacks);
    void Destroy() noexcept;

    HWND Handle() const noexcept { return m_window; }
    bool IsCreated() const noexcept { return m_window != nullptr; }

    void Show(bool visible);
    bool IsVisible() const noexcept { return m_visible; }

    // Re-applies every flag in the settings struct: topmost, click-through, locks,
    // fullscreen, geometry. Safe to call as often as the UI changes anything.
    void ApplySettings();

    // RF-006 / AT-006. Edit mode suspends click-through, shows the border and the resize
    // handles, and restores the previous flags on exit.
    void SetEditMode(bool editing);
    bool IsEditMode() const noexcept { return m_editMode; }

    // RF-007: copy the target's visible bounds onto the overlay.
    void MatchBounds(const RECT& bounds);

    RECT Bounds() const noexcept;
    UINT Dpi() const noexcept { return m_dpi; }

    // Border thickness in physical pixels for the current DPI, handed to the shader.
    float EditBorderThickness() const noexcept;

    // ADR-0012. Shows the native quick menu centred on the overlay and blocks until the user
    // chooses or dismisses it. Play Mode's click-through and no-activate bits are suspended
    // for the duration and restored before returning, so a menu the user can actually click
    // never leaves the overlay stealing input afterwards.
    QuickMenuCommand ShowQuickMenu();
    bool IsQuickMenuOpen() const noexcept { return m_quickMenuOpen; }

private:
    static LRESULT CALLBACK WindowProcThunk(HWND window, UINT message, WPARAM wParam,
                                            LPARAM lParam);
    LRESULT WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT HandleHitTest(POINT screenPoint) const;
    void HandleSizing(WPARAM edge, RECT* rect) const;
    void UpdateStyles();
    void ApplyGeometry();
    void StoreGeometryFromWindow();

    // Guards against a second menu being opened from inside the first one's modal loop -
    // Escape and the shortcut can both arrive while it is up.
    bool m_quickMenuOpen = false;

    HWND m_window = nullptr;
    HINSTANCE m_instance = nullptr;
    OverlaySettings* m_settings = nullptr;
    Callbacks m_callbacks;

    UINT m_dpi = 96;
    bool m_editMode = false;
    bool m_visible = false;

    // Set while we are the ones moving the window, so the lockPosition / lockSize guards
    // in WM_WINDOWPOSCHANGING do not veto our own programmatic updates.
    bool m_applyingGeometry = false;
};

}  // namespace overlaydesk
