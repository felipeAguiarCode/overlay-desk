#pragma once

// UI-SPEC.md: a single window with Overlay / Filters / Effects / Presets / Settings tabs,
// plus a Target tab for RF-001. Milestones 0-3 ship Target, Overlay and Settings; the
// remaining tabs exist and say which milestone fills them in, so the navigation the spec
// describes is present from the start rather than appearing later.

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "core/AppState.h"
#include "core/PresetRepository.h"
#include "windowing/TargetWindow.h"

namespace overlaydesk::ui {

// RF-017. Grouped separately from the rest so the preset UI never touches the file system
// directly - it asks, the application stores.
struct PresetActions {
    std::function<std::vector<Preset>()> list;
    std::function<void(const Preset&)> apply;
    // Both capture the current visual state; saveAs creates a file, overwrite replaces one.
    std::function<void(std::string name)> saveAs;
    std::function<void(const Preset&)> overwrite;
    std::function<void(const Preset&)> duplicate;
    std::function<void(const Preset&, std::string newName)> rename;
    std::function<void(const Preset&)> remove;
    // Drops every visual parameter back to its default without touching stored presets.
    std::function<void()> resetToNeutral;
};

// Everything the panel can ask the application to do. Keeping these as callbacks means the
// UI never owns a window, a device or a capture session.
struct PanelActions {
    std::function<void(HWND target)> selectTarget;
    // Un-minimizes a window and raises it. Double-clicking a row in the target list both picks
    // it and shows it, because picking a window you cannot see is how you end up tuning filters
    // against a source that is sending nothing.
    std::function<void(HWND window)> revealWindow;
    std::function<void()> clearTarget;
    // Restarts capture on the window the last target described. Returns false when that window
    // is not open any more, which is the only case the button can fail in.
    std::function<bool()> resumeTarget;
    std::function<bool()> canResumeTarget;

    std::function<void(bool enabled)> setOverlayEnabled;
    std::function<void()> applyOverlaySettings;
    std::function<void(bool editing)> setEditMode;
    std::function<void()> matchTargetWindow;

    // Visual parameters reach the shader through the constant buffer on the next rendered
    // frame. A paused or static source produces no frames, so tuning a filter has to ask
    // for a repaint explicitly or the overlay would not move until the source did.
    std::function<void()> refreshOverlay;

    // Un-minimizes the source window. A minimized window delivers no frames at all, so the
    // overlay sits on its no-signal backdrop and every adjustment the user makes appears to do
    // nothing - the banner offers the one-click way out rather than leaving them to work out
    // why the picture never moves.
    std::function<void()> restoreSourceWindow;

    // Re-registers every global shortcut from the current settings. RegisterHotKey cannot edit
    // a binding in place, so a rebind is always an unregister-and-register of the whole set.
    std::function<void()> applyHotkeys;
    // True when the binding is set but Windows refused it, which normally means another
    // application holds the chord. The page has to say so, or the shortcut silently does
    // nothing.
    std::function<bool(HotkeyAction)> hotkeyUnavailable;

    std::function<void()> applyRenderSettings;
    std::function<void()> saveSettings;
    std::function<void()> resetToDefaults;
    std::function<void()> openConfigFolder;
    std::function<void()> reloadShaders;

    PresetActions presets;
};

class ControlPanel {
public:
    // Draws the whole panel into the current ImGui frame.
    void Draw(AppState& state, const PanelActions& actions);

    void RequestTargetRefresh() noexcept { m_targetsDirty = true; }
    void RequestPresetRefresh() noexcept { m_presetsDirty = true; }
    void RequestTabRestore() noexcept { m_pendingTabRestore = true; }

private:
    void DrawTargetPage(AppState& state, const PanelActions& actions);
    void DrawOverlayPage(AppState& state, const PanelActions& actions);
    void DrawFiltersPage(AppState& state, const PanelActions& actions);
    void DrawEffectsPage(AppState& state, const PanelActions& actions);
    void DrawPresetsPage(AppState& state, const PanelActions& actions);
    void DrawHotkeysPage(AppState& state, const PanelActions& actions);
    void DrawSettingsPage(AppState& state, const PanelActions& actions);
    void DrawPendingPage(const char* title, const char* milestone, const char* summary);
    void DrawStatusBar(const AppState& state);
    void DrawSourceWarning(const AppState& state, const PanelActions& actions);

    void RefreshTargetsIfStale();

    std::vector<TargetWindowInfo> m_targets;
    std::chrono::steady_clock::time_point m_lastTargetRefresh{};
    bool m_targetsDirty = true;

    std::string m_shaderReloadMessage;

    // Which row is waiting for a keystroke, or Count when none is. Rebinding is modal by
    // necessity: while it is armed the page swallows whatever key is pressed next, so only one
    // row can be armed at a time.
    HotkeyAction m_capturingHotkey = HotkeyAction::Count;

    // settings.json remembers which tab was open; this replays that choice on the first
    // frame and then gets out of the way so the user can navigate freely.
    bool m_pendingTabRestore = true;

    // Per-card "Advanced" disclosure state. Kept in the panel rather than in settings.json
    // because it is transient view state, not configuration.
    bool m_advancedDistortion = false;
    bool m_advancedChromatic = false;
    bool m_advancedColor = false;
    bool m_advancedScanlines = false;
    bool m_advancedVignette = false;
    bool m_advancedScope = false;
    bool m_advancedBloom = false;
    bool m_advancedFalseColour = false;
    bool m_advancedEdgeGlow = false;
    bool m_advancedLensDirt = false;
    bool m_advancedLensSoftness = false;
    bool m_advancedSharpen = false;
    bool m_advancedGlitch = false;
    bool m_advancedNoise = false;
    bool m_advancedFlicker = false;
    bool m_advancedJitter = false;
    bool m_advancedShimmer = false;
    bool m_advancedRollingShutter = false;
    bool m_advancedScanSweep = false;

    // --- Presets tab state ---
    std::vector<Preset> m_presets;
    bool m_presetsDirty = true;
    int m_selectedPreset = -1;
    char m_presetNameInput[64] = {};
    bool m_renaming = false;
    bool m_confirmingDelete = false;
};

// Shared helpers, defined in ControlPanel.cpp and used by the page implementations.
namespace widgets {

// UI-SPEC.md: "estado ON/OFF sempre evidente" - a header row carrying the module name on
// the left and, on the right, the per-module reset and the ON/OFF toggle.
// Returns true when the toggle changed; `resetPressed`, when supplied, reports the reset.
bool BeginModuleCard(const char* label, bool* enabled, bool* resetPressed = nullptr);
void EndModuleCard();

// Slider that shows its value numerically and restores `defaultValue` on double click,
// as UI-SPEC.md asks for.
bool PercentSlider(const char* label, float* value, float defaultValue);
bool RangedSlider(const char* label, float* value, float minimum, float maximum,
                  float defaultValue, const char* format = "%.2f");

// Collapsible "Advanced" section, per UI-SPEC.md ("parametros avancados recolhiveis").
bool BeginAdvanced(const char* id, bool* open);
void EndAdvanced();

void HelpText(const char* text);

}  // namespace widgets

}  // namespace overlaydesk::ui
