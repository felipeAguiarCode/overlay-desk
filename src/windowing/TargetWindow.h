#pragma once

// RF-001: list the windows available as a capture source and describe the selected one.
// ARCHITECTURE.md lists exactly what a TargetWindow carries; none of it is persisted
// (DATA-MODEL.md forbids storing HWND or PID).

#include <Windows.h>

#include <string>
#include <vector>

namespace overlaydesk {

struct TargetWindowInfo {
    HWND window = nullptr;
    DWORD processId = 0;
    std::wstring title;
    std::wstring executable;  // file name only, e.g. "retroarch.exe"
    RECT bounds{};
    bool visible = false;
    bool minimized = false;
    int monitorIndex = 0;

    bool IsAlive() const noexcept { return window != nullptr && ::IsWindow(window) != 0; }
};

// True for top-level windows a user could reasonably pick: visible, titled, not our own,
// not a tool window, and not cloaked by the shell.
bool IsCaptureCandidate(HWND window);

// Snapshot of `window`. Safe to call on a handle that has since died - the result is then
// simply not alive.
TargetWindowInfo DescribeWindow(HWND window);

// Ordered by title so the list does not reshuffle between refreshes.
std::vector<TargetWindowInfo> EnumerateCaptureTargets();

// Best-effort re-selection by title and executable, used only when the user opted into
// restoring the last target. Returns a null handle when nothing matches.
HWND FindWindowByDescription(const std::wstring& title, const std::wstring& executable);

}  // namespace overlaydesk
