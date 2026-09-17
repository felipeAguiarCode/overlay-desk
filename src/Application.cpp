#include "Application.h"

#include <shellapi.h>

#include <algorithm>

#include "core/PresetRepository.h"
#include "graphics/ShaderManager.h"
#include "util/CrashHandler.h"
#include "util/Log.h"
#include "util/Win32Helpers.h"
#include "windowing/TargetWindow.h"

namespace overlaydesk {
namespace {

// The tracker only needs to notice a window closing or minimizing, so a quarter second is
// responsive enough and stays far away from the frame path (ARCHITECTURE.md section 3).
constexpr auto kTrackerPollInterval = std::chrono::milliseconds(250);

// CONFIGURATION.md allows global settings to be saved on change; batching them keeps a
// slider drag from writing the file on every pixel.
constexpr auto kAutoSaveInterval = std::chrono::seconds(2);

// While no capture is feeding the overlay, the loop repaints it at this rate so anything
// animated - the edit-mode pulse, a running glitch - keeps moving. When nothing is animated
// RepaintOverlay returns immediately, so a genuinely idle overlay still costs no GPU at all.
// When capture *is* flowing, FrameArrived has already repainted well inside this window and
// the gate in Tick skips the call entirely.
constexpr auto kIdleRedrawInterval = std::chrono::milliseconds(33);
// Long enough that a slow first frame is not reported as a fault, short enough that the user
// is told before they start blaming a filter.
constexpr auto kSilentSourceGrace = std::chrono::seconds(2);

// One UI frame per display refresh is plenty; the wait also yields the CPU so an idle
// panel costs nothing.
constexpr DWORD kTickTimeoutMs = 16;

}  // namespace

Application::~Application() {
    Shutdown();
}

bool Application::Initialize(HINSTANCE instance) {
    m_instance = instance;
    m_startTime = std::chrono::steady_clock::now();
    m_lastTrackerPoll = m_startTime;
    m_lastOverlayRender = m_startTime;
    m_lastAutoSave = m_startTime;

    const std::filesystem::path appData = AppDataDirectory();
    LogInitialize(appData);

    // Installed before anything else can fault, so an access violation - which walks past
    // every catch(...) in the program - still leaves a record and a minidump behind.
    InstallCrashHandler(appData / L"logs");

    LogInfo("Startup: Overlay Desk {}.", OVERLAYDESK_VERSION);

    m_settingsRepository = std::make_unique<SettingsRepository>(appData);
    m_state.settings = m_settingsRepository->Load();
    SetLogLevel(ParseLogLevel(m_state.settings.ui.logLevel));

    // Materialise the file on first run. CONFIGURATION.md treats settings.json as the
    // published contract, and a user who never touches a control should still be able to
    // find it and hand-edit it.
    if (!std::filesystem::exists(m_settingsRepository->FilePath())) {
        m_settingsRepository->Save(m_state.settings);
    }

    // RF-017: the shipped presets are written out as ordinary files, exactly like the ones
    // the user creates. Any preset added by a later version arrives on the next launch; the
    // ones the user deleted stay deleted.
    m_presetRepository = std::make_unique<PresetRepository>(appData);
    {
        const std::vector<std::string> added =
            m_presetRepository->SeedMissingBuiltIns(m_state.settings.ui.seededPresets);
        if (!added.empty()) {
            m_state.settings.ui.seededPresets.insert(m_state.settings.ui.seededPresets.end(),
                                                     added.begin(), added.end());
            m_state.settingsDirty = true;
            SaveSettingsNow();
        }
    }

    try {
        m_device.Create();
    } catch (...) {
        LogError("Startup: could not create a Direct3D 11 device: {}", DescribeCurrentException());
        ::MessageBoxW(nullptr,
                      L"Overlay Desk could not create a Direct3D 11 device.\n\n"
                      L"Update your graphics driver and try again.",
                      L"Overlay Desk", MB_ICONERROR | MB_OK);
        return false;
    }

    m_controlWindow.Create(m_instance, m_device, m_state.settings.ui,
                           ControlWindow::Callbacks{
                               .onCloseRequested = [this]() { m_running = false; },
                               .onHotkey =
                                   [this](int id) { TriggerHotkey(m_hotkeys.ActionForId(id)); },
                               .onEscape = [this]() { HandleEscape(); },
                           });
    if (!m_controlWindow.IsCreated()) {
        return false;
    }

    ApplyHotkeys();

    if (!CaptureSession::IsSupported()) {
        m_state.captureStatus = CaptureStatus::Unsupported;
        m_state.statusMessage = "Windows.Graphics.Capture is unavailable on this system";
        LogWarn("Startup: {}.", m_state.statusMessage);
    } else if (!m_capture.Initialize(m_device)) {
        m_state.captureStatus = CaptureStatus::Failed;
        m_state.statusMessage = m_capture.LastError();
    }

    ApplyRenderSettings();

    // AT-001: no overlay is created at startup unless there is a target to show in it.
    // Restoring the previous target is opt-in, and matches on title/executable because
    // CONFIGURATION.md forbids persisting an HWND or a PID.
    if (m_state.settings.ui.restoreLastTarget) {
        const HWND previous =
            FindWindowByDescription(WideFromUtf8(m_state.settings.ui.lastTargetTitle),
                                    WideFromUtf8(m_state.settings.ui.lastTargetExecutable));
        if (previous != nullptr) {
            SelectTarget(previous);
        }
    }

    UpdateOverlayPresence();
    m_initialized = true;
    m_running = true;
    return true;
}

void Application::Shutdown() noexcept {
    if (m_settingsRepository && m_state.settingsDirty) {
        SaveSettingsNow();
    }

    // Before the window goes: the registrations are held against that HWND.
    m_hotkeys.Clear();

    m_capture.Shutdown();
    m_tracker.Clear();
    DestroyOverlay();
    m_controlWindow.Destroy();
    m_device.Reset();

    if (m_initialized) {
        LogInfo("Shutdown: clean.");
        m_initialized = false;
    }
    m_running = false;
    LogShutdown();
}

int Application::Run() {
    int exitCode = 0;

    while (m_running) {
        bool quitRequested = false;
        PumpMessages(quitRequested, exitCode);
        if (quitRequested || !m_running) {
            break;
        }

        Tick();

        // Wait for either an input message or the tick timeout. This is what keeps an idle
        // application off the CPU instead of spinning on PeekMessage.
        ::MsgWaitForMultipleObjectsEx(0, nullptr, kTickTimeoutMs, QS_ALLINPUT,
                                      MWMO_INPUTAVAILABLE);
    }

    return exitCode;
}

void Application::PumpMessages(bool& quitRequested, int& exitCode) {
    MSG message{};
    while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
        if (message.message == WM_QUIT) {
            quitRequested = true;
            exitCode = static_cast<int>(message.wParam);
            m_running = false;
            return;
        }
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }
}

void Application::Tick() {
    const auto now = std::chrono::steady_clock::now();

    if (now - m_lastTrackerPoll >= kTrackerPollInterval) {
        m_lastTrackerPoll = now;
        m_tracker.Poll();
        UpdateCapturePauseState();
    }

    m_state.stats = m_capture.Stats();
    UpdateSilentSourceWatchdog(now);

    if (now - m_lastOverlayRender >= kIdleRedrawInterval) {
        RepaintOverlay();
    }

    m_controlWindow.Render([this]() { m_panel.Draw(m_state, MakePanelActions()); });

    if (m_state.settingsDirty && now - m_lastAutoSave >= kAutoSaveInterval) {
        SaveSettingsNow();
    }
}

// --------------------------------------------------------------------------------------
// Overlay
// --------------------------------------------------------------------------------------

void Application::CreateOverlay() {
    if (m_overlayWindow.IsCreated()) {
        return;
    }

    m_overlayWindow.Create(
        m_instance, m_state.settings.overlay,
        OverlayWindow::Callbacks{
            .onResized = [this](uint32_t width, uint32_t height) { OnOverlayResized(width, height); },
            .onGeometryChanged = [this]() { m_state.settingsDirty = true; },
            .onRedrawNeeded = [this]() { RepaintOverlay(); },
            .onEscape = [this]() { HandleEscape(); },
        });

    if (!m_overlayWindow.IsCreated()) {
        return;
    }

    if (!RebuildGraphics()) {
        DestroyOverlay();
    }
}

void Application::DestroyOverlay() noexcept {
    m_overlayShowingIdle = false;
    m_renderer.Reset();
    m_overlayWindow.Destroy();
    m_graphicsReady = false;
    m_state.overlayVisible = false;
    m_state.overlayMode = OverlayMode::Play;
}

bool Application::RebuildGraphics() {
    if (!m_overlayWindow.IsCreated()) {
        return false;
    }

    const RECT bounds = m_overlayWindow.Bounds();
    try {
        m_renderer.Create(m_device, m_overlayWindow.Handle(),
                          static_cast<uint32_t>(std::max(RectWidth(bounds), 1)),
                          static_cast<uint32_t>(std::max(RectHeight(bounds), 1)));
        m_graphicsReady = true;
        return true;
    } catch (...) {
        LogError("Overlay: renderer creation failed: {}", DescribeCurrentException());
        m_state.statusMessage = "Overlay renderer could not be created";
        m_graphicsReady = false;
        return false;
    }
}

void Application::HandleDeviceLoss() {
    LogError("Graphics: device lost (reason 0x{:08X}); rebuilding.",
             static_cast<uint32_t>(m_device.DeviceRemovedReason()));

    // Everything below hangs off the device, so it all goes and comes back together.
    const HWND target = m_capture.Target();
    m_capture.Shutdown();
    m_renderer.Reset();
    m_graphicsReady = false;
    m_device.Reset();

    try {
        m_device.Create();
    } catch (...) {
        m_state.captureStatus = CaptureStatus::Failed;
        m_state.statusMessage = "Direct3D device could not be recreated";
        LogError("Graphics: device recreation failed: {}", DescribeCurrentException());
        return;
    }

    if (!RebuildGraphics()) {
        return;
    }
    if (m_capture.Initialize(m_device) && target != nullptr) {
        SelectTarget(target);
    }
}

void Application::UpdateOverlayPresence() {
    // AT-001: the overlay exists when it is enabled and there is something to put in it,
    // or when the user is deliberately positioning it in edit mode.
    const bool wanted = m_state.settings.overlay.enabled &&
                        (m_state.HasTarget() || m_state.overlayMode == OverlayMode::Edit);

    if (wanted && !m_overlayWindow.IsCreated()) {
        CreateOverlay();
    } else if (!wanted && m_overlayWindow.IsCreated()) {
        DestroyOverlay();
        return;
    }

    if (m_overlayWindow.IsCreated()) {
        m_overlayWindow.Show(wanted);
        m_state.overlayVisible = wanted;
    }
}

// ADR-0012. Escape opens the overlay's quick menu. It only reaches the overlay when the
// overlay has focus - Play Mode carries WS_EX_NOACTIVATE, so while a game is in front the
// keystroke goes to the game and the ShowQuickMenu shortcut is the way in.
void Application::HandleEscape() {
    ShowQuickMenu();
}

void Application::ShowQuickMenu() {
    // Nothing on screen to hang a menu off, and no sensible action behind any of its items.
    if (!m_overlayWindow.IsCreated() || !m_state.overlayVisible) {
        return;
    }

    switch (m_overlayWindow.ShowQuickMenu()) {
        case QuickMenuCommand::ShowControlPanel:
            m_controlWindow.BringToFront();
            return;

        case QuickMenuCommand::StopOverlay:
            CloseOverlayToControlPanel();
            return;

        case QuickMenuCommand::Quit:
            LogInfo("Quick menu: quit requested.");
            m_running = false;
            return;

        case QuickMenuCommand::None:
        default:
            return;
    }
}

void Application::CloseOverlayToControlPanel() {
    // Leaving edit mode first keeps the window flags from being torn down half-applied.
    if (m_state.overlayMode == OverlayMode::Edit) {
        SetEditMode(false);
    }

    m_state.settings.overlay.enabled = false;
    m_state.settingsDirty = true;
    UpdateOverlayPresence();

    if (m_controlWindow.IsCreated()) {
        const HWND panel = m_controlWindow.Handle();
        ::ShowWindow(panel, SW_RESTORE);
        ::SetForegroundWindow(panel);
        // The Overlay tab is where the user turns it back on, so that is where they land.
        m_state.settings.ui.activeTab = 1;
        m_panel.RequestTabRestore();
    }

    LogInfo("Overlay: stopped; back to the control panel.");
}

void Application::MatchOverlayToTarget() {
    if (!m_state.HasTarget()) {
        return;
    }
    if (!m_overlayWindow.IsCreated()) {
        UpdateOverlayPresence();
    }
    if (m_overlayWindow.IsCreated()) {
        m_overlayWindow.MatchBounds(GetVisibleWindowBounds(m_state.targetWindow));
    }
}

// --- Hotkeys ---------------------------------------------------------------------------

void Application::ApplyHotkeys() {
    if (!m_controlWindow.IsCreated()) {
        return;
    }

    m_hotkeys.Apply(m_controlWindow.Handle(), m_state.settings.hotkeys);

    // The Overlay tab reports the edit-mode shortcut, and it has always been able to say "X or
    // Y" because two chords are bound to it by default. Only chords Windows actually accepted
    // are named - telling the user about a shortcut that will not fire is worse than silence.
    m_state.editModeHotkey.clear();
    const HotkeyAction editActions[] = {HotkeyAction::ToggleEditMode,
                                        HotkeyAction::ToggleEditModeAlt};
    for (const HotkeyAction action : editActions) {
        if (!m_hotkeys.Registered(action)) {
            continue;
        }
        const std::string name =
            DescribeHotkey(m_state.settings.hotkeys.bindings[static_cast<int>(action)]);
        if (name.empty()) {
            continue;
        }
        if (!m_state.editModeHotkey.empty()) {
            m_state.editModeHotkey += " or ";
        }
        m_state.editModeHotkey += name;
    }

    if (m_state.editModeHotkey.empty()) {
        LogWarn("Hotkeys: no Edit Mode shortcut is active; use the Edit Overlay button.");
    } else {
        LogInfo("Hotkeys: Edit Mode shortcut is {}.", m_state.editModeHotkey);
    }
}

void Application::TriggerHotkey(HotkeyAction action) {
    OverlaySettings& overlay = m_state.settings.overlay;

    switch (action) {
        case HotkeyAction::ToggleEditMode:
        case HotkeyAction::ToggleEditModeAlt:
            // Toggle: the same keystroke leaves Edit Mode and restores Play Mode's
            // click-through.
            SetEditMode(m_state.overlayMode != OverlayMode::Edit);
            return;

        case HotkeyAction::ToggleOverlay:
            overlay.enabled = !overlay.enabled;
            m_state.settingsDirty = true;
            UpdateOverlayPresence();
            return;

        case HotkeyAction::ToggleClickThrough:
            overlay.clickThrough = !overlay.clickThrough;
            m_state.settingsDirty = true;
            break;

        case HotkeyAction::ToggleAlwaysOnTop:
            overlay.alwaysOnTop = !overlay.alwaysOnTop;
            m_state.settingsDirty = true;
            break;

        case HotkeyAction::ToggleFullscreen:
            overlay.fullscreen = !overlay.fullscreen;
            m_state.settingsDirty = true;
            break;

        case HotkeyAction::MatchTargetWindow:
            MatchOverlayToTarget();
            return;

        case HotkeyAction::NextPreset:
            CyclePreset(1);
            return;

        case HotkeyAction::PreviousPreset:
            CyclePreset(-1);
            return;

        case HotkeyAction::ShowControlPanel:
            m_controlWindow.BringToFront();
            return;

        case HotkeyAction::ShowQuickMenu:
            ShowQuickMenu();
            return;

        case HotkeyAction::Count:
        default:
            // Not one of ours, or a stale id from a binding that has since been cleared.
            return;
    }

    // The three that fall through here changed a window flag rather than doing something on
    // their own, so they share the one path that pushes those flags at the window.
    if (m_overlayWindow.IsCreated()) {
        m_overlayWindow.ApplySettings();
    }
    UpdateOverlayPresence();
}

void Application::CyclePreset(int direction) {
    if (!m_presetRepository) {
        return;
    }

    const std::vector<Preset> presets = m_presetRepository->LoadAll();
    if (presets.empty()) {
        return;
    }

    // Where the cycle starts from is whatever the current look actually matches, not the last
    // name written to settings: the user may have edited a slider since, and stepping from a
    // preset that is no longer on screen would skip one.
    size_t index = 0;
    bool found = false;
    for (size_t i = 0; i < presets.size(); ++i) {
        if (PresetMatches(presets[i], m_state.settings.filters, m_state.settings.effects)) {
            index = i;
            found = true;
            break;
        }
    }

    const size_t count = presets.size();
    size_t next = 0;
    if (found) {
        const size_t step = direction >= 0 ? 1u : count - 1u;
        next = (index + step) % count;
    } else if (direction < 0) {
        // Coming from a look that matches nothing, stepping backwards should land on the end
        // of the list rather than on its second entry.
        next = count - 1u;
    }

    ApplyPreset(presets[next]);
    m_panel.RequestPresetRefresh();
    LogInfo("Hotkeys: applied preset '{}'.", presets[next].name);
}

void Application::SetEditMode(bool editing) {
    m_state.overlayMode = editing ? OverlayMode::Edit : OverlayMode::Play;
    m_overlayShowingIdle = false;

    // Edit mode is a legitimate reason to bring the overlay into being even with no
    // target: positioning it before starting the game is the natural workflow.
    UpdateOverlayPresence();

    if (m_overlayWindow.IsCreated()) {
        m_overlayWindow.SetEditMode(editing);
    }
    RepaintOverlay();
}

// Draws the overlay outside the capture callback: when there is no capture at all, and
// when there is one but the source has stopped repainting.
//
// Windows.Graphics.Capture only delivers a frame when the source content changes, so a
// paused emulator produces nothing. Re-rendering from the last frame's view keeps the
// picture correct and lets edit-mode chrome animate over it (AT-006), with no copy and no
// second texture - the frame stays on the GPU exactly where the capture put it.
// Handles WM_SIZE from the overlay. This runs INSIDE the modal resize loop that
// DefWindowProc spins while the user drags an edge, and that loop does not return to
// Application::Tick until the drag finishes. So both the swap chain resize and the redraw
// have to happen right here: deferring either one leaves the composition visual showing the
// old, smaller buffer for the whole duration of the drag, which is exactly the distorted
// shape a resize used to produce.
void Application::OnOverlayResized(uint32_t width, uint32_t height) {
    if (!m_graphicsReady || !m_renderer.IsValid()) {
        return;
    }
    if (!m_renderer.Resize(width, height)) {
        HandleDeviceLoss();
        return;
    }

    // Fresh back buffers hold undefined content, so something has to be drawn into them
    // before the next present - otherwise a source that is not producing frames leaves the
    // overlay showing garbage.
    m_overlayRepaintRequested = true;
    RepaintOverlay();
}

void Application::RepaintOverlay() {
    if (!m_graphicsReady || !m_renderer.IsValid() || !m_state.overlayVisible) {
        return;
    }

    const bool editing = m_state.overlayMode == OverlayMode::Edit;

    // Anything driven by g_time has to be redrawn even when nothing else changed. The glitch
    // is procedural and time-based (RF-016), so it animates on its own schedule rather than
    // the source's.
    const bool animating = editing || m_state.settings.effects.glitch.enabled;

    const bool captureLive = m_capture.IsRunning() && !m_capture.IsPaused();
    ID3D11ShaderResourceView* source = captureLive ? m_capture.LastFrameView() : nullptr;

    if (source != nullptr) {
        // FrameArrived already repaints whenever the source moves. The only reasons to draw
        // again are an animated effect and a parameter the user just changed.
        if (!animating && !m_overlayRepaintRequested) {
            return;
        }
    } else if (m_overlayShowingIdle && !animating && !m_overlayRepaintRequested) {
        // The backdrop is static and already on screen, so an idle overlay costs no GPU.
        return;
    }

    RenderFrame frame;
    frame.settings = &m_state.settings;
    frame.source = source;
    frame.sourceGeometry = source != nullptr ? m_capture.LastFrameGeometry() : SourceGeometry{};
    frame.editMode = editing;
    frame.timeSeconds = ElapsedSeconds();
    frame.editBorderThickness = m_overlayWindow.EditBorderThickness();

    if (!m_renderer.Render(frame)) {
        HandleDeviceLoss();
        return;
    }
    m_overlayShowingIdle = source == nullptr;
    m_overlayRepaintRequested = false;
    m_lastOverlayRender = std::chrono::steady_clock::now();
}

// --------------------------------------------------------------------------------------
// Capture
// --------------------------------------------------------------------------------------

void Application::SelectTarget(HWND target) {
    if (target == nullptr || ::IsWindow(target) == 0) {
        return;
    }

    const TargetWindowInfo info = DescribeWindow(target);
    m_state.targetWindow = target;
    m_state.targetTitle = info.title;
    m_state.targetExecutable = info.executable;

    m_state.settings.ui.lastTargetTitle = Utf8FromWide(info.title);
    m_state.settings.ui.lastTargetExecutable = Utf8FromWide(info.executable);
    m_state.settingsDirty = true;

    // The overlay has to exist before capture starts: the first frame arrives almost
    // immediately and needs a renderer to land in.
    UpdateOverlayPresence();

    const bool started = m_capture.Start(
        target, CaptureSession::Callbacks{
                    .onFrame =
                        [this](ID3D11ShaderResourceView* source, const SourceGeometry& geometry) {
                            OnCaptureFrame(source, geometry);
                        },
                    .onClosed = [this]() { OnTargetClosed(); },
                });

    if (started) {
        m_state.captureStatus = CaptureStatus::Running;
        m_state.statusMessage.clear();
        m_captureStartedAt = std::chrono::steady_clock::now();
        m_silentSourceReported = false;
        m_tracker.SetTarget(target,
                            WindowTracker::Events{
                                .onTargetClosed = [this]() { OnTargetClosed(); },
                                .onMinimizedChanged = [this](bool) { UpdateCapturePauseState(); },
                                .onBoundsChanged = [this](const RECT&) {},
                                .onMonitorChanged = [this](int) {},
                            });
        ApplyRenderSettings();
        UpdateCapturePauseState();
    } else {
        m_state.captureStatus = CaptureStatus::Failed;
        m_state.statusMessage = m_capture.LastError();
        m_state.targetWindow = nullptr;
        UpdateOverlayPresence();
    }
}

bool Application::ResumeLastTarget() {
    const UiSettings& ui = m_state.settings.ui;
    if (ui.lastTargetTitle.empty() && ui.lastTargetExecutable.empty()) {
        return false;
    }

    // By description, never by HWND: CONFIGURATION.md forbids persisting a handle, and the
    // window may well have been closed and reopened since.
    const HWND previous = FindWindowByDescription(WideFromUtf8(ui.lastTargetTitle),
                                                  WideFromUtf8(ui.lastTargetExecutable));
    if (previous == nullptr) {
        m_state.statusMessage = "The last target window is not open any more";
        LogInfo("Target: cannot resume '{}'; the window is gone.", ui.lastTargetTitle);
        return false;
    }

    SelectTarget(previous);
    return true;
}

void Application::ClearTarget() {
    m_overlayShowingIdle = false;
    m_capture.Stop();
    m_tracker.Clear();
    m_state.targetWindow = nullptr;
    m_state.targetTitle.clear();
    m_state.targetExecutable.clear();
    m_state.captureStatus = CaptureStatus::Idle;
    m_state.statusMessage.clear();
    m_state.stats = CaptureStats{};
    UpdateOverlayPresence();
    RepaintOverlay();
}

void Application::OnCaptureFrame(ID3D11ShaderResourceView* source,
                                 const SourceGeometry& geometry) {
    // Called straight from FrameArrived on this thread (ADR-0006). The texture behind
    // `source` belongs to the frame pool and is only valid until this returns, which is
    // exactly why the render happens here instead of being queued.
    if (!m_graphicsReady || !m_renderer.IsValid() || !m_state.overlayVisible) {
        return;
    }

    RenderFrame frame;
    frame.settings = &m_state.settings;
    frame.source = source;
    frame.sourceGeometry = geometry;
    frame.editMode = m_state.overlayMode == OverlayMode::Edit;
    frame.timeSeconds = ElapsedSeconds();
    frame.editBorderThickness = m_overlayWindow.EditBorderThickness();

    if (!m_renderer.Render(frame)) {
        HandleDeviceLoss();
        return;
    }
    m_overlayShowingIdle = false;
    m_lastOverlayRender = std::chrono::steady_clock::now();
}

void Application::OnTargetClosed() {
    m_overlayShowingIdle = false;
    // AT-014: the application survives, capture stops, the overlay goes to a safe state
    // and the user can pick a new source.
    LogInfo("Capture: target closed; returning to idle.");
    m_capture.Stop();
    m_tracker.Clear();
    m_state.targetWindow = nullptr;
    m_state.captureStatus = CaptureStatus::TargetClosed;
    m_state.statusMessage = "The captured window was closed";
    m_state.stats = CaptureStats{};
    m_panel.RequestTargetRefresh();
    UpdateOverlayPresence();
    RepaintOverlay();
}

// A capture that starts successfully and then never delivers a frame is indistinguishable, on
// screen, from one that is working: the overlay just keeps showing its no-signal backdrop. The
// commonest cause is a target that was already minimized when it was picked, but any window that
// stops producing content lands here too, so this reports the symptom rather than guessing at
// the cause.
void Application::UpdateSilentSourceWatchdog(std::chrono::steady_clock::time_point now) {
    if (!m_capture.IsRunning() || m_capture.IsPaused() ||
        m_state.captureStatus != CaptureStatus::Running) {
        return;
    }
    if (m_state.stats.framesArrived > 0) {
        return;
    }
    if (now - m_captureStartedAt < kSilentSourceGrace) {
        return;
    }

    m_state.statusMessage = "No frames from the source yet";
    if (!m_silentSourceReported) {
        m_silentSourceReported = true;
        LogWarn("Capture: no frame has arrived since the session started. The source is "
                "producing nothing - a minimized or hidden window is the usual reason.");
    }
}

void Application::UpdateCapturePauseState() {
    if (!m_capture.IsRunning()) {
        return;
    }

    // RNF-004 / AT-015. A minimized source produces nothing worth rendering, and a hidden
    // overlay has nowhere to put it.
    const bool sourceMinimized =
        m_state.settings.render.pauseWhenSourceMinimized && m_tracker.IsMinimized();
    const bool paused = sourceMinimized || !m_state.overlayVisible;

    if (paused != m_capture.IsPaused()) {
        m_capture.SetPaused(paused);
        // Releasing deferred allocations while parked keeps the working set flat over the
        // long idle stretches AT-016 measures.
        if (paused) {
            m_device.Trim();
        }
    }

    m_state.captureStatus = paused ? CaptureStatus::Paused : CaptureStatus::Running;
    m_state.statusMessage = paused ? "Source minimized" : std::string{};
}

void Application::ApplyRenderSettings() {
    m_capture.SetFrameInterval(FrameIntervalSeconds(m_state.settings.render));
    UpdateCapturePauseState();
}

// --------------------------------------------------------------------------------------
// Presets
// --------------------------------------------------------------------------------------

Preset Application::CurrentVisualState(std::string name) const {
    // CONFIGURATION.md: visual state only. Overlay geometry, the target and the render
    // settings are deliberately left out - they describe this session, not this look.
    Preset preset;
    preset.name = std::move(name);
    preset.filters = m_state.settings.filters;
    preset.effects = m_state.settings.effects;
    return preset;
}

void Application::ApplyPreset(const Preset& preset) {
    m_state.settings.filters = preset.filters;
    m_state.settings.effects = preset.effects;
    m_state.settings.ui.activePreset = preset.name;
    m_state.settingsDirty = true;

    // Picking a preset is a request to *see* it. Applying one to an overlay that is switched
    // off, or that has no target, used to change nothing on screen at all - the look was
    // stored and the user was left staring at their desktop wondering what had happened.
    //
    // So the preset brings the overlay up with it: resume the last target if capture is not
    // running, switch the overlay on, and lay it over the window it is filtering.
    if (!m_state.HasTarget()) {
        ResumeLastTarget();
    }

    if (m_state.HasTarget()) {
        if (!m_state.settings.overlay.enabled) {
            m_state.settings.overlay.enabled = true;
            LogInfo("Overlay: enabled by applying a preset.");
        }
        UpdateOverlayPresence();
        UpdateCapturePauseState();

        // Over the target, at the target's size. A preset describes a whole look, and a look
        // that only covers a corner of the window is not the one the user picked.
        MatchOverlayToTarget();
    }

    RequestOverlayRepaint();
    LogInfo("Presets: applied '{}'.", preset.name);
}

ui::PresetActions Application::MakePresetActions() {
    ui::PresetActions actions;
    if (!m_presetRepository) {
        return actions;
    }

    actions.list = [this]() { return m_presetRepository->LoadAll(); };
    actions.apply = [this](const Preset& preset) { ApplyPreset(preset); };

    actions.saveAs = [this](std::string name) {
        const std::string unique =
            PresetRepository::MakeUniqueName(name, m_presetRepository->LoadAll());
        Preset preset = CurrentVisualState(unique);
        // A preset the user made goes in their own group; the shipped categories describe what
        // the application decided a look was for, and it has no basis to guess that here.
        preset.category = kCategoryCustom;
        if (m_presetRepository->Save(preset)) {
            m_state.settings.ui.activePreset = preset.name;
            m_state.settingsDirty = true;
            LogInfo("Presets: saved '{}'.", preset.name);
        }
    };

    actions.overwrite = [this](const Preset& target) {
        Preset preset = CurrentVisualState(target.name);
        preset.file = target.file;
        // Overwriting the contents does not move it: it stays in whatever group it was in.
        preset.category = target.category;
        // A built-in the user has overwritten is theirs now; keeping the flag would imply
        // it still matches what shipped.
        preset.builtIn = false;
        if (m_presetRepository->Save(preset)) {
            m_state.settings.ui.activePreset = preset.name;
            m_state.settingsDirty = true;
            LogInfo("Presets: overwrote '{}' with the current look.", preset.name);
        }
    };

    actions.duplicate = [this](const Preset& source) {
        Preset copy = source;
        copy.name = PresetRepository::MakeUniqueName(source.name, m_presetRepository->LoadAll());
        copy.builtIn = false;
        copy.file.clear();
        if (m_presetRepository->Save(copy)) {
            LogInfo("Presets: duplicated '{}' as '{}'.", source.name, copy.name);
        }
    };

    actions.rename = [this](const Preset& target, std::string newName) {
        if (m_presetRepository->Rename(target, newName)) {
            if (m_state.settings.ui.activePreset == target.name) {
                m_state.settings.ui.activePreset = PresetRepository::SanitizeName(newName);
                m_state.settingsDirty = true;
            }
        }
    };

    actions.remove = [this](const Preset& target) {
        if (m_presetRepository->Delete(target) &&
            m_state.settings.ui.activePreset == target.name) {
            m_state.settings.ui.activePreset.clear();
            m_state.settingsDirty = true;
        }
    };

    actions.resetToNeutral = [this]() {
        const AppSettings defaults;
        m_state.settings.filters = defaults.filters;
        m_state.settings.effects = defaults.effects;
        m_state.settings.ui.activePreset.clear();
        m_state.settingsDirty = true;
        RequestOverlayRepaint();
    };

    return actions;
}

// --------------------------------------------------------------------------------------
// Settings
// --------------------------------------------------------------------------------------

void Application::SaveSettingsNow() {
    if (!m_settingsRepository) {
        return;
    }
    if (m_settingsRepository->Save(m_state.settings)) {
        m_state.settingsDirty = false;
        m_lastAutoSave = std::chrono::steady_clock::now();
    }
}

void Application::ResetToDefaults() {
    const AppSettings defaults;

    // Keep the runtime association with the current target; only the visual and window
    // configuration resets.
    m_state.settings.overlay = defaults.overlay;
    m_state.settings.filters = defaults.filters;
    m_state.settings.effects = defaults.effects;
    m_state.settings.render = defaults.render;
    m_state.settings.ui.logLevel = defaults.ui.logLevel;
    m_state.settings.ui.showAdvanced = defaults.ui.showAdvanced;

    SetLogLevel(ParseLogLevel(m_state.settings.ui.logLevel));
    m_state.settingsDirty = true;

    if (m_overlayWindow.IsCreated()) {
        m_overlayWindow.ApplySettings();
    }
    ApplyRenderSettings();
    UpdateOverlayPresence();
    SaveSettingsNow();
    LogInfo("Settings: reset to defaults.");
}

void Application::OpenConfigFolder() {
    if (!m_settingsRepository) {
        return;
    }
    const std::filesystem::path directory = m_settingsRepository->FilePath().parent_path();
    if (directory.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    ::ShellExecuteW(nullptr, L"open", directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

// --------------------------------------------------------------------------------------

ui::PanelActions Application::MakePanelActions() {
    ui::PanelActions actions;

    actions.selectTarget = [this](HWND target) { SelectTarget(target); };
    actions.clearTarget = [this]() { ClearTarget(); };
    actions.resumeTarget = [this]() { return ResumeLastTarget(); };
    actions.canResumeTarget = [this]() {
        const UiSettings& ui = m_state.settings.ui;
        return !m_state.HasTarget() &&
               !(ui.lastTargetTitle.empty() && ui.lastTargetExecutable.empty());
    };

    actions.setOverlayEnabled = [this](bool) { UpdateOverlayPresence(); };
    actions.applyOverlaySettings = [this]() {
        if (m_overlayWindow.IsCreated()) {
            m_overlayWindow.ApplySettings();
        }
        UpdateOverlayPresence();
    };
    actions.setEditMode = [this](bool editing) { SetEditMode(editing); };
    actions.matchTargetWindow = [this]() { MatchOverlayToTarget(); };
    actions.applyHotkeys = [this]() { ApplyHotkeys(); };
    actions.hotkeyUnavailable = [this](HotkeyAction action) {
        return m_hotkeys.Unavailable(action);
    };

    actions.refreshOverlay = [this]() { RequestOverlayRepaint(); };
    actions.restoreSourceWindow = [this]() {
        HWND target = m_tracker.Target();
        if (target == nullptr || ::IsWindow(target) == 0) {
            return;
        }
        // The tracker picks the change up on its next poll and UpdateCapturePauseState un-parks
        // the capture from there.
        RestoreAndFocusWindow(target);
        LogInfo("Tracker: restore requested for the source window.");
    };
    actions.revealWindow = [](HWND window) { RestoreAndFocusWindow(window); };
    actions.presets = MakePresetActions();
    actions.applyRenderSettings = [this]() { ApplyRenderSettings(); };
    actions.saveSettings = [this]() { SaveSettingsNow(); };
    actions.resetToDefaults = [this]() { ResetToDefaults(); };
    actions.openConfigFolder = [this]() { OpenConfigFolder(); };
    actions.reloadShaders = [this]() {
        if (!m_renderer.IsValid()) {
            return;
        }
        std::string error;
        if (m_renderer.Shaders().ReloadFromSource(m_device.Device(),
                                                  ShaderManager::DefaultSourceDirectory(), error)) {
            m_state.statusMessage = "Shaders reloaded";
            RepaintOverlay();
        } else {
            m_state.statusMessage = error;
        }
    };

    return actions;
}

float Application::ElapsedSeconds() const noexcept {
    const auto elapsed = std::chrono::steady_clock::now() - m_startTime;
    return std::chrono::duration<float>(elapsed).count();
}

}  // namespace overlaydesk
