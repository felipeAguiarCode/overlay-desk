#pragma once

// ARCHITECTURE.md section 2: lifecycle, initialisation, shutdown, main loop, and the
// coordination between the subsystems. Nothing else owns another subsystem.

#include <Windows.h>

#include <chrono>
#include <memory>
#include <vector>

#include "capture/CaptureSession.h"
#include "core/AppState.h"
#include "core/PresetRepository.h"
#include "core/Settings.h"
#include "graphics/D3D11Device.h"
#include "graphics/Renderer.h"
#include "ui/ControlPanel.h"
#include "windowing/ControlWindow.h"
#include "windowing/HotkeyManager.h"
#include "windowing/OverlayWindow.h"
#include "windowing/WindowTracker.h"

namespace overlaydesk {

class Application {
public:
    Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    ~Application();

    bool Initialize(HINSTANCE instance);
    int Run();
    void Shutdown() noexcept;

private:
    // --- Loop ---------------------------------------------------------------------------
    void Tick();
    void PumpMessages(bool& quitRequested, int& exitCode);

    // --- Overlay ------------------------------------------------------------------------
    void CreateOverlay();
    void DestroyOverlay() noexcept;
    void UpdateOverlayPresence();
    void SetEditMode(bool editing);
    void RepaintOverlay();
    void RequestOverlayRepaint() noexcept { m_overlayRepaintRequested = true; }
    void OnOverlayResized(uint32_t width, uint32_t height);
    void HandleEscape();
    void CloseOverlayToControlPanel();
    void MatchOverlayToTarget();

    // --- Hotkeys ------------------------------------------------------------------------
    // Re-registers everything from the current settings. Called at startup and after every
    // rebind, because RegisterHotKey has no notion of editing a binding in place.
    void ApplyHotkeys();
    void TriggerHotkey(HotkeyAction action);
    void CyclePreset(int direction);

    // --- Capture ------------------------------------------------------------------------
    void SelectTarget(HWND target);
    void ClearTarget();
    // Restarts capture on the window the last session used, matched by title and executable.
    // ClearTarget deliberately leaves that description behind so this can find it again.
    bool ResumeLastTarget();
    void OnCaptureFrame(ID3D11ShaderResourceView* source, const SourceGeometry& geometry);
    void OnTargetClosed();
    void UpdateCapturePauseState();
    void ApplyRenderSettings();

    // --- Presets ------------------------------------------------------------------------
    Preset CurrentVisualState(std::string name) const;
    void ApplyPreset(const Preset& preset);
    ui::PresetActions MakePresetActions();

    // --- Settings -----------------------------------------------------------------------
    void SaveSettingsNow();
    void ResetToDefaults();
    void OpenConfigFolder();

    // --- Graphics -----------------------------------------------------------------------
    bool RebuildGraphics();
    void HandleDeviceLoss();

    ui::PanelActions MakePanelActions();

    float ElapsedSeconds() const noexcept;

    HINSTANCE m_instance = nullptr;
    AppState m_state;
    std::unique_ptr<SettingsRepository> m_settingsRepository;
    std::unique_ptr<PresetRepository> m_presetRepository;

    D3D11Device m_device;
    OverlayWindow m_overlayWindow;
    Renderer m_renderer;
    CaptureSession m_capture;
    WindowTracker m_tracker;
    ControlWindow m_controlWindow;
    HotkeyManager m_hotkeys;
    ui::ControlPanel m_panel;

    std::chrono::steady_clock::time_point m_startTime{};
    std::chrono::steady_clock::time_point m_lastTrackerPoll{};
    std::chrono::steady_clock::time_point m_lastOverlayRender{};
    std::chrono::steady_clock::time_point m_lastAutoSave{};

    // True while the overlay is showing the idle backdrop rather than a captured frame.
    // Lets the idle repaint skip work once the backdrop is already on screen.
    bool m_overlayShowingIdle = false;
    // Set when a visual parameter changes, so the overlay updates even though the source
    // has not produced a new frame.
    bool m_overlayRepaintRequested = false;
    // Set once Initialize has fully succeeded. Shutdown uses it rather than m_running,
    // which the close handler has already cleared by the time shutdown runs.
    bool m_initialized = false;
    bool m_running = false;
    bool m_graphicsReady = false;

    // Double-Escape closes the overlay. The timestamp is what makes it a deliberate gesture
    // rather than something a stray Escape can trigger.
    std::chrono::steady_clock::time_point m_lastEscape{};
};

}  // namespace overlaydesk
