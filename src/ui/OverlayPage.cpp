// UI-SPEC.md "Overlay" tab: every control the spec lists, in the order it lists them.

#include <imgui.h>

#include <algorithm>
#include <format>
#include <string>

#include "core/RenderMath.h"
#include "ui/ControlPanel.h"
#include "util/Win32Helpers.h"

namespace overlaydesk::ui {
namespace {

constexpr const char* kAspectPresets[] = {"16:9", "4:3", "16:10", "3:2", "5:4", "1:1", "21:9"};

}  // namespace

void ControlPanel::DrawOverlayPage(AppState& state, const PanelActions& actions) {
    OverlaySettings& overlay = state.settings.overlay;
    bool dirty = false;
    bool needsApply = false;

    ImGui::Spacing();

    // --- Enable / edit ------------------------------------------------------------------

    bool enabled = overlay.enabled;
    if (ImGui::Checkbox("Overlay enabled", &enabled)) {
        overlay.enabled = enabled;
        dirty = true;
        if (actions.setOverlayEnabled) {
            actions.setOverlayEnabled(enabled);
        }
    }

    const bool editing = state.overlayMode == OverlayMode::Edit;
    if (ImGui::Button(editing ? "Leave Edit Mode" : "Edit Overlay") && actions.setEditMode) {
        actions.setEditMode(!editing);
    }
    ImGui::SameLine();
    if (ImGui::Button("Match Target Window") && actions.matchTargetWindow) {
        actions.matchTargetWindow();
    }
    ImGui::SameLine();
    if (state.editModeHotkey.empty()) {
        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f), "shortcut unavailable");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Another application already owns every Edit Mode shortcut. Use this button.");
        }
    } else {
        ImGui::TextDisabled("%s", state.editModeHotkey.c_str());
    }

    widgets::HelpText(
        "Edit Mode makes the overlay draggable and resizable and draws a border with handles. "
        "The same shortcut leaves it and restores click-through. You can also just turn "
        "Click-through off below, which makes it draggable without the border.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // --- Geometry -----------------------------------------------------------------------

    ImGui::BeginDisabled(overlay.fullscreen);

    int position[2] = {overlay.x, overlay.y};
    if (ImGui::DragInt2("Position", position, 1.0f)) {
        overlay.x = position[0];
        overlay.y = position[1];
        dirty = true;
        needsApply = true;
    }

    const int previousWidth = overlay.width;
    int size[2] = {overlay.width, overlay.height};
    if (ImGui::DragInt2("Size", size, 1.0f, 64, 16384)) {
        overlay.width = std::max(size[0], 64);
        overlay.height = std::max(size[1], 64);

        // Honour the aspect lock when the size is typed rather than dragged, so the two
        // input paths cannot disagree (RF-005). Whichever axis the user moved is the one
        // that stays put.
        if (overlay.lockAspectRatio) {
            const float ratio = ParseAspectRatio(overlay.aspectRatio.c_str());
            const bool widthMoved = size[0] != previousWidth;
            const SizeI corrected = ApplyAspectRatio(SizeI{overlay.width, overlay.height}, ratio,
                                                     /*adjustHeight=*/widthMoved);
            overlay.width = corrected.width;
            overlay.height = corrected.height;
        }
        dirty = true;
        needsApply = true;
    }

    ImGui::EndDisabled();

    ImGui::Spacing();

    // --- Locks --------------------------------------------------------------------------

    if (ImGui::Checkbox("Resizable", &overlay.resizable)) {
        dirty = true;
        needsApply = true;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Lock position", &overlay.lockPosition)) {
        dirty = true;
        needsApply = true;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Lock size", &overlay.lockSize)) {
        dirty = true;
        needsApply = true;
    }

    if (ImGui::Checkbox("Lock aspect ratio", &overlay.lockAspectRatio)) {
        dirty = true;
    }
    if (overlay.lockAspectRatio) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::BeginCombo("##aspect", overlay.aspectRatio.c_str())) {
            for (const char* preset : kAspectPresets) {
                const bool selected = overlay.aspectRatio == preset;
                if (ImGui::Selectable(preset, selected)) {
                    overlay.aspectRatio = preset;
                    dirty = true;
                }
            }
            ImGui::EndCombo();
        }

        if (ParseAspectRatio(overlay.aspectRatio.c_str()) <= 0.0f) {
            ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f),
                               "Unrecognised ratio; the lock is inactive.");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // --- Composition --------------------------------------------------------------------

    if (ImGui::Checkbox("Always on top", &overlay.alwaysOnTop)) {
        dirty = true;
        needsApply = true;
    }

    ImGui::BeginDisabled(editing);
    if (ImGui::Checkbox("Click-through", &overlay.clickThrough)) {
        dirty = true;
        needsApply = true;
    }
    ImGui::EndDisabled();
    if (editing) {
        ImGui::SameLine();
        ImGui::TextDisabled("(suspended while editing)");
    }

    if (widgets::PercentSlider("Opacity", &overlay.opacity, 1.0f)) {
        dirty = true;
        // Opacity reaches the screen through the shader constant buffer, so a static source
        // needs an explicit repaint to show the change.
        if (actions.refreshOverlay) {
            actions.refreshOverlay();
        }
    }

    ImGui::Spacing();

    if (ImGui::Checkbox("Fullscreen", &overlay.fullscreen)) {
        dirty = true;
        needsApply = true;
    }

    if (overlay.fullscreen) {
        const auto monitors = EnumerateMonitors();
        std::string preview = "Monitor 1";
        if (!monitors.empty()) {
            const size_t index = std::min(static_cast<size_t>(std::max(overlay.monitorIndex, 0)),
                                          monitors.size() - 1);
            preview = std::format("Monitor {} - {}x{}{}", index + 1,
                                  RectWidth(monitors[index].bounds),
                                  RectHeight(monitors[index].bounds),
                                  monitors[index].primary ? " (primary)" : "");
        }

        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::BeginCombo("Monitor", preview.c_str())) {
            for (size_t i = 0; i < monitors.size(); ++i) {
                const std::string label =
                    std::format("Monitor {} - {}x{}{}", i + 1, RectWidth(monitors[i].bounds),
                                RectHeight(monitors[i].bounds),
                                monitors[i].primary ? " (primary)" : "");
                if (ImGui::Selectable(label.c_str(),
                                      static_cast<int>(i) == overlay.monitorIndex)) {
                    overlay.monitorIndex = static_cast<int>(i);
                    dirty = true;
                    needsApply = true;
                }
            }
            ImGui::EndCombo();
        }
    }

    if (dirty) {
        state.settingsDirty = true;
    }
    if (needsApply && actions.applyOverlaySettings) {
        actions.applyOverlaySettings();
    }
}

}  // namespace overlaydesk::ui
