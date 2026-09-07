#include "core/Hotkeys.h"

#include <Windows.h>

#include <cstring>

namespace overlaydesk {
namespace {

struct ActionInfo {
    const char* key;    // stable, persisted
    const char* label;  // shown in the UI
    const char* help;
    uint32_t modifiers;
    uint32_t virtualKey;
};

// Defaults. Ctrl+Shift is used throughout because it is the combination games and emulators
// are least likely to have claimed, and because OVERLAY-WINDOW.md already specified
// Ctrl+Shift+O for edit mode.
//
// Edit mode gets two entries rather than one: the letter O and the digit 0 are
// indistinguishable in writing and easy to confuse at the keyboard, and both have worked
// since the first release. Modelling the second one as its own action is what makes it
// rebindable instead of a hidden special case.
constexpr ActionInfo kActions[kHotkeyActionCount] = {
    {"toggleEditMode", "Edit Mode", "Makes the overlay draggable and resizable, and back.",
     HotkeyModControl | HotkeyModShift, 'O'},
    {"toggleEditModeAlt", "Edit Mode (alternate)",
     "A second chord for the same action. The letter O and the digit 0 are easy to confuse, so "
     "both are bound out of the box.",
     HotkeyModControl | HotkeyModShift, '0'},
    {"toggleOverlay", "Show / hide overlay", "Turns the overlay off without losing the target.",
     HotkeyModControl | HotkeyModShift, 'H'},
    {"toggleClickThrough", "Click-through",
     "Switches between clicks passing through to the game and the overlay taking them.",
     HotkeyModControl | HotkeyModShift, 'C'},
    {"toggleAlwaysOnTop", "Always on top", "Keeps the overlay above other windows, or lets it go.",
     HotkeyModControl | HotkeyModShift, 'T'},
    {"toggleFullscreen", "Fullscreen", "Fills the selected monitor, or returns to the last size.",
     HotkeyModControl | HotkeyModShift, 'F'},
    {"matchTargetWindow", "Match target window",
     "Snaps the overlay to the position and size of the captured window.",
     HotkeyModControl | HotkeyModShift, 'M'},
    {"nextPreset", "Next preset", "Applies the next preset in the list.",
     HotkeyModControl | HotkeyModShift, VK_RIGHT},
    {"previousPreset", "Previous preset", "Applies the previous preset in the list.",
     HotkeyModControl | HotkeyModShift, VK_LEFT},
    {"showControlPanel", "Show control panel", "Brings this window back to the front.",
     HotkeyModControl | HotkeyModShift, 'P'},
    {"showQuickMenu", "Quick menu",
     "Opens the overlay's quick menu. Escape does the same, but only reaches the overlay when "
     "it has focus - in Play Mode the game does, so this chord is the way in.",
     HotkeyModControl | HotkeyModShift, 'Q'},
};

constexpr const char* kLetterNames[] = {"A", "B", "C", "D", "E", "F", "G", "H", "I",
                                        "J", "K", "L", "M", "N", "O", "P", "Q", "R",
                                        "S", "T", "U", "V", "W", "X", "Y", "Z"};

constexpr const char* kDigitNames[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

constexpr const char* kFunctionNames[] = {"F1",  "F2",  "F3",  "F4",  "F5",  "F6",  "F7",  "F8",
                                          "F9",  "F10", "F11", "F12", "F13", "F14", "F15", "F16",
                                          "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24"};

bool InRange(HotkeyAction action) noexcept {
    const int index = static_cast<int>(action);
    return index >= 0 && index < kHotkeyActionCount;
}

}  // namespace

HotkeySettings::HotkeySettings() {
    for (int i = 0; i < kHotkeyActionCount; ++i) {
        bindings[i] = DefaultHotkeyBinding(static_cast<HotkeyAction>(i));
    }
}

const char* HotkeyActionKey(HotkeyAction action) noexcept {
    return InRange(action) ? kActions[static_cast<int>(action)].key : "";
}

const char* HotkeyActionLabel(HotkeyAction action) noexcept {
    return InRange(action) ? kActions[static_cast<int>(action)].label : "";
}

const char* HotkeyActionHelp(HotkeyAction action) noexcept {
    return InRange(action) ? kActions[static_cast<int>(action)].help : "";
}

HotkeyAction HotkeyActionFromKey(const char* key) noexcept {
    HotkeyAction result = HotkeyAction::Count;

    if (key != nullptr) {
        for (int i = 0; i < kHotkeyActionCount; ++i) {
            if (std::strcmp(kActions[i].key, key) == 0) {
                result = static_cast<HotkeyAction>(i);
                break;
            }
        }
    }

    return result;
}

HotkeyBinding DefaultHotkeyBinding(HotkeyAction action) noexcept {
    HotkeyBinding result;

    if (InRange(action)) {
        const ActionInfo& info = kActions[static_cast<int>(action)];
        result.modifiers = info.modifiers;
        result.key = info.virtualKey;
    }

    return result;
}

const char* HotkeyKeyName(uint32_t virtualKey) noexcept {
    const char* name = "";

    if (virtualKey >= 'A' && virtualKey <= 'Z') {
        name = kLetterNames[virtualKey - 'A'];
    } else if (virtualKey >= '0' && virtualKey <= '9') {
        name = kDigitNames[virtualKey - '0'];
    } else if (virtualKey >= VK_F1 && virtualKey <= VK_F24) {
        name = kFunctionNames[virtualKey - VK_F1];
    } else {
        switch (virtualKey) {
            case VK_LEFT: name = "Left"; break;
            case VK_RIGHT: name = "Right"; break;
            case VK_UP: name = "Up"; break;
            case VK_DOWN: name = "Down"; break;
            case VK_HOME: name = "Home"; break;
            case VK_END: name = "End"; break;
            case VK_PRIOR: name = "Page Up"; break;
            case VK_NEXT: name = "Page Down"; break;
            case VK_INSERT: name = "Insert"; break;
            case VK_DELETE: name = "Delete"; break;
            case VK_SPACE: name = "Space"; break;
            case VK_RETURN: name = "Enter"; break;
            case VK_BACK: name = "Backspace"; break;
            case VK_OEM_3: name = "`"; break;
            case VK_OEM_MINUS: name = "-"; break;
            case VK_OEM_PLUS: name = "="; break;
            case VK_OEM_4: name = "["; break;
            case VK_OEM_6: name = "]"; break;
            case VK_OEM_1: name = ";"; break;
            case VK_OEM_7: name = "'"; break;
            case VK_OEM_COMMA: name = ","; break;
            case VK_OEM_PERIOD: name = "."; break;
            case VK_OEM_2: name = "/"; break;
            case VK_OEM_5: name = "\\"; break;
            default: break;
        }
    }

    return name;
}

uint32_t HotkeyKeyFromName(const char* name) noexcept {
    uint32_t result = 0;

    if (name != nullptr && name[0] != '\0') {
        // 0xFF is the top of the virtual-key range; the scan is only ever run when loading a
        // settings file, never per frame.
        for (uint32_t vk = 1; vk <= 0xFF; ++vk) {
            const char* candidate = HotkeyKeyName(vk);
            if (candidate[0] != '\0' && std::strcmp(candidate, name) == 0) {
                result = vk;
                break;
            }
        }
    }

    return result;
}

std::string DescribeHotkey(const HotkeyBinding& binding) {
    std::string result;

    const char* keyName = HotkeyKeyName(binding.key);
    if (binding.Bound() && keyName[0] != '\0') {
        // Windows shows modifiers in this order in its own UI, so shortcuts read the way a
        // user expects to see them written down.
        if ((binding.modifiers & HotkeyModControl) != 0) {
            result += "Ctrl+";
        }
        if ((binding.modifiers & HotkeyModShift) != 0) {
            result += "Shift+";
        }
        if ((binding.modifiers & HotkeyModAlt) != 0) {
            result += "Alt+";
        }
        if ((binding.modifiers & HotkeyModWin) != 0) {
            result += "Win+";
        }
        result += keyName;
    }

    return result;
}

}  // namespace overlaydesk
