// The "Hotkeys" tab: one row per action, each showing the chord it is bound to and whether
// Windows accepted it.
//
// Rebinding works by listening rather than by dropdowns. A dropdown for the key plus three
// checkboxes for the modifiers would be less code, but it asks the user to know that the key
// they want is called "OEM_3" - pressing the key is the only interface that does not.
//
// A chord that another application registered first is not an error and is not blocked here:
// the user may well be about to close that application. The row says the shortcut is
// unavailable and everything it does stays reachable from the other tabs.

#include <imgui.h>

#include <Windows.h>

#include "ui/ControlPanel.h"
#include "util/Log.h"

namespace overlaydesk::ui {
namespace {

// ImGui reports keys in its own enumeration, and the Win32 backend only translates one way.
// This is the reverse for the keys HotkeyKeyName is willing to name; anything absent here
// simply cannot be captured, which is the same set the rest of the application accepts.
struct KeyMapping {
    ImGuiKey imguiKey;
    uint32_t virtualKey;
};

constexpr KeyMapping kKeyMappings[] = {
    {ImGuiKey_A, 'A'},          {ImGuiKey_B, 'B'},         {ImGuiKey_C, 'C'},
    {ImGuiKey_D, 'D'},          {ImGuiKey_E, 'E'},         {ImGuiKey_F, 'F'},
    {ImGuiKey_G, 'G'},          {ImGuiKey_H, 'H'},         {ImGuiKey_I, 'I'},
    {ImGuiKey_J, 'J'},          {ImGuiKey_K, 'K'},         {ImGuiKey_L, 'L'},
    {ImGuiKey_M, 'M'},          {ImGuiKey_N, 'N'},         {ImGuiKey_O, 'O'},
    {ImGuiKey_P, 'P'},          {ImGuiKey_Q, 'Q'},         {ImGuiKey_R, 'R'},
    {ImGuiKey_S, 'S'},          {ImGuiKey_T, 'T'},         {ImGuiKey_U, 'U'},
    {ImGuiKey_V, 'V'},          {ImGuiKey_W, 'W'},         {ImGuiKey_X, 'X'},
    {ImGuiKey_Y, 'Y'},          {ImGuiKey_Z, 'Z'},

    {ImGuiKey_0, '0'},          {ImGuiKey_1, '1'},         {ImGuiKey_2, '2'},
    {ImGuiKey_3, '3'},          {ImGuiKey_4, '4'},         {ImGuiKey_5, '5'},
    {ImGuiKey_6, '6'},          {ImGuiKey_7, '7'},         {ImGuiKey_8, '8'},
    {ImGuiKey_9, '9'},

    {ImGuiKey_F1, VK_F1},       {ImGuiKey_F2, VK_F2},      {ImGuiKey_F3, VK_F3},
    {ImGuiKey_F4, VK_F4},       {ImGuiKey_F5, VK_F5},      {ImGuiKey_F6, VK_F6},
    {ImGuiKey_F7, VK_F7},       {ImGuiKey_F8, VK_F8},      {ImGuiKey_F9, VK_F9},
    {ImGuiKey_F10, VK_F10},     {ImGuiKey_F11, VK_F11},    {ImGuiKey_F12, VK_F12},

    {ImGuiKey_LeftArrow, VK_LEFT},
    {ImGuiKey_RightArrow, VK_RIGHT},
    {ImGuiKey_UpArrow, VK_UP},
    {ImGuiKey_DownArrow, VK_DOWN},
    {ImGuiKey_Home, VK_HOME},   {ImGuiKey_End, VK_END},
    {ImGuiKey_PageUp, VK_PRIOR},
    {ImGuiKey_PageDown, VK_NEXT},
    {ImGuiKey_Insert, VK_INSERT},
    {ImGuiKey_Delete, VK_DELETE},
    {ImGuiKey_Space, VK_SPACE}, {ImGuiKey_Enter, VK_RETURN},
    {ImGuiKey_Backspace, VK_BACK},

    {ImGuiKey_GraveAccent, VK_OEM_3},
    {ImGuiKey_Minus, VK_OEM_MINUS},
    {ImGuiKey_Equal, VK_OEM_PLUS},
    {ImGuiKey_LeftBracket, VK_OEM_4},
    {ImGuiKey_RightBracket, VK_OEM_6},
    {ImGuiKey_Semicolon, VK_OEM_1},
    {ImGuiKey_Apostrophe, VK_OEM_7},
    {ImGuiKey_Comma, VK_OEM_COMMA},
    {ImGuiKey_Period, VK_OEM_PERIOD},
    {ImGuiKey_Slash, VK_OEM_2},
    {ImGuiKey_Backslash, VK_OEM_5},
};

uint32_t CurrentModifiers() {
    const ImGuiIO& io = ImGui::GetIO();
    uint32_t modifiers = HotkeyModNone;
    if (io.KeyCtrl) {
        modifiers |= HotkeyModControl;
    }
    if (io.KeyShift) {
        modifiers |= HotkeyModShift;
    }
    if (io.KeyAlt) {
        modifiers |= HotkeyModAlt;
    }
    if (io.KeySuper) {
        modifiers |= HotkeyModWin;
    }
    return modifiers;
}

// The first bindable key pressed this frame, or 0. Modifiers are excluded by construction:
// they are not in the mapping table.
uint32_t CapturedKey() {
    uint32_t result = 0;

    for (const KeyMapping& mapping : kKeyMappings) {
        if (ImGui::IsKeyPressed(mapping.imguiKey, /*repeat=*/false)) {
            result = mapping.virtualKey;
            break;
        }
    }

    return result;
}

// The other action already using this chord, or Count. Two actions on one chord is not fatal -
// Windows gives the keystroke to whichever registered first - but it is never what anyone
// meant, so the page says so.
HotkeyAction FindClash(const HotkeySettings& settings, HotkeyAction self,
                       const HotkeyBinding& binding) {
    HotkeyAction result = HotkeyAction::Count;

    if (binding.Bound()) {
        for (int i = 0; i < kHotkeyActionCount; ++i) {
            const HotkeyAction other = static_cast<HotkeyAction>(i);
            if (other == self) {
                continue;
            }
            if (SameChord(settings.bindings[i], binding)) {
                result = other;
                break;
            }
        }
    }

    return result;
}

}  // namespace

void ControlPanel::DrawHotkeysPage(AppState& state, const PanelActions& actions) {
    HotkeySettings& hotkeys = state.settings.hotkeys;
    bool changed = false;

    ImGui::Spacing();

    if (ImGui::Checkbox("Global shortcuts enabled", &hotkeys.enabled)) {
        changed = true;
        // Turning the master switch off has to release the registrations immediately, or the
        // chords stay taken while the checkbox claims otherwise.
        m_capturingHotkey = HotkeyAction::Count;
    }
    widgets::HelpText(
        "These work while another application has focus, which is the whole point of them. "
        "Turning this off releases every chord without forgetting what it was bound to.");

    ImGui::SameLine();
    if (ImGui::SmallButton("Restore defaults")) {
        for (int i = 0; i < kHotkeyActionCount; ++i) {
            hotkeys.bindings[i] = DefaultHotkeyBinding(static_cast<HotkeyAction>(i));
        }
        m_capturingHotkey = HotkeyAction::Count;
        changed = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!ImGui::BeginChild("##hotkeys", ImVec2(0, 0))) {
        ImGui::EndChild();
        return;
    }

    ImGui::BeginDisabled(!hotkeys.enabled);

    for (int i = 0; i < kHotkeyActionCount; ++i) {
        const HotkeyAction action = static_cast<HotkeyAction>(i);
        HotkeyBinding& binding = hotkeys.bindings[i];
        const bool capturing = m_capturingHotkey == action;

        ImGui::PushID(i);

        ImGui::TextUnformatted(HotkeyActionLabel(action));
        widgets::HelpText(HotkeyActionHelp(action));

        const std::string chord = DescribeHotkey(binding);
        const char* buttonLabel = capturing               ? "Press a key..."
                                  : chord.empty()         ? "Not bound"
                                                          : chord.c_str();

        if (ImGui::Button(buttonLabel, ImVec2(200.0f, 0.0f))) {
            m_capturingHotkey = capturing ? HotkeyAction::Count : action;
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) {
            binding = HotkeyBinding{};
            m_capturingHotkey = HotkeyAction::Count;
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Default")) {
            binding = DefaultHotkeyBinding(action);
            m_capturingHotkey = HotkeyAction::Count;
            changed = true;
        }

        if (capturing) {
            ImGui::TextDisabled("Escape cancels. A modifier on its own is not a shortcut.");

            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                m_capturingHotkey = HotkeyAction::Count;
            } else if (const uint32_t key = CapturedKey(); key != 0) {
                const uint32_t modifiers = CurrentModifiers();
                // Windows will happily register a bare letter as a global hotkey, at which
                // point that letter stops reaching every other application on the desktop.
                // Requiring a modifier is what keeps a rebind from breaking the user's system.
                if (modifiers == HotkeyModNone) {
                    LogWarn("Hotkeys: '{}' needs at least one modifier.", HotkeyKeyName(key));
                } else {
                    binding.key = key;
                    binding.modifiers = modifiers;
                    m_capturingHotkey = HotkeyAction::Count;
                    changed = true;
                }
            }
        } else {
            if (actions.hotkeyUnavailable && binding.Bound() && hotkeys.enabled &&
                actions.hotkeyUnavailable(action)) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.95f, 0.62f, 0.20f, 1.0f), "unavailable");
                widgets::HelpText(
                    "Another application registered this chord first. Pick a different one, or "
                    "close whatever is holding it.");
            }

            if (const HotkeyAction clash = FindClash(hotkeys, action, binding);
                clash != HotkeyAction::Count) {
                ImGui::TextColored(ImVec4(0.95f, 0.62f, 0.20f, 1.0f), "Also bound to \"%s\"",
                                   HotkeyActionLabel(clash));
            }
        }

        ImGui::PopID();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    ImGui::EndDisabled();
    ImGui::EndChild();

    if (changed) {
        state.settingsDirty = true;
        if (actions.applyHotkeys) {
            actions.applyHotkeys();
        }
    }
}

}  // namespace overlaydesk::ui
