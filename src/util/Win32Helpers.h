#pragma once

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace overlaydesk {

// --- Text -----------------------------------------------------------------------------

std::string Utf8FromWide(std::wstring_view text);
std::wstring WideFromUtf8(std::string_view text);

// Formats a Win32 error code as "0x80070005: Access is denied".
std::string FormatWin32Error(DWORD error);

// Ordinal case-insensitive comparison. Windows file names are case-insensitive, so an
// executable recorded as "notepad.exe" has to match a process reporting "Notepad.exe".
bool EqualsIgnoreCase(std::wstring_view a, std::wstring_view b) noexcept;

// --- Paths ----------------------------------------------------------------------------

// %APPDATA%\OverlayDesk (CONFIGURATION.md). Returns an empty path if the known folder
// cannot be resolved; callers must handle that instead of assuming success.
std::filesystem::path AppDataDirectory();

// --- DPI ------------------------------------------------------------------------------

inline constexpr UINT kDefaultDpi = 96;

UINT GetDpiForWindowSafe(HWND window) noexcept;

constexpr int ScaleForDpi(int value, UINT dpi) noexcept {
    return static_cast<int>((static_cast<int64_t>(value) * dpi + kDefaultDpi / 2) / kDefaultDpi);
}

// --- Monitors -------------------------------------------------------------------------

struct MonitorInfo {
    HMONITOR handle = nullptr;
    RECT bounds{};       // full monitor rectangle, virtual-screen coordinates
    RECT workArea{};     // bounds minus taskbar
    UINT dpi = kDefaultDpi;
    bool primary = false;
    std::wstring deviceName;
};

std::vector<MonitorInfo> EnumerateMonitors();

// Monitor currently hosting the window, or the primary monitor when the window handle is
// not valid yet.
MonitorInfo GetMonitorForWindow(HWND window);

// --- Window geometry ------------------------------------------------------------------

// GetWindowRect includes the invisible resize border that DWM draws around most windows,
// which would make "Match Target Window" leave a few pixels of slop on every edge.
// DWMWA_EXTENDED_FRAME_BOUNDS reports the visible frame instead.
RECT GetVisibleWindowBounds(HWND window) noexcept;

// True for windows the shell keeps alive but does not show - suspended UWP apps in
// particular. They enumerate as visible yet capture as a frozen or empty surface.
bool IsWindowCloaked(HWND window) noexcept;

std::wstring GetWindowTitleText(HWND window);

// Executable file name (no directory) owning the window, e.g. "retroarch.exe".
std::wstring GetWindowProcessImageName(HWND window);

constexpr int RectWidth(const RECT& r) noexcept { return r.right - r.left; }
constexpr int RectHeight(const RECT& r) noexcept { return r.bottom - r.top; }

}  // namespace overlaydesk
