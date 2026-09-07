// UI-SPEC.md "Presets" tab: New, Save, Save As, Duplicate, Rename, Delete, Apply (RF-017).
//
// PRD section 11 suggests the starting set and adds "Custom". Custom is not stored as a file
// here: it is the state the panel reports whenever the live parameters do not match any
// preset, which is both truthful and one less thing to keep in sync.

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>

#include "ui/ControlPanel.h"

namespace overlaydesk::ui {
namespace {

void SetBuffer(char (&buffer)[64], const std::string& value) {
    const size_t count = std::min(value.size(), sizeof(buffer) - 1);
    std::memcpy(buffer, value.data(), count);
    buffer[count] = '\0';
}

}  // namespace

void ControlPanel::DrawPresetsPage(AppState& state, const PanelActions& actions) {
    const PresetActions& preset = actions.presets;

    if (m_presetsDirty && preset.list) {
        m_presets = preset.list();
        m_presetsDirty = false;
        m_selectedPreset = std::min(m_selectedPreset, static_cast<int>(m_presets.size()) - 1);
    }

    ImGui::Spacing();

    // --- What is actually on screen right now ------------------------------------------
    //
    // The stored name alone would lie the moment the user nudges a slider, so it is checked
    // against the live parameters every frame.
    const std::string& activeName = state.settings.ui.activePreset;
    const Preset* active = nullptr;
    for (const Preset& candidate : m_presets) {
        if (candidate.name == activeName) {
            active = &candidate;
            break;
        }
    }
    const bool activeIsClean =
        active != nullptr && PresetMatches(*active, state.settings.filters, state.settings.effects);

    ImGui::TextUnformatted("Current look:");
    ImGui::SameLine();
    if (active == nullptr) {
        ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.70f, 1.0f), "Custom");
    } else if (activeIsClean) {
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "%s", active->name.c_str());
    } else {
        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f), "%s (edited)", active->name.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // --- The list ------------------------------------------------------------------------

    const float listHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 6.0f;
    if (ImGui::BeginChild("##presets", ImVec2(0, std::max(listHeight, 120.0f)),
                          ImGuiChildFlags_Borders)) {
        if (m_presets.empty()) {
            widgets::HelpText("No presets yet. Set up a look and use Save As.");
        }

        // LoadAll returns the list already grouped, so a heading is emitted wherever the
        // category changes and the page never sorts or buckets anything itself. `visible`
        // carries the collapsed state down to the rows: a collapsed group still occupies its
        // indices, which is what keeps m_selectedPreset meaningful while one is folded away.
        std::string_view currentCategory;
        bool visible = true;
        bool first = true;

        for (int i = 0; i < static_cast<int>(m_presets.size()); ++i) {
            const Preset& item = m_presets[static_cast<size_t>(i)];

            if (first || item.category != currentCategory) {
                currentCategory = item.category;
                first = false;

                // Counting the group up front is what lets the header say how big it is, which
                // is the whole reason to fold it in the first place.
                int count = 0;
                for (size_t j = static_cast<size_t>(i); j < m_presets.size(); ++j) {
                    if (m_presets[j].category != currentCategory) {
                        break;
                    }
                    ++count;
                }

                // "###" makes everything after it the ID and everything before it the label, so
                // the count can change without ImGui deciding this is a different header and
                // forgetting whether the user had folded it.
                const std::string heading = std::string(currentCategory) + "  (" +
                                            std::to_string(count) + ")###" +
                                            std::string(currentCategory);
                visible = ImGui::CollapsingHeader(heading.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            }

            if (!visible) {
                continue;
            }

            ImGui::PushID(i);
            ImGui::Indent(ImGui::GetStyle().IndentSpacing * 0.5f);

            if (ImGui::Selectable(item.name.c_str(), m_selectedPreset == i,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                m_selectedPreset = i;
                m_renaming = false;
                m_confirmingDelete = false;
                SetBuffer(m_presetNameInput, item.name);

                // Double click applies, which is what every list like this does.
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && preset.apply) {
                    preset.apply(item);
                }
            }
            if (item.builtIn) {
                ImGui::SameLine();
                ImGui::TextDisabled("  built-in");
            }

            ImGui::Unindent(ImGui::GetStyle().IndentSpacing * 0.5f);
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    const bool hasSelection = m_selectedPreset >= 0 &&
                              m_selectedPreset < static_cast<int>(m_presets.size());
    const Preset* selected =
        hasSelection ? &m_presets[static_cast<size_t>(m_selectedPreset)] : nullptr;

    ImGui::Spacing();

    // --- Actions ---------------------------------------------------------------------------

    ImGui::BeginDisabled(!hasSelection);
    if (ImGui::Button("Apply") && selected != nullptr && preset.apply) {
        preset.apply(*selected);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save") && selected != nullptr && preset.overwrite) {
        preset.overwrite(*selected);
        m_presetsDirty = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Overwrite the selected preset with the current look");
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate") && selected != nullptr && preset.duplicate) {
        preset.duplicate(*selected);
        m_presetsDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Rename") && selected != nullptr) {
        m_renaming = true;
        m_confirmingDelete = false;
        SetBuffer(m_presetNameInput, selected->name);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete") && selected != nullptr) {
        m_confirmingDelete = true;
        m_renaming = false;
    }
    ImGui::EndDisabled();

    ImGui::Spacing();

    if (ImGui::Button("New") && preset.resetToNeutral) {
        // "New" starts a look from scratch. Nothing is written until the user says Save As,
        // so it cannot destroy a stored preset by accident.
        preset.resetToNeutral();
        m_selectedPreset = -1;
        SetBuffer(m_presetNameInput, "");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reset every filter and effect to its default; nothing is saved yet");
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    const bool submitted = ImGui::InputText("##presetName", m_presetNameInput,
                                            sizeof(m_presetNameInput),
                                            ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();

    const bool nameEmpty = m_presetNameInput[0] == '\0';

    if (m_renaming) {
        ImGui::BeginDisabled(nameEmpty);
        if ((ImGui::Button("Confirm rename") || submitted) && selected != nullptr &&
            preset.rename) {
            preset.rename(*selected, m_presetNameInput);
            m_presetsDirty = true;
            m_renaming = false;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_renaming = false;
        }
    } else {
        ImGui::BeginDisabled(nameEmpty);
        if ((ImGui::Button("Save As") || submitted) && preset.saveAs) {
            preset.saveAs(m_presetNameInput);
            m_presetsDirty = true;
        }
        ImGui::EndDisabled();
    }

    if (m_confirmingDelete && selected != nullptr) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.40f, 0.40f, 1.0f), "Delete '%s'?",
                           selected->name.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Yes, delete")) {
            if (preset.remove) {
                preset.remove(*selected);
            }
            m_presetsDirty = true;
            m_confirmingDelete = false;
            m_selectedPreset = -1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Keep it")) {
            m_confirmingDelete = false;
        }
        if (selected->builtIn) {
            widgets::HelpText(
                "This is one of the presets shipped with the application. Deleting it is "
                "permanent - it is not recreated on the next launch.");
        }
    }

    ImGui::Spacing();
    widgets::HelpText(
        "A preset stores filters and effects only. Window geometry, the selected target and "
        "FPS settings are never part of it, so the same preset works on any machine.");
}

}  // namespace overlaydesk::ui
