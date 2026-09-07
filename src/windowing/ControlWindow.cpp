#include "windowing/ControlWindow.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <algorithm>

#include "util/Log.h"
#include "util/Win32Helpers.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message,
                                                             WPARAM wParam, LPARAM lParam);

namespace overlaydesk {
namespace {

constexpr wchar_t kWindowClassName[] = L"OverlayDesk.ControlWindow";
constexpr wchar_t kWindowTitle[] = L"Overlay Desk";

constexpr int kMinimumWidthDip = 380;
constexpr int kMinimumHeightDip = 420;

bool g_classRegistered = false;

void ApplyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();

    // UI-SPEC.md asks for a compact technical panel with an obvious ON/OFF state per
    // module, so the card padding and rounding are set once here rather than per widget.
    style.WindowRounding = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FrameRounding = 4.0f;
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.GrabRounding = 4.0f;
    style.ItemSpacing = ImVec2(8.0f, 7.0f);
    style.ChildRounding = 6.0f;
    style.ChildBorderSize = 1.0f;
    style.ScrollbarSize = 12.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.13f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.21f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.24f, 0.28f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.15f, 0.65f, 0.95f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.25f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.20f, 0.80f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.30f, 0.38f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.55f, 0.85f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.15f, 0.45f, 0.70f, 1.00f);
}

}  // namespace

void ControlWindow::Create(HINSTANCE instance, const D3D11Device& device, UiSettings& uiSettings,
                           Callbacks callbacks) {
    if (m_window != nullptr) {
        return;
    }

    m_device = &device;
    m_uiSettings = &uiSettings;
    m_callbacks = std::move(callbacks);

    if (!g_classRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &ControlWindow::WindowProcThunk;
        wc.hInstance = instance;
        wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);

        // Resource id 1 is the icon the .rc embeds. LoadImage rather than LoadIcon, twice, so
        // the title bar gets a 16 px rendering and Alt+Tab a 32 px one instead of both being
        // scaled from whichever size LoadIcon happens to pick. Falling back to the system icon
        // keeps a missing resource from leaving the window with no icon at all.
        const auto loadIcon = [instance](int widthMetric, int heightMetric) {
            return static_cast<HICON>(::LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                                   ::GetSystemMetrics(widthMetric),
                                                   ::GetSystemMetrics(heightMetric),
                                                   LR_DEFAULTCOLOR));
        };
        wc.hIcon = loadIcon(SM_CXICON, SM_CYICON);
        wc.hIconSm = loadIcon(SM_CXSMICON, SM_CYSMICON);
        if (wc.hIcon == nullptr) {
            wc.hIcon = ::LoadIconW(nullptr, IDI_APPLICATION);
        }
        wc.lpszClassName = kWindowClassName;

        if (::RegisterClassExW(&wc) == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            LogError("Control: RegisterClassEx failed: {}", FormatWin32Error(::GetLastError()));
            return;
        }
        g_classRegistered = true;
    }

    const int width = std::max(uiSettings.controlWindowWidth, kMinimumWidthDip);
    const int height = std::max(uiSettings.controlWindowHeight, kMinimumHeightDip);

    m_window = ::CreateWindowExW(0, kWindowClassName, kWindowTitle, WS_OVERLAPPEDWINDOW,
                                 CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr,
                                 instance, this);
    if (m_window == nullptr) {
        LogError("Control: CreateWindowEx failed: {}", FormatWin32Error(::GetLastError()));
        return;
    }

    m_dpi = GetDpiForWindowSafe(m_window);

    RECT client{};
    ::GetClientRect(m_window, &client);
    m_swapChain.CreateForHwnd(device, m_window, static_cast<UINT>(RectWidth(client)),
                              static_cast<UINT>(RectHeight(client)));

    InitializeImGui();

    ::ShowWindow(m_window, SW_SHOW);
    ::UpdateWindow(m_window);

    LogInfo("Control: panel window created ({}x{}, dpi {}).", width, height, m_dpi);
}

void ControlWindow::BringToFront() noexcept {
    if (m_window == nullptr) {
        return;
    }

    if (::IsIconic(m_window) != 0) {
        ::ShowWindow(m_window, SW_RESTORE);
    } else {
        ::ShowWindow(m_window, SW_SHOW);
    }

    // SetForegroundWindow is allowed here because the call is a direct consequence of the user
    // pressing a hotkey this process registered, which is one of the cases Windows accepts.
    ::SetForegroundWindow(m_window);
}

void ControlWindow::Destroy() noexcept {
    ShutdownImGui();
    m_swapChain.Reset();

    if (m_window != nullptr) {
        HWND window = m_window;
        m_window = nullptr;
        ::DestroyWindow(window);
    }

    m_device = nullptr;
    m_uiSettings = nullptr;
    m_callbacks = {};
}

void ControlWindow::InitializeImGui() {
    if (m_imguiInitialised || m_device == nullptr) {
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // No imgui.ini: the panel's own layout is fixed and the parts worth remembering live
    // in settings.json (CONFIGURATION.md), not in a second config file.
    io.IniFilename = nullptr;

    ApplyTheme();
    m_baseStyle = ImGui::GetStyle();
    ApplyDpiScaling(m_dpi);

    ImGui_ImplWin32_Init(m_window);
    ImGui_ImplDX11_Init(m_device->Device(), m_device->Context());

    m_imguiInitialised = true;
}

void ControlWindow::ShutdownImGui() noexcept {
    if (!m_imguiInitialised) {
        return;
    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    m_imguiInitialised = false;
}

void ControlWindow::ApplyDpiScaling(UINT dpi) {
    // ScaleAllSizes multiplies in place, so it is applied to a pristine copy of the theme
    // rather than to the already-scaled live style; otherwise a second DPI change would
    // compound on the first.
    const float scale = static_cast<float>(dpi) / static_cast<float>(kDefaultDpi);
    ImGuiStyle scaled = m_baseStyle;
    scaled.ScaleAllSizes(scale);
    // ImGui 1.92 moved the font scale out of ImGuiIO and into the style.
    scaled.FontScaleMain = scale;
    ImGui::GetStyle() = scaled;
}

void ControlWindow::Render(const std::function<void()>& drawUi) {
    if (!m_imguiInitialised || m_window == nullptr || !m_swapChain.IsValid()) {
        return;
    }

    if (m_pendingResize) {
        RECT client{};
        ::GetClientRect(m_window, &client);
        if (RectWidth(client) > 0 && RectHeight(client) > 0) {
            m_swapChain.Resize(static_cast<UINT>(RectWidth(client)),
                               static_cast<UINT>(RectHeight(client)));
        }
        m_pendingResize = false;
    }

    ID3D11RenderTargetView* renderTarget = m_swapChain.RenderTargetView();
    if (renderTarget == nullptr) {
        return;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (drawUi) {
        drawUi();
    }

    ImGui::Render();

    constexpr float kBackground[4] = {0.09f, 0.09f, 0.10f, 1.0f};
    ID3D11RenderTargetView* renderTargets[] = {renderTarget};
    m_device->Context()->OMSetRenderTargets(1, renderTargets, nullptr);
    m_device->Context()->ClearRenderTargetView(renderTarget, kBackground);

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Vsync here, unlike the overlay: the panel has no latency requirement and pacing it
    // to the display keeps an idle control window off the GPU budget.
    m_swapChain.Present(1);
}

LRESULT CALLBACK ControlWindow::WindowProcThunk(HWND window, UINT message, WPARAM wParam,
                                                LPARAM lParam) {
    ControlWindow* self = nullptr;

    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<ControlWindow*>(create->lpCreateParams);
        self->m_window = window;
        ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<ControlWindow*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    if (self != nullptr) {
        return self->WindowProc(window, message, wParam, lParam);
    }
    return ::DefWindowProcW(window, message, wParam, lParam);
}

LRESULT ControlWindow::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (m_imguiInitialised &&
        ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam) != 0) {
        return 1;
    }

    switch (message) {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                m_pendingResize = true;
                if (m_uiSettings != nullptr) {
                    RECT bounds{};
                    ::GetWindowRect(window, &bounds);
                    m_uiSettings->controlWindowWidth = RectWidth(bounds);
                    m_uiSettings->controlWindowHeight = RectHeight(bounds);
                }
            }
            return 0;

        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = ScaleForDpi(kMinimumWidthDip, m_dpi);
            info->ptMinTrackSize.y = ScaleForDpi(kMinimumHeightDip, m_dpi);
            return 0;
        }

        case WM_DPICHANGED: {
            m_dpi = HIWORD(wParam);
            if (m_imguiInitialised) {
                ApplyDpiScaling(m_dpi);
            }

            const auto* suggested = reinterpret_cast<const RECT*>(lParam);
            if (suggested != nullptr) {
                ::SetWindowPos(window, nullptr, suggested->left, suggested->top,
                               RectWidth(*suggested), RectHeight(*suggested),
                               SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;
        }

        case WM_HOTKEY:
            if (m_callbacks.onHotkey) {
                m_callbacks.onHotkey(static_cast<int>(wParam));
            }
            return 0;

        case WM_KEYDOWN:
            // Escape is also how ImGui cancels an active text field, so it is only treated
            // as the close gesture when nothing is being typed into.
            if (wParam == VK_ESCAPE && m_callbacks.onEscape && m_imguiInitialised &&
                !ImGui::GetIO().WantTextInput) {
                m_callbacks.onEscape();
                return 0;
            }
            break;

        case WM_SYSCOMMAND:
            // Swallow the Alt / F10 menu activation, which ImGui has no use for.
            if ((wParam & 0xFFF0) == SC_KEYMENU) {
                return 0;
            }
            break;

        case WM_CLOSE:
            if (m_callbacks.onCloseRequested) {
                m_callbacks.onCloseRequested();
            }
            return 0;

        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;

        default:
            break;
    }

    return ::DefWindowProcW(window, message, wParam, lParam);
}

}  // namespace overlaydesk
