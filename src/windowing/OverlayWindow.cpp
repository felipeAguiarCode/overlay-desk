#include "windowing/OverlayWindow.h"

#include <windowsx.h>

#include <algorithm>

#include "core/RenderMath.h"
#include "util/Log.h"
#include "util/Win32Helpers.h"

namespace overlaydesk {
namespace {

constexpr wchar_t kWindowClassName[] = L"OverlayDesk.OverlayWindow";
constexpr wchar_t kWindowTitle[] = L"Overlay Desk";

// Logical sizes, scaled to the window DPI before use.
constexpr int kResizeBorderDip = 8;
constexpr int kEditBorderDip = 3;
constexpr int kMinimumExtentDip = 64;

bool g_classRegistered = false;

// Quick-menu command ids. Local to TrackPopupMenu's TPM_RETURNCMD, which hands the id straight
// back rather than posting WM_COMMAND, so they never collide with anything else.
constexpr UINT kQuickMenuControlPanel = 1;
constexpr UINT kQuickMenuStopOverlay = 2;
constexpr UINT kQuickMenuQuit = 3;

}  // namespace

void OverlayWindow::Create(HINSTANCE instance, OverlaySettings& settings, Callbacks callbacks) {
    if (m_window != nullptr) {
        return;
    }

    m_instance = instance;
    m_settings = &settings;
    m_callbacks = std::move(callbacks);

    if (!g_classRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        // The whole surface is redrawn on every present, so a background brush would only
        // add a GDI paint we immediately overwrite.
        wc.style = 0;
        wc.lpfnWndProc = &OverlayWindow::WindowProcThunk;
        wc.hInstance = instance;
        wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = kWindowClassName;

        if (::RegisterClassExW(&wc) == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            LogError("Overlay: RegisterClassEx failed: {}", FormatWin32Error(::GetLastError()));
            return;
        }
        g_classRegistered = true;
    }

    // WS_THICKFRAME is what gives DefWindowProc its resize machinery. WM_NCCALCSIZE then
    // hands the entire window rectangle to the client area, so the frame is invisible
    // while the resize behaviour survives.
    constexpr DWORD style = WS_POPUP | WS_THICKFRAME;
    const DWORD exStyle = WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;

    m_window = ::CreateWindowExW(exStyle, kWindowClassName, kWindowTitle, style, settings.x,
                                 settings.y, settings.width, settings.height, nullptr, nullptr,
                                 instance, this);
    if (m_window == nullptr) {
        LogError("Overlay: CreateWindowEx failed: {}", FormatWin32Error(::GetLastError()));
        return;
    }

    m_dpi = GetDpiForWindowSafe(m_window);
    ApplySettings();

    LogInfo("Overlay: window created at {},{} {}x{} (dpi {}).", settings.x, settings.y,
            settings.width, settings.height, m_dpi);
}

void OverlayWindow::Destroy() noexcept {
    if (m_window != nullptr) {
        HWND window = m_window;
        m_window = nullptr;
        ::DestroyWindow(window);
        LogInfo("Overlay: window destroyed.");
    }
    m_visible = false;
    m_settings = nullptr;
    m_callbacks = {};
}

void OverlayWindow::Show(bool visible) {
    if (m_window == nullptr || m_visible == visible) {
        return;
    }
    m_visible = visible;
    // SW_SHOWNOACTIVATE: showing the overlay must never pull focus away from the game.
    ::ShowWindow(m_window, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
}

RECT OverlayWindow::Bounds() const noexcept {
    RECT bounds{};
    if (m_window != nullptr) {
        ::GetWindowRect(m_window, &bounds);
    }
    return bounds;
}

float OverlayWindow::EditBorderThickness() const noexcept {
    return static_cast<float>(ScaleForDpi(kEditBorderDip, m_dpi));
}

// ADR-0012. A native menu rather than something drawn into the swap chain: it costs nothing
// per frame, and Windows already handles DPI, keyboard navigation and dismissal on click-away.
QuickMenuCommand OverlayWindow::ShowQuickMenu() {
    if (m_window == nullptr || !m_visible || m_quickMenuOpen) {
        return QuickMenuCommand::None;
    }

    HMENU menu = ::CreatePopupMenu();
    if (menu == nullptr) {
        LogWarn("Quick menu: CreatePopupMenu failed; nothing shown.");
        return QuickMenuCommand::None;
    }

    ::AppendMenuW(menu, MF_STRING, kQuickMenuControlPanel, L"Control panel");
    ::AppendMenuW(menu, MF_STRING, kQuickMenuStopOverlay, L"Stop overlay");
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, kQuickMenuQuit, L"Quit");

    // The menu has to be clickable, so Play Mode's click-through and no-activate bits come off
    // for as long as it is up. UpdateStyles puts back whatever the settings actually say.
    m_quickMenuOpen = true;
    LONG_PTR exStyle = ::GetWindowLongPtrW(m_window, GWL_EXSTYLE);
    ::SetWindowLongPtrW(m_window, GWL_EXSTYLE,
                        exStyle & ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE));

    // Without foreground the menu would not close when the user clicks elsewhere. A process
    // that is handling a hotkey is allowed to take it, which is the case that matters here.
    ::SetForegroundWindow(m_window);

    const RECT bounds = Bounds();
    const int x = bounds.left + RectWidth(bounds) / 2;
    const int y = bounds.top + RectHeight(bounds) / 2;

    const int chosen = ::TrackPopupMenu(
        menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTBUTTON | TPM_CENTERALIGN | TPM_VCENTERALIGN,
        x, y, 0, m_window, nullptr);

    // MSDN's workaround: without a message posted to the owner the menu can stay up after the
    // selection.
    ::PostMessageW(m_window, WM_NULL, 0, 0);
    ::DestroyMenu(menu);

    m_quickMenuOpen = false;
    UpdateStyles();

    switch (chosen) {
        case kQuickMenuControlPanel:
            return QuickMenuCommand::ShowControlPanel;
        case kQuickMenuStopOverlay:
            return QuickMenuCommand::StopOverlay;
        case kQuickMenuQuit:
            return QuickMenuCommand::Quit;
        default:
            return QuickMenuCommand::None;
    }
}

void OverlayWindow::UpdateStyles() {
    if (m_window == nullptr || m_settings == nullptr) {
        return;
    }

    LONG_PTR exStyle = ::GetWindowLongPtrW(m_window, GWL_EXSTYLE);

    // RF-004 / AT-005. WS_EX_TRANSPARENT stops the window being considered for mouse input
    // at all; WM_NCHITTEST returning HTTRANSPARENT covers the same ground for the cases
    // where a hit test still runs. Edit mode always wins over the setting (RF-006).
    const bool clickThrough = m_settings->clickThrough && !m_editMode;
    if (clickThrough) {
        exStyle |= WS_EX_TRANSPARENT;
    } else {
        exStyle &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    }

    // OVERLAY-WINDOW.md: the overlay must not eat the keyboard during Play Mode. Edit mode
    // needs activation so the drag loop behaves like a normal window.
    if (m_editMode) {
        exStyle &= ~static_cast<LONG_PTR>(WS_EX_NOACTIVATE);
    } else {
        exStyle |= WS_EX_NOACTIVATE;
    }

    ::SetWindowLongPtrW(m_window, GWL_EXSTYLE, exStyle);

    // RF-003. Topmost is a z-order band, so it is set through SetWindowPos rather than the
    // style bits we just wrote.
    const HWND insertAfter = m_settings->alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
    ::SetWindowPos(m_window, insertAfter, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void OverlayWindow::ApplyGeometry() {
    if (m_window == nullptr || m_settings == nullptr) {
        return;
    }

    RECT desired{};
    if (m_settings->fullscreen) {
        // OVERLAY-WINDOW.md: the user picks the monitor and the overlay takes its bounds.
        const auto monitors = EnumerateMonitors();
        if (!monitors.empty()) {
            const size_t index =
                std::min(static_cast<size_t>(std::max(m_settings->monitorIndex, 0)),
                         monitors.size() - 1);
            desired = monitors[index].bounds;
        } else {
            desired = GetMonitorForWindow(m_window).bounds;
        }
    } else {
        desired.left = m_settings->x;
        desired.top = m_settings->y;
        desired.right = m_settings->x + m_settings->width;
        desired.bottom = m_settings->y + m_settings->height;
    }

    const int width = std::max(RectWidth(desired), ScaleForDpi(kMinimumExtentDip, m_dpi));
    const int height = std::max(RectHeight(desired), ScaleForDpi(kMinimumExtentDip, m_dpi));

    m_applyingGeometry = true;
    ::SetWindowPos(m_window, nullptr, desired.left, desired.top, width, height,
                   SWP_NOZORDER | SWP_NOACTIVATE);
    m_applyingGeometry = false;
}

void OverlayWindow::ApplySettings() {
    if (m_window == nullptr || m_settings == nullptr) {
        return;
    }
    UpdateStyles();
    ApplyGeometry();
    if (m_callbacks.onRedrawNeeded) {
        m_callbacks.onRedrawNeeded();
    }
}

void OverlayWindow::SetEditMode(bool editing) {
    if (m_window == nullptr || m_settings == nullptr || m_editMode == editing) {
        return;
    }

    // Edit mode suspends click-through rather than overwriting the setting: UpdateStyles
    // derives the effective flag from `clickThrough && !editMode`, so leaving edit mode
    // restores Play Mode exactly without a saved copy to drift out of date.
    m_editMode = editing;
    UpdateStyles();

    if (editing) {
        // Bring it forward once so the user can find what they are about to drag.
        ::SetWindowPos(m_window, m_settings->alwaysOnTop ? HWND_TOPMOST : HWND_TOP, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    if (m_callbacks.onRedrawNeeded) {
        m_callbacks.onRedrawNeeded();
    }
    LogInfo("Overlay: edit mode {}.", editing ? "entered" : "left");
}

void OverlayWindow::MatchBounds(const RECT& bounds) {
    if (m_window == nullptr || m_settings == nullptr) {
        return;
    }
    if (RectWidth(bounds) <= 0 || RectHeight(bounds) <= 0) {
        return;
    }

    m_settings->fullscreen = false;
    m_settings->x = bounds.left;
    m_settings->y = bounds.top;
    m_settings->width = RectWidth(bounds);
    m_settings->height = RectHeight(bounds);

    ApplyGeometry();
    if (m_callbacks.onGeometryChanged) {
        m_callbacks.onGeometryChanged();
    }
    LogInfo("Overlay: matched target bounds {},{} {}x{}.", m_settings->x, m_settings->y,
            m_settings->width, m_settings->height);
}

void OverlayWindow::StoreGeometryFromWindow() {
    if (m_window == nullptr || m_settings == nullptr || m_settings->fullscreen) {
        return;
    }
    RECT bounds{};
    ::GetWindowRect(m_window, &bounds);
    m_settings->x = bounds.left;
    m_settings->y = bounds.top;
    m_settings->width = RectWidth(bounds);
    m_settings->height = RectHeight(bounds);
}

LRESULT OverlayWindow::HandleHitTest(POINT screenPoint) const {
    if (m_settings == nullptr) {
        return HTCLIENT;
    }

    // Click-through wins outright: refuse the hit so the click lands on whatever is
    // underneath (AT-005). Edit mode suspends it, which is the whole point of edit mode.
    if (!m_editMode && m_settings->clickThrough) {
        return HTTRANSPARENT;
    }

    // Everything below applies both in Edit Mode and in Play Mode with click-through off.
    // They get the same grab regions on purpose: once the window accepts the mouse at all,
    // a borderless overlay that cannot be moved or resized by dragging it is simply inert -
    // there is no visible frame to grab instead. Edit Mode differs by drawing the chrome and
    // by suspending click-through, not by being the only way to move the window.

    RECT bounds{};
    ::GetWindowRect(m_window, &bounds);

    const int border = ScaleForDpi(kResizeBorderDip, m_dpi);
    const bool canResize = m_settings->resizable && !m_settings->lockSize && !m_settings->fullscreen;

    if (canResize) {
        const bool onLeft = screenPoint.x < bounds.left + border;
        const bool onRight = screenPoint.x >= bounds.right - border;
        const bool onTop = screenPoint.y < bounds.top + border;
        const bool onBottom = screenPoint.y >= bounds.bottom - border;

        if (onTop && onLeft) return HTTOPLEFT;
        if (onTop && onRight) return HTTOPRIGHT;
        if (onBottom && onLeft) return HTBOTTOMLEFT;
        if (onBottom && onRight) return HTBOTTOMRIGHT;
        if (onLeft) return HTLEFT;
        if (onRight) return HTRIGHT;
        if (onTop) return HTTOP;
        if (onBottom) return HTBOTTOM;
    }

    // HTCAPTION anywhere else makes the whole surface a drag handle, which is what a
    // borderless overlay needs. lockPosition takes that away (RF-005).
    const bool canMove = !m_settings->lockPosition && !m_settings->fullscreen;
    return canMove ? HTCAPTION : HTCLIENT;
}

void OverlayWindow::HandleSizing(WPARAM edge, RECT* rect) const {
    if (m_settings == nullptr || rect == nullptr || !m_settings->lockAspectRatio) {
        return;
    }

    const float ratio = ParseAspectRatio(m_settings->aspectRatio.c_str());
    if (ratio <= 0.0f) {
        return;
    }

    // Dragging a vertical edge means the user chose a width, so the height gives way, and
    // vice versa. Corners keep the width the drag produced.
    const bool draggingVerticalEdge = (edge == WMSZ_LEFT || edge == WMSZ_RIGHT);
    const SizeI adjusted = ApplyAspectRatio(SizeI{RectWidth(*rect), RectHeight(*rect)}, ratio,
                                            /*adjustHeight=*/draggingVerticalEdge ||
                                                edge == WMSZ_TOPLEFT || edge == WMSZ_TOPRIGHT ||
                                                edge == WMSZ_BOTTOMLEFT || edge == WMSZ_BOTTOMRIGHT);

    // Keep the edge the user is actually holding anchored.
    switch (edge) {
        case WMSZ_TOP:
        case WMSZ_TOPLEFT:
        case WMSZ_TOPRIGHT:
            rect->top = rect->bottom - adjusted.height;
            break;
        default:
            rect->bottom = rect->top + adjusted.height;
            break;
    }

    switch (edge) {
        case WMSZ_LEFT:
        case WMSZ_TOPLEFT:
        case WMSZ_BOTTOMLEFT:
            rect->left = rect->right - adjusted.width;
            break;
        default:
            rect->right = rect->left + adjusted.width;
            break;
    }
}

LRESULT CALLBACK OverlayWindow::WindowProcThunk(HWND window, UINT message, WPARAM wParam,
                                                LPARAM lParam) {
    OverlayWindow* self = nullptr;

    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<OverlayWindow*>(create->lpCreateParams);
        self->m_window = window;
        ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<OverlayWindow*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    if (self != nullptr) {
        return self->WindowProc(window, message, wParam, lParam);
    }
    return ::DefWindowProcW(window, message, wParam, lParam);
}

LRESULT OverlayWindow::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_NCCALCSIZE:
            // Client area == window rectangle: the WS_THICKFRAME border becomes invisible
            // while DefWindowProc keeps handling the resize drag.
            if (wParam == TRUE) {
                return 0;
            }
            break;

        case WM_NCHITTEST: {
            const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            return HandleHitTest(point);
        }

        case WM_ERASEBKGND:
            // Everything is painted by the swap chain; letting GDI erase would flash.
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            ::BeginPaint(window, &ps);
            ::EndPaint(window, &ps);
            if (m_callbacks.onRedrawNeeded) {
                m_callbacks.onRedrawNeeded();
            }
            return 0;
        }

        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            const int minimum = ScaleForDpi(kMinimumExtentDip, m_dpi);
            info->ptMinTrackSize.x = minimum;
            info->ptMinTrackSize.y = minimum;
            return 0;
        }

        case WM_WINDOWPOSCHANGING: {
            // RF-005: lockPosition / lockSize veto user-driven moves and resizes but must
            // not block the app applying its own settings.
            if (!m_applyingGeometry && m_settings != nullptr) {
                auto* position = reinterpret_cast<WINDOWPOS*>(lParam);
                if (m_settings->lockPosition) {
                    position->flags |= SWP_NOMOVE;
                }
                if (m_settings->lockSize || !m_settings->resizable) {
                    position->flags |= SWP_NOSIZE;
                }
            }
            break;
        }

        case WM_SIZING:
            HandleSizing(wParam, reinterpret_cast<RECT*>(lParam));
            return TRUE;

        case WM_SIZE: {
            const UINT width = LOWORD(lParam);
            const UINT height = HIWORD(lParam);
            if (width > 0 && height > 0 && m_callbacks.onResized) {
                m_callbacks.onResized(width, height);
            }
            return 0;
        }

        case WM_EXITSIZEMOVE:
            StoreGeometryFromWindow();
            if (m_callbacks.onGeometryChanged) {
                m_callbacks.onGeometryChanged();
            }
            return 0;

        case WM_DPICHANGED: {
            // RNF-007 / AT-017: honour the rectangle Windows suggests, otherwise the
            // window ends up the wrong physical size after crossing to a monitor with a
            // different scale factor.
            m_dpi = HIWORD(wParam);
            const auto* suggested = reinterpret_cast<const RECT*>(lParam);
            if (suggested != nullptr) {
                m_applyingGeometry = true;
                ::SetWindowPos(window, nullptr, suggested->left, suggested->top,
                               RectWidth(*suggested), RectHeight(*suggested),
                               SWP_NOZORDER | SWP_NOACTIVATE);
                m_applyingGeometry = false;
                StoreGeometryFromWindow();
            }
            LogInfo("Overlay: dpi changed to {}.", m_dpi);
            return 0;
        }

        case WM_DISPLAYCHANGE:
            // A monitor came or went; fullscreen bounds may no longer be valid.
            if (m_settings != nullptr && m_settings->fullscreen) {
                ApplyGeometry();
            }
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE && m_callbacks.onEscape) {
                m_callbacks.onEscape();
                return 0;
            }
            break;

        case WM_CLOSE:
            // The overlay is closed by the control panel or the Escape gesture, never by
            // stray input.
            return 0;

        case WM_DESTROY:
            m_window = nullptr;
            return 0;

        default:
            break;
    }

    return ::DefWindowProcW(window, message, wParam, lParam);
}

}  // namespace overlaydesk
