#pragma once

// Runtime state, as listed in ARCHITECTURE.md section 2. DATA-MODEL.md is explicit that
// none of the handles below are ever persisted - settings.json stores geometry and visual
// parameters only.

#include <Windows.h>

#include <cstdint>
#include <string>

#include "core/Settings.h"

namespace overlaydesk {

enum class CaptureStatus {
    Idle,          // no target selected
    Running,       // frames arriving
    Paused,        // source minimized, or the overlay is hidden (RNF-004)
    TargetClosed,  // AT-014: source went away, application stays up
    Unsupported,   // Windows.Graphics.Capture unavailable on this machine
    Failed,
};

// OVERLAY-WINDOW.md / ADR-0002. Locked is a property of the settings rather than a mode of
// its own, so the two runtime modes are Play and Edit.
enum class OverlayMode { Play, Edit };

struct CaptureStats {
    uint64_t framesArrived = 0;
    uint64_t framesRendered = 0;
    uint64_t framesDroppedByPacing = 0;
    uint64_t framesDroppedWhilePaused = 0;
    float renderFps = 0.0f;
    uint32_t sourceWidth = 0;
    uint32_t sourceHeight = 0;
};

struct AppState {
    AppSettings settings;

    // --- Target (runtime only) ---
    HWND targetWindow = nullptr;
    std::wstring targetTitle;
    std::wstring targetExecutable;

    // --- Capture ---
    CaptureStatus captureStatus = CaptureStatus::Idle;
    std::string statusMessage;
    CaptureStats stats;

    // --- Overlay ---
    OverlayMode overlayMode = OverlayMode::Play;
    bool overlayVisible = false;

    // Which Edit Mode shortcut the system actually accepted. Empty when every candidate was
    // already taken by another application, which the panel needs to say out loud - a
    // shortcut that silently does nothing is worse than no shortcut.
    std::string editModeHotkey;

    // Set by the UI whenever a control changes, consumed by the autosave tick so we write
    // settings.json at most once per second instead of once per slider pixel.
    bool settingsDirty = false;

    bool HasTarget() const noexcept {
        return targetWindow != nullptr && ::IsWindow(targetWindow) != 0;
    }
};

}  // namespace overlaydesk
