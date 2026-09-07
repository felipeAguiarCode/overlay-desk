#include "ui/ControlPanel.h"

#include <imgui.h>

#include <format>

#include "graphics/ShaderManager.h"
#include "util/Log.h"
#include "util/Win32Helpers.h"

namespace overlaydesk::ui {
namespace {

constexpr std::chrono::seconds kTargetRefreshInterval{2};

const char* StatusText(CaptureStatus status) {
    switch (status) {
        case CaptureStatus::Idle: return "Idle";
        case CaptureStatus::Running: return "Capturing";
        case CaptureStatus::Paused: return "Paused";
        case CaptureStatus::TargetClosed: return "Target closed";
        case CaptureStatus::Unsupported: return "Capture unsupported";
        case CaptureStatus::Failed: return "Capture failed";
    }
    return "Unknown";
}

ImVec4 StatusColor(CaptureStatus status) {
    switch (status) {
        case CaptureStatus::Running: return ImVec4(0.35f, 0.85f, 0.45f, 1.0f);
        case CaptureStatus::Paused: return ImVec4(0.95f, 0.75f, 0.25f, 1.0f);
        case CaptureStatus::TargetClosed:
        case CaptureStatus::Unsupported:
        case CaptureStatus::Failed: return ImVec4(0.95f, 0.40f, 0.40f, 1.0f);
        case CaptureStatus::Idle: break;
    }
    return ImVec4(0.65f, 0.65f, 0.70f, 1.0f);
}

}  // namespace

// --------------------------------------------------------------------------------------
// Shared widgets
// --------------------------------------------------------------------------------------

namespace widgets {

bool BeginModuleCard(const char* label, bool* enabled, bool* resetPressed) {
    ImGui::PushID(label);
    ImGui::BeginChild(label, ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY,
                      ImGuiWindowFlags_NoScrollbar);

    // Captured before anything is emitted, while the cursor is still at the left edge:
    // GetContentRegionAvail only describes what is left from the current cursor position.
    const float regionRight = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;

    ImGui::TextUnformatted(label);

    bool changed = false;
    if (enabled != nullptr) {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float buttonWidth = ImGui::CalcTextSize("OFF").x + style.FramePadding.x * 4.0f;

        // SameLine takes a window-local X, so this right-aligns the toggle without the
        // caller having to know how wide the card is.
        ImGui::SameLine(regionRight - buttonWidth);

        // UI-SPEC.md: "estado ON/OFF sempre evidente". Colour carries that at a glance,
        // rather than making the user read a three-letter label.
        const ImVec4 on(0.13f, 0.55f, 0.30f, 1.0f);
        const ImVec4 off(0.24f, 0.24f, 0.28f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, *enabled ? on : off);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              *enabled ? ImVec4(0.17f, 0.68f, 0.38f, 1.0f)
                                       : ImVec4(0.32f, 0.32f, 0.38f, 1.0f));
        if (ImGui::Button(*enabled ? "ON" : "OFF", ImVec2(buttonWidth, 0))) {
            *enabled = !*enabled;
            changed = true;
        }
        ImGui::PopStyleColor(2);

        // UI-SPEC.md asks for a per-module reset. It belongs on the header row next to the
        // state it resets, not stranded on a line of its own.
        if (resetPressed != nullptr) {
            const float resetWidth = ImGui::CalcTextSize("Reset").x + style.FramePadding.x * 3.0f;
            ImGui::SameLine(regionRight - buttonWidth - resetWidth - style.ItemSpacing.x);
            *resetPressed = ImGui::SmallButton("Reset");
        }
    }

    ImGui::Separator();
    return changed;
}

void EndModuleCard() {
    ImGui::EndChild();
    ImGui::PopID();
    ImGui::Spacing();
}

bool BeginAdvanced(const char* id, bool* open) {
    if (ImGui::ArrowButton(id, *open ? ImGuiDir_Down : ImGuiDir_Right)) {
        *open = !*open;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Advanced");
    if (*open) {
        ImGui::Indent(ImGui::GetStyle().IndentSpacing * 0.5f);
    }
    return *open;
}

void EndAdvanced() {
    ImGui::Unindent(ImGui::GetStyle().IndentSpacing * 0.5f);
}

bool PercentSlider(const char* label, float* value, float defaultValue) {
    // FILTERS-AND-EFFECTS.md: intensity is 0..1 internally, 0-100% in the UI.
    float percent = *value * 100.0f;
    const bool changed = ImGui::SliderFloat(label, &percent, 0.0f, 100.0f, "%.0f%%");
    if (changed) {
        *value = percent / 100.0f;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        *value = defaultValue;
        return true;
    }
    return changed;
}

bool RangedSlider(const char* label, float* value, float minimum, float maximum,
                  float defaultValue, const char* format) {
    const bool changed = ImGui::SliderFloat(label, value, minimum, maximum, format);
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        *value = defaultValue;
        return true;
    }
    return changed;
}

void HelpText(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.60f, 0.66f, 1.0f));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

}  // namespace widgets

// --------------------------------------------------------------------------------------
// ControlPanel
// --------------------------------------------------------------------------------------

void ControlPanel::RefreshTargetsIfStale() {
    const auto now = std::chrono::steady_clock::now();
    if (!m_targetsDirty && now - m_lastTargetRefresh < kTargetRefreshInterval) {
        return;
    }
    m_targets = EnumerateCaptureTargets();
    m_lastTargetRefresh = now;
    m_targetsDirty = false;
}

void ControlPanel::Draw(AppState& state, const PanelActions& actions) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (!ImGui::Begin("##OverlayDeskPanel", nullptr, flags)) {
        ImGui::End();
        return;
    }

    DrawStatusBar(state);
    ImGui::Spacing();

    // RF-018 lists UI preferences among the things that persist, and reopening on the tab
    // the user left is the only part of that which is not already automatic.
    const int restoreTab = m_pendingTabRestore ? state.settings.ui.activeTab : -1;
    const auto tabFlags = [restoreTab](int index) {
        return index == restoreTab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    };

    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("Target", nullptr, tabFlags(0))) {
            state.settings.ui.activeTab = 0;
            DrawTargetPage(state, actions);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Overlay", nullptr, tabFlags(1))) {
            state.settings.ui.activeTab = 1;
            DrawOverlayPage(state, actions);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Filters", nullptr, tabFlags(2))) {
            state.settings.ui.activeTab = 2;
            DrawFiltersPage(state, actions);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Effects", nullptr, tabFlags(3))) {
            state.settings.ui.activeTab = 3;
            DrawEffectsPage(state, actions);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Presets", nullptr, tabFlags(4))) {
            state.settings.ui.activeTab = 4;
            DrawPresetsPage(state, actions);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Settings", nullptr, tabFlags(5))) {
            state.settings.ui.activeTab = 5;
            DrawSettingsPage(state, actions);
            ImGui::EndTabItem();
        }
        // Appended rather than slotted in next to Settings: ui.activeTab is an index, and
        // inserting a tab before Settings would reopen an upgrading user on the wrong page.
        if (ImGui::BeginTabItem("Hotkeys", nullptr, tabFlags(6))) {
            state.settings.ui.activeTab = 6;
            DrawHotkeysPage(state, actions);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    m_pendingTabRestore = false;

    ImGui::End();
}

void ControlPanel::DrawStatusBar(const AppState& state) {
    ImGui::TextColored(StatusColor(state.captureStatus), "%s", StatusText(state.captureStatus));

    ImGui::SameLine();
    if (state.captureStatus == CaptureStatus::Running && state.stats.sourceWidth > 0) {
        ImGui::TextDisabled("| %ux%u | %.0f fps", state.stats.sourceWidth, state.stats.sourceHeight,
                            static_cast<double>(state.stats.renderFps));
    } else if (!state.statusMessage.empty()) {
        ImGui::TextDisabled("| %s", state.statusMessage.c_str());
    } else {
        ImGui::TextDisabled("| no source selected");
    }

    ImGui::SameLine();
    ImGui::TextDisabled("| overlay %s%s", state.overlayVisible ? "on" : "off",
                        state.overlayMode == OverlayMode::Edit ? " (edit)" : "");
}

void ControlPanel::DrawPendingPage(const char* title, const char* milestone,
                                   const char* summary) {
    ImGui::Spacing();
    ImGui::TextUnformatted(title);
    ImGui::TextDisabled("Scheduled for %s.", milestone);
    ImGui::Spacing();
    widgets::HelpText(summary);
}

void ControlPanel::DrawSettingsPage(AppState& state, const PanelActions& actions) {
    ImGui::Spacing();

    RenderSettings& render = state.settings.render;

    // RNF-003.
    static const char* kFpsModes[] = {"Match source", "60 FPS cap", "30 FPS cap", "Unlimited"};
    int fpsMode = static_cast<int>(render.fpsMode);
    if (ImGui::Combo("FPS mode", &fpsMode, kFpsModes, IM_ARRAYSIZE(kFpsModes))) {
        render.fpsMode = static_cast<FpsMode>(fpsMode);
        state.settingsDirty = true;
        if (actions.applyRenderSettings) {
            actions.applyRenderSettings();
        }
    }

    if (render.fpsMode == FpsMode::MatchSource) {
        if (ImGui::SliderInt("Operational cap", &render.fpsCap, 15, 240, "%d fps")) {
            state.settingsDirty = true;
            if (actions.applyRenderSettings) {
                actions.applyRenderSettings();
            }
        }
        widgets::HelpText(
            "Match source follows the captured window's own pace, capped here so a "
            "high-refresh source cannot drag the overlay's GPU cost up with it.");
    }

    if (ImGui::Checkbox("Pause rendering while the source is minimized",
                        &render.pauseWhenSourceMinimized)) {
        state.settingsDirty = true;
        if (actions.applyRenderSettings) {
            actions.applyRenderSettings();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    static const char* kLogLevels[] = {"trace", "debug", "info", "warn", "error", "off"};
    int logLevel = static_cast<int>(ParseLogLevel(state.settings.ui.logLevel));
    if (ImGui::Combo("Log level", &logLevel, kLogLevels, IM_ARRAYSIZE(kLogLevels))) {
        state.settings.ui.logLevel = kLogLevels[logLevel];
        SetLogLevel(static_cast<LogLevel>(logLevel));
        state.settingsDirty = true;
    }

    ImGui::Spacing();

    if (ImGui::Button("Open config folder") && actions.openConfigFolder) {
        actions.openConfigFolder();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save now") && actions.saveSettings) {
        actions.saveSettings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset to defaults") && actions.resetToDefaults) {
        actions.resetToDefaults();
    }

    if (ShaderManager::HotReloadSupported()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("Debug build");
        if (ImGui::Button("Reload shaders") && actions.reloadShaders) {
            actions.reloadShaders();
        }
        widgets::HelpText(
            "Recompiles shaders/OverlayDeskVS.hlsl and OverlayDeskPS.hlsl from disk. A "
            "compile error keeps the shaders currently in use and reports the message in "
            "the log.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Overlay Desk %s", OVERLAYDESK_VERSION);
}

}  // namespace overlaydesk::ui
