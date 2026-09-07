#pragma once

// Global shortcut model. The bindings are persisted, so the types live in core alongside the
// rest of the settings; registering them with the OS is windowing/HotkeyManager's job, and
// this header deliberately pulls in no Windows headers so the tests can link against it.

#include <cstdint>
#include <string>

namespace overlaydesk {

// Modifier bits. The values match the Win32 MOD_* constants exactly so the registrar can pass
// them straight through - HotkeyManager static_asserts that rather than trusting this comment.
enum HotkeyModifier : uint32_t {
    HotkeyModNone = 0,
    HotkeyModAlt = 1u << 0,
    HotkeyModControl = 1u << 1,
    HotkeyModShift = 1u << 2,
    HotkeyModWin = 1u << 3,
};

// Every action a global shortcut can trigger.
//
// The enum value is an index into the bindings array and into the id space RegisterHotKey
// sees, so appending is safe and reordering is not. What ties a stored binding to its action
// on disk is the string from HotkeyActionKey, never this number.
enum class HotkeyAction : int {
    ToggleEditMode = 0,
    ToggleEditModeAlt,
    ToggleOverlay,
    ToggleClickThrough,
    ToggleAlwaysOnTop,
    ToggleFullscreen,
    MatchTargetWindow,
    NextPreset,
    PreviousPreset,
    ShowControlPanel,
    ShowQuickMenu,
    Count,
};

inline constexpr int kHotkeyActionCount = static_cast<int>(HotkeyAction::Count);

struct HotkeyBinding {
    uint32_t modifiers = HotkeyModNone;
    uint32_t key = 0;  // Windows virtual-key code; 0 means unbound

    bool Bound() const noexcept { return key != 0; }
};

inline bool SameChord(const HotkeyBinding& a, const HotkeyBinding& b) noexcept {
    return a.key == b.key && a.modifiers == b.modifiers;
}

struct HotkeySettings {
    // Master switch. Turning it off unregisters everything without discarding a binding, which
    // is what a user who wants their shortcuts back later actually needs.
    bool enabled = true;
    HotkeyBinding bindings[kHotkeyActionCount];

    HotkeySettings();
};

// Stable JSON key. Changing one of these orphans every binding already on disk.
const char* HotkeyActionKey(HotkeyAction action) noexcept;
const char* HotkeyActionLabel(HotkeyAction action) noexcept;
const char* HotkeyActionHelp(HotkeyAction action) noexcept;
HotkeyAction HotkeyActionFromKey(const char* key) noexcept;

HotkeyBinding DefaultHotkeyBinding(HotkeyAction action) noexcept;

// "Ctrl+Shift+O", or an empty string when the binding is unset.
std::string DescribeHotkey(const HotkeyBinding& binding);

// The display name of a virtual key, or an empty string when it is not one this application
// is willing to bind. That makes this function the single definition of the bindable set.
const char* HotkeyKeyName(uint32_t virtualKey) noexcept;

// The inverse, for reading a hand-edited settings file. Returns 0 for a name no bindable key
// carries. Implemented by searching HotkeyKeyName rather than from a second table, so the two
// directions cannot drift apart.
uint32_t HotkeyKeyFromName(const char* name) noexcept;

}  // namespace overlaydesk
