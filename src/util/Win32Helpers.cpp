#include "util/Win32Helpers.h"

#include <ShellScalingApi.h>
#include <ShlObj.h>
#include <dwmapi.h>

#include <algorithm>
#include <format>
#include <memory>
#include <type_traits>

namespace overlaydesk {
namespace {

struct LocalFreeDeleter {
    void operator()(void* p) const noexcept { ::LocalFree(p); }
};

struct HandleCloser {
    void operator()(HANDLE h) const noexcept {
        if (h != nullptr && h != INVALID_HANDLE_VALUE) {
            ::CloseHandle(h);
        }
    }
};
using UniqueHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, HandleCloser>;

struct CoTaskMemDeleter {
    void operator()(void* p) const noexcept { ::CoTaskMemFree(p); }
};

BOOL CALLBACK MonitorEnumProc(HMONITOR monitor, HDC, LPRECT, LPARAM userData) {
    auto* monitors = reinterpret_cast<std::vector<MonitorInfo>*>(userData);

    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (::GetMonitorInfoW(monitor, &info) == 0) {
        return TRUE;  // Skip this one, keep enumerating.
    }

    MonitorInfo entry;
    entry.handle = monitor;
    entry.bounds = info.rcMonitor;
    entry.workArea = info.rcWork;
    entry.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    entry.deviceName = info.szDevice;

    UINT dpiX = kDefaultDpi;
    UINT dpiY = kDefaultDpi;
    if (SUCCEEDED(::GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        entry.dpi = dpiX;
    }

    monitors->push_back(std::move(entry));
    return TRUE;
}

}  // namespace

std::string Utf8FromWide(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    const int required = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                               nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(required), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(),
                          required, nullptr, nullptr);
    return result;
}

std::wstring WideFromUtf8(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int required = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                               nullptr, 0);
    if (required <= 0) {
        return {};
    }
    std::wstring result(static_cast<size_t>(required), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(),
                          required);
    return result;
}

std::string FormatWin32Error(DWORD error) {
    LPWSTR buffer = nullptr;
    const DWORD length = ::FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&buffer),
        0, nullptr);

    const std::unique_ptr<wchar_t, LocalFreeDeleter> owned(buffer);
    if (length == 0 || buffer == nullptr) {
        return std::format("0x{:08X}", error);
    }

    std::wstring message(buffer, length);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L'.')) {
        message.pop_back();
    }
    return std::format("0x{:08X}: {}", error, Utf8FromWide(message));
}

bool EqualsIgnoreCase(std::wstring_view a, std::wstring_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    if (a.empty()) {
        return true;
    }
    return ::CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(),
                                  static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

std::filesystem::path AppDataDirectory() {
    PWSTR raw = nullptr;
    if (FAILED(::SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &raw))) {
        return {};
    }
    const std::unique_ptr<wchar_t, CoTaskMemDeleter> owned(raw);
    return std::filesystem::path(raw) / L"OverlayDesk";
}

UINT GetDpiForWindowSafe(HWND window) noexcept {
    if (window == nullptr || ::IsWindow(window) == 0) {
        return kDefaultDpi;
    }
    const UINT dpi = ::GetDpiForWindow(window);
    return dpi != 0 ? dpi : kDefaultDpi;
}

std::vector<MonitorInfo> EnumerateMonitors() {
    std::vector<MonitorInfo> monitors;
    ::EnumDisplayMonitors(nullptr, nullptr, &MonitorEnumProc, reinterpret_cast<LPARAM>(&monitors));

    // Stable, human-meaningful order: primary first, then left-to-right, top-to-bottom.
    // The UI persists a monitor index, so the order must not depend on enumeration luck.
    std::sort(monitors.begin(), monitors.end(), [](const MonitorInfo& a, const MonitorInfo& b) {
        if (a.primary != b.primary) return a.primary;
        if (a.bounds.left != b.bounds.left) return a.bounds.left < b.bounds.left;
        return a.bounds.top < b.bounds.top;
    });
    return monitors;
}

MonitorInfo GetMonitorForWindow(HWND window) {
    const HMONITOR monitor = (window != nullptr && ::IsWindow(window) != 0)
                                 ? ::MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST)
                                 : ::MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);

    for (auto& info : EnumerateMonitors()) {
        if (info.handle == monitor) {
            return info;
        }
    }

    MonitorInfo fallback;
    fallback.handle = monitor;
    fallback.bounds = RECT{0, 0, ::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN)};
    fallback.workArea = fallback.bounds;
    fallback.primary = true;
    return fallback;
}

RECT GetVisibleWindowBounds(HWND window) noexcept {
    RECT bounds{};
    if (window == nullptr || ::IsWindow(window) == 0) {
        return bounds;
    }
    if (FAILED(::DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds))) ||
        RectWidth(bounds) <= 0 || RectHeight(bounds) <= 0) {
        ::GetWindowRect(window, &bounds);
    }
    return bounds;
}

bool IsWindowCloaked(HWND window) noexcept {
    DWORD cloaked = 0;
    if (FAILED(::DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))) {
        return false;
    }
    return cloaked != 0;
}

void RestoreAndFocusWindow(HWND window) noexcept {
    if (window == nullptr || ::IsWindow(window) == 0) {
        return;
    }

    // SW_RESTORE on a window that is not minimized would undo a maximise, which is not what
    // was asked for - so only the minimized case is restored, and the rest is just a raise.
    if (::IsIconic(window) != 0) {
        ::ShowWindow(window, SW_RESTORE);
    }

    ::BringWindowToTop(window);

    // Windows can refuse this when the caller is not the foreground process. It is allowed
    // here because the user just clicked inside our own window, which is exactly the case the
    // foreground rules are written to permit. If it is refused anyway, BringWindowToTop above
    // has already done the visible part of the job.
    ::SetForegroundWindow(window);
}

std::wstring GetWindowTitleText(HWND window) {
    const int length = ::GetWindowTextLengthW(window);
    if (length <= 0) {
        return {};
    }
    std::wstring title(static_cast<size_t>(length) + 1, L'\0');
    const int written = ::GetWindowTextW(window, title.data(), length + 1);
    title.resize(static_cast<size_t>(std::max(written, 0)));
    return title;
}

std::wstring GetWindowProcessImageName(HWND window) {
    DWORD processId = 0;
    ::GetWindowThreadProcessId(window, &processId);
    if (processId == 0) {
        return {};
    }

    const UniqueHandle process(
        ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId));
    if (!process) {
        return {};
    }

    wchar_t buffer[MAX_PATH]{};
    DWORD size = static_cast<DWORD>(std::size(buffer));
    if (::QueryFullProcessImageNameW(process.get(), 0, buffer, &size) == 0) {
        return {};
    }

    return std::filesystem::path(std::wstring_view(buffer, size)).filename().wstring();
}

}  // namespace overlaydesk
