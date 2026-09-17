// RF-001 / AT-002: list the windows that can be captured and start capture on the one the
// user picks, without restarting the application.

#include <imgui.h>

#include "capture/CaptureSession.h"
#include "ui/ControlPanel.h"
#include "util/Win32Helpers.h"

namespace overlaydesk::ui {

void ControlPanel::DrawTargetPage(AppState& state, const PanelActions& actions) {
    ImGui::Spacing();

    if (!CaptureSession::IsSupported()) {
        ImGui::TextColored(ImVec4(0.95f, 0.40f, 0.40f, 1.0f),
                           "Windows.Graphics.Capture is not available on this system.");
        widgets::HelpText(
            "Window capture needs Windows 10 version 1903 or newer. The overlay itself "
            "still works; it just has nothing to show.");
        return;
    }

    if (ImGui::Button("Refresh list")) {
        RequestTargetRefresh();
    }

    // Start and Stop as a pair. Stopping used to be a one-way door: the only way back was to
    // find the window in the list again, even though the application still knows exactly which
    // one it was.
    const bool capturing = state.HasTarget();
    const bool canResume = !capturing && actions.canResumeTarget && actions.canResumeTarget();

    ImGui::SameLine();
    ImGui::BeginDisabled(!canResume);
    if (ImGui::Button("Start capture") && actions.resumeTarget) {
        if (!actions.resumeTarget()) {
            // The window is gone. The status message says so; refreshing the list is what the
            // user needs next, so do it for them.
            RequestTargetRefresh();
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!capturing);
    if (ImGui::Button("Stop capture") && actions.clearTarget) {
        actions.clearTarget();
    }
    ImGui::EndDisabled();

    ImGui::Spacing();

    if (capturing) {
        ImGui::Text("Current: %s", Utf8FromWide(state.targetTitle).c_str());
        ImGui::TextDisabled("%s", Utf8FromWide(state.targetExecutable).c_str());
    } else if (canResume) {
        ImGui::TextDisabled("Stopped. Start capture resumes on %s",
                            state.settings.ui.lastTargetTitle.c_str());
    } else {
        ImGui::TextDisabled("No target selected.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    RefreshTargetsIfStale();

    if (m_targets.empty()) {
        widgets::HelpText("No capturable windows found. Open the emulator and refresh.");
        return;
    }

    widgets::HelpText(
        "Click to capture a window. Double-click to capture it and bring it to the front - "
        "which is the one to use for anything marked minimized, because a minimized window "
        "sends no frames at all and the overlay will sit on its placeholder until it is back.");
    ImGui::Spacing();

    if (ImGui::BeginChild("##targets", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
        for (const TargetWindowInfo& candidate : m_targets) {
            ImGui::PushID(candidate.window);

            const bool selected = candidate.window == state.targetWindow;
            const std::string title = Utf8FromWide(candidate.title);
            const std::string executable = Utf8FromWide(candidate.executable);

            if (ImGui::Selectable(title.empty() ? "(untitled)" : title.c_str(), selected,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                // Double-click means "use this one and show it to me". A single click only
                // picks, so the list stays browsable without windows jumping around.
                //
                // Raising it is not a convenience. A minimized window sends no frames at all,
                // so picking one leaves the overlay on its no-signal backdrop while every
                // control still appears to work - which is exactly the dead end this list used
                // to walk people into.
                //
                // Restore first, then capture: a minimized window reports a degenerate size,
                // and starting on that would build the frame pool at the wrong dimensions and
                // rely on the resize path to sort it out afterwards.
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && actions.revealWindow) {
                    actions.revealWindow(candidate.window);
                }

                if (actions.selectTarget) {
                    actions.selectTarget(candidate.window);
                }
            }

            // Secondary line: executable, size, and whether it is currently minimized -
            // enough for the user to tell two emulator windows apart.
            //
            // A minimized window has no meaningful size to report: Windows hands back something
            // like 223x32 for it, which looks like a real measurement and is not one. Printing
            // it next to genuine sizes invited exactly the wrong conclusion, so minimized rows
            // say so in words instead.
            ImGui::SameLine();
            if (candidate.minimized) {
                ImGui::TextDisabled("  %s  minimized - double-click to restore and capture",
                                    executable.c_str());
            } else {
                ImGui::TextDisabled("  %s  %dx%d", executable.c_str(),
                                    RectWidth(candidate.bounds), RectHeight(candidate.bounds));
            }

            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

}  // namespace overlaydesk::ui
