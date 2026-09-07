#pragma once

// ARCHITECTURE.md: the Dear ImGui control panel. It shares the application's single D3D11
// device with the overlay renderer, and the renderer is never allowed to depend on it -
// closing the panel does not stop frames from being composed onto the overlay, it ends the
// process because the panel is the only entry point the MVP ships (a tray icon is P1).

#include <Windows.h>

#include <imgui.h>

#include <functional>

#include "core/Settings.h"
#include "graphics/D3D11Device.h"
#include "graphics/SwapChain.h"

namespace overlaydesk {

class ControlWindow {
public:
    struct Callbacks {
        std::function<void()> onCloseRequested;
        // Fired for WM_HOTKEY so the application can act on the Edit Mode shortcut.
        std::function<void(int hotkeyId)> onHotkey;
        // Escape pressed while the panel has focus and no text field is active.
        std::function<void()> onEscape;
    };

    ControlWindow() = default;
    ControlWindow(const ControlWindow&) = delete;
    ControlWindow& operator=(const ControlWindow&) = delete;
    ~ControlWindow() { Destroy(); }

    void Create(HINSTANCE instance, const D3D11Device& device, UiSettings& uiSettings,
                Callbacks callbacks);
    void Destroy() noexcept;

    HWND Handle() const noexcept { return m_window; }
    bool IsCreated() const noexcept { return m_window != nullptr; }

    // Restores the panel from minimised and brings it to the front. Used by the "show control
    // panel" shortcut, which exists precisely for the case where the window is buried under a
    // fullscreen game.
    void BringToFront() noexcept;

    // Runs one ImGui frame and presents it. `drawUi` supplies the panel contents.
    void Render(const std::function<void()>& drawUi);

private:
    static LRESULT CALLBACK WindowProcThunk(HWND window, UINT message, WPARAM wParam,
                                            LPARAM lParam);
    LRESULT WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    void InitializeImGui();
    void ShutdownImGui() noexcept;
    void ApplyDpiScaling(UINT dpi);

    HWND m_window = nullptr;
    const D3D11Device* m_device = nullptr;
    UiSettings* m_uiSettings = nullptr;
    Callbacks m_callbacks;

    SwapChain m_swapChain;
    // Unscaled theme, kept so DPI changes re-derive from it instead of compounding.
    ImGuiStyle m_baseStyle;
    bool m_imguiInitialised = false;
    UINT m_dpi = 96;
    bool m_pendingResize = false;
};

}  // namespace overlaydesk
