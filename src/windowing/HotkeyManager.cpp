#include "windowing/HotkeyManager.h"

#include "util/Log.h"

namespace overlaydesk {
namespace {

// core/Hotkeys.h declares its modifier bits to be the Win32 values so they can be passed
// through untranslated. This is where that claim is checked.
static_assert(HotkeyModAlt == MOD_ALT, "HotkeyModAlt must match MOD_ALT");
static_assert(HotkeyModControl == MOD_CONTROL, "HotkeyModControl must match MOD_CONTROL");
static_assert(HotkeyModShift == MOD_SHIFT, "HotkeyModShift must match MOD_SHIFT");
static_assert(HotkeyModWin == MOD_WIN, "HotkeyModWin must match MOD_WIN");

}  // namespace

int HotkeyManager::IdFor(HotkeyAction action) noexcept {
    // RegisterHotKey ids must be in 0x0000..0xBFFF for an application, and 0 is a legal id we
    // would rather not use - offsetting by one keeps 0 meaning "not one of ours".
    return static_cast<int>(action) + 1;
}

void HotkeyManager::Apply(HWND window, const HotkeySettings& settings) {
    Clear();
    m_window = window;

    if (window == nullptr || !settings.enabled) {
        return;
    }

    for (int i = 0; i < kHotkeyActionCount; ++i) {
        const HotkeyAction action = static_cast<HotkeyAction>(i);
        const HotkeyBinding& binding = settings.bindings[i];
        if (!binding.Bound()) {
            continue;
        }

        // MOD_NOREPEAT: every one of these actions is a toggle, and auto-repeat would flip it
        // back and forth for as long as the key is held.
        const UINT modifiers = static_cast<UINT>(binding.modifiers) | MOD_NOREPEAT;
        if (::RegisterHotKey(window, IdFor(action), modifiers, static_cast<UINT>(binding.key)) !=
            0) {
            m_registered[i] = true;
        } else {
            m_unavailable[i] = true;
            LogWarn("Hotkeys: {} is unavailable for '{}' - another application holds it.",
                    DescribeHotkey(binding), HotkeyActionLabel(action));
        }
    }
}

void HotkeyManager::Clear() noexcept {
    if (m_window != nullptr) {
        for (int i = 0; i < kHotkeyActionCount; ++i) {
            if (m_registered[i]) {
                ::UnregisterHotKey(m_window, IdFor(static_cast<HotkeyAction>(i)));
            }
        }
    }

    for (int i = 0; i < kHotkeyActionCount; ++i) {
        m_registered[i] = false;
        m_unavailable[i] = false;
    }
    m_window = nullptr;
}

HotkeyAction HotkeyManager::ActionForId(int id) const noexcept {
    HotkeyAction result = HotkeyAction::Count;

    const int index = id - 1;
    if (index >= 0 && index < kHotkeyActionCount && m_registered[index]) {
        result = static_cast<HotkeyAction>(index);
    }

    return result;
}

bool HotkeyManager::Registered(HotkeyAction action) const noexcept {
    const int index = static_cast<int>(action);
    return index >= 0 && index < kHotkeyActionCount && m_registered[index];
}

bool HotkeyManager::Unavailable(HotkeyAction action) const noexcept {
    const int index = static_cast<int>(action);
    return index >= 0 && index < kHotkeyActionCount && m_unavailable[index];
}

}  // namespace overlaydesk
