#include "core/Settings.h"

#include "core/SettingsJson.h"

namespace overlaydesk {
namespace {

using json_io::Child;
using json_io::json;
using json_io::Num;
using json_io::Read;

void ReadOverlay(const json& root, OverlaySettings& out) {
    const json* node = Child(root, "overlay");
    if (node == nullptr) {
        return;
    }
    Read(*node, "enabled", out.enabled);
    Read(*node, "x", out.x);
    Read(*node, "y", out.y);
    Read(*node, "width", out.width);
    Read(*node, "height", out.height);
    Read(*node, "resizable", out.resizable);
    Read(*node, "lockPosition", out.lockPosition);
    Read(*node, "lockSize", out.lockSize);
    Read(*node, "lockAspectRatio", out.lockAspectRatio);
    Read(*node, "aspectRatio", out.aspectRatio);
    Read(*node, "alwaysOnTop", out.alwaysOnTop);
    Read(*node, "clickThrough", out.clickThrough);
    Read(*node, "opacity", out.opacity);
    Read(*node, "fullscreen", out.fullscreen);
    Read(*node, "monitorIndex", out.monitorIndex);

    out.width = out.width > 16 ? out.width : 16;
    out.height = out.height > 16 ? out.height : 16;
    out.opacity = ClampIntensity(out.opacity);
    out.monitorIndex = out.monitorIndex >= 0 ? out.monitorIndex : 0;
}

void ReadRender(const json& root, RenderSettings& out) {
    const json* node = Child(root, "render");
    if (node == nullptr) {
        return;
    }
    out.fpsMode = json_io::ParseFpsMode(json_io::ReadEnumName(*node, "fpsMode", "matchSource"));
    Read(*node, "fpsCap", out.fpsCap);
    Read(*node, "pauseWhenSourceMinimized", out.pauseWhenSourceMinimized);
    out.fpsCap = out.fpsCap > 0 ? out.fpsCap : 60;
}

void ReadUi(const json& root, UiSettings& out) {
    const json* node = Child(root, "ui");
    if (node == nullptr) {
        return;
    }
    Read(*node, "logLevel", out.logLevel);
    Read(*node, "activeTab", out.activeTab);
    Read(*node, "controlWindowWidth", out.controlWindowWidth);
    Read(*node, "controlWindowHeight", out.controlWindowHeight);
    Read(*node, "showAdvanced", out.showAdvanced);
    Read(*node, "restoreLastTarget", out.restoreLastTarget);
    Read(*node, "lastTargetTitle", out.lastTargetTitle);
    Read(*node, "lastTargetExecutable", out.lastTargetExecutable);
    Read(*node, "activePreset", out.activePreset);
    Read(*node, "seededPresets", out.seededPresets);
}

// Shortcuts are stored by name - "ctrl", "shift", "O" - rather than as the numbers Windows
// uses. ADR-0004 chose JSON so the file could be read and corrected by hand, and a virtual-key
// code in hexadecimal would defeat that for the one section a user is most likely to edit.
void ReadHotkeys(const json& root, HotkeySettings& out) {
    const json* node = Child(root, "hotkeys");
    if (node == nullptr) {
        return;
    }
    Read(*node, "enabled", out.enabled);

    const json* bindings = Child(*node, "bindings");
    if (bindings == nullptr || !bindings->is_object()) {
        return;
    }

    for (int i = 0; i < kHotkeyActionCount; ++i) {
        const HotkeyAction action = static_cast<HotkeyAction>(i);
        const json* entry = Child(*bindings, HotkeyActionKey(action));
        if (entry == nullptr || !entry->is_object()) {
            continue;
        }

        // An entry that is present but malformed clears the binding rather than falling back
        // to the default: the user wrote something there on purpose, and silently restoring a
        // shortcut they were trying to remove is worse than leaving it unbound.
        HotkeyBinding binding;

        std::string keyName;
        Read(*entry, "key", keyName);
        binding.key = HotkeyKeyFromName(keyName.c_str());

        std::vector<std::string> modifiers;
        Read(*entry, "modifiers", modifiers);
        for (const std::string& name : modifiers) {
            if (name == "ctrl" || name == "control") {
                binding.modifiers |= HotkeyModControl;
            } else if (name == "shift") {
                binding.modifiers |= HotkeyModShift;
            } else if (name == "alt") {
                binding.modifiers |= HotkeyModAlt;
            } else if (name == "win") {
                binding.modifiers |= HotkeyModWin;
            }
        }

        out.bindings[i] = binding;
    }
}

json SerializeHotkeys(const HotkeySettings& s) {
    json hotkeys;
    hotkeys["enabled"] = s.enabled;

    json& bindings = hotkeys["bindings"];
    for (int i = 0; i < kHotkeyActionCount; ++i) {
        const HotkeyAction action = static_cast<HotkeyAction>(i);
        const HotkeyBinding& binding = s.bindings[i];

        json entry;
        json modifiers = json::array();
        if ((binding.modifiers & HotkeyModControl) != 0) {
            modifiers.push_back("ctrl");
        }
        if ((binding.modifiers & HotkeyModShift) != 0) {
            modifiers.push_back("shift");
        }
        if ((binding.modifiers & HotkeyModAlt) != 0) {
            modifiers.push_back("alt");
        }
        if ((binding.modifiers & HotkeyModWin) != 0) {
            modifiers.push_back("win");
        }
        entry["modifiers"] = modifiers;
        entry["key"] = HotkeyKeyName(binding.key);

        bindings[HotkeyActionKey(action)] = entry;
    }

    return hotkeys;
}

json Serialize(const AppSettings& s) {
    json root;
    root["schemaVersion"] = s.schemaVersion;

    json& overlay = root["overlay"];
    overlay["enabled"] = s.overlay.enabled;
    overlay["x"] = s.overlay.x;
    overlay["y"] = s.overlay.y;
    overlay["width"] = s.overlay.width;
    overlay["height"] = s.overlay.height;
    overlay["resizable"] = s.overlay.resizable;
    overlay["lockPosition"] = s.overlay.lockPosition;
    overlay["lockSize"] = s.overlay.lockSize;
    overlay["lockAspectRatio"] = s.overlay.lockAspectRatio;
    overlay["aspectRatio"] = s.overlay.aspectRatio;
    overlay["alwaysOnTop"] = s.overlay.alwaysOnTop;
    overlay["clickThrough"] = s.overlay.clickThrough;
    overlay["opacity"] = Num(s.overlay.opacity);
    overlay["fullscreen"] = s.overlay.fullscreen;
    overlay["monitorIndex"] = s.overlay.monitorIndex;

    root["filters"] = json_io::SerializeFilters(s.filters);
    root["effects"] = json_io::SerializeEffects(s.effects);

    json& render = root["render"];
    render["fpsMode"] = ToString(s.render.fpsMode);
    render["fpsCap"] = s.render.fpsCap;
    render["pauseWhenSourceMinimized"] = s.render.pauseWhenSourceMinimized;

    json& ui = root["ui"];
    ui["logLevel"] = s.ui.logLevel;
    ui["activeTab"] = s.ui.activeTab;
    ui["controlWindowWidth"] = s.ui.controlWindowWidth;
    ui["controlWindowHeight"] = s.ui.controlWindowHeight;
    ui["showAdvanced"] = s.ui.showAdvanced;
    ui["restoreLastTarget"] = s.ui.restoreLastTarget;
    ui["lastTargetTitle"] = s.ui.lastTargetTitle;
    ui["lastTargetExecutable"] = s.ui.lastTargetExecutable;
    ui["activePreset"] = s.ui.activePreset;
    ui["seededPresets"] = s.ui.seededPresets;

    root["hotkeys"] = SerializeHotkeys(s.hotkeys);

    return root;
}

}  // namespace

const char* ToString(FpsMode mode) noexcept {
    switch (mode) {
        case FpsMode::MatchSource: return "matchSource";
        case FpsMode::Cap60: return "cap60";
        case FpsMode::Cap30: return "cap30";
        case FpsMode::Unlimited: return "unlimited";
    }
    return "matchSource";
}

const char* ToString(DistortionShape shape) noexcept {
    switch (shape) {
        case DistortionShape::Radial: return "radial";
        case DistortionShape::Crt: return "crt";
        case DistortionShape::Cylindrical: return "cylindrical";
        case DistortionShape::Vertical: return "vertical";
        case DistortionShape::Corner: return "corner";
    }
    return "radial";
}

const char* ToString(ScanlineStyle style) noexcept {
    switch (style) {
        case ScanlineStyle::Hard: return "hard";
        case ScanlineStyle::Soft: return "soft";
        case ScanlineStyle::Sharp: return "sharp";
        case ScanlineStyle::ApertureGrille: return "apertureGrille";
        case ScanlineStyle::SlotMask: return "slotMask";
    }
    return "hard";
}

const char* ToString(ScanlineOrientation orientation) noexcept {
    switch (orientation) {
        case ScanlineOrientation::Horizontal: return "horizontal";
        case ScanlineOrientation::Vertical: return "vertical";
        case ScanlineOrientation::Grid: return "grid";
    }
    return "horizontal";
}

const char* ToString(ScanlineScaleMode mode) noexcept {
    switch (mode) {
        case ScanlineScaleMode::Relative: return "relative";
        case ScanlineScaleMode::PixelPerfect: return "pixelPerfect";
    }
    return "pixelPerfect";
}

const char* ToString(ChromaticAberrationMode mode) noexcept {
    switch (mode) {
        case ChromaticAberrationMode::Radial: return "radial";
        case ChromaticAberrationMode::Horizontal: return "horizontal";
        case ChromaticAberrationMode::Vertical: return "vertical";
        case ChromaticAberrationMode::Edge: return "edge";
        case ChromaticAberrationMode::Prism: return "prism";
        case ChromaticAberrationMode::Barrel: return "barrel";
    }
    return "radial";
}

const char* ToString(ScopeShape shape) noexcept {
    switch (shape) {
        case ScopeShape::Binocular:
            return "binocular";
        case ScopeShape::QuadTube:
            return "quadTube";
        case ScopeShape::Tube:
            return "tube";
        case ScopeShape::Circle:
        default:
            return "circle";
    }
}

const char* ToString(FalseColourPalette palette) noexcept {
    switch (palette) {
        case FalseColourPalette::BlackHot: return "blackHot";
        case FalseColourPalette::Ironbow: return "ironbow";
        case FalseColourPalette::Phosphor: return "phosphor";
        case FalseColourPalette::WhitePhosphor: return "whitePhosphor";
        case FalseColourPalette::CrossCom: return "crossCom";
        case FalseColourPalette::WhiteHot:
        default:
            return "whiteHot";
    }
}

const char* ToString(GlitchStyle style) noexcept {
    switch (style) {
        case GlitchStyle::Digital:
            return "digital";
        case GlitchStyle::Analog:
        default:
            return "analog";
    }
}

double FrameIntervalSeconds(const RenderSettings& render) noexcept {
    switch (render.fpsMode) {
        case FpsMode::Unlimited:
            return 0.0;
        case FpsMode::Cap30:
            return 1.0 / 30.0;
        case FpsMode::Cap60:
            return 1.0 / 60.0;
        case FpsMode::MatchSource:
            // RNF-003: match source, but with an operational cap so a 240 Hz source cannot
            // drag the overlay's GPU cost along with it.
            break;
    }
    return render.fpsCap > 0 ? 1.0 / static_cast<double>(render.fpsCap) : 0.0;
}

SettingsRepository::SettingsRepository(std::filesystem::path appDataDirectory)
    : m_directory(std::move(appDataDirectory)) {
    if (!m_directory.empty()) {
        m_filePath = m_directory / L"settings.json";
    }
}

AppSettings SettingsRepository::Load() const {
    AppSettings settings;

    if (m_filePath.empty()) {
        LogWarn("Settings: no application data directory; running with defaults.");
        return settings;
    }

    std::error_code ec;
    if (!std::filesystem::exists(m_filePath, ec) || ec) {
        LogInfo("Settings: {} not present; writing defaults on first save.",
                Utf8FromWide(m_filePath.wstring()));
        return settings;
    }

    // AT-018: a corrupt settings file must not stop startup.
    const json root = json_io::ParseJsonFile(m_filePath);
    if (root.is_discarded() || !root.is_object()) {
        LogError("Settings: {} is not valid JSON; using defaults.",
                 Utf8FromWide(m_filePath.wstring()));
        return settings;
    }

    uint32_t schemaVersion = kSettingsSchemaVersion;
    if (const json* node = Child(root, "schemaVersion");
        node != nullptr && node->is_number_unsigned()) {
        schemaVersion = node->get<uint32_t>();
    }

    // CONFIGURATION.md: never ignore the version silently. There is only one schema so far,
    // so anything newer is refused rather than half-read.
    if (schemaVersion > kSettingsSchemaVersion) {
        LogWarn("Settings: schemaVersion {} is newer than supported {}; using defaults.",
                schemaVersion, kSettingsSchemaVersion);
        return settings;
    }
    settings.schemaVersion = kSettingsSchemaVersion;

    ReadOverlay(root, settings.overlay);
    if (const json* filters = Child(root, "filters"); filters != nullptr) {
        json_io::ReadFilters(*filters, settings.filters);
    }
    if (const json* effects = Child(root, "effects"); effects != nullptr) {
        json_io::ReadEffects(*effects, settings.effects);
    }
    ReadRender(root, settings.render);
    ReadUi(root, settings.ui);
    ReadHotkeys(root, settings.hotkeys);

    LogInfo("Settings: loaded {} (schemaVersion {}).", Utf8FromWide(m_filePath.wstring()),
            schemaVersion);
    return settings;
}

bool SettingsRepository::Save(const AppSettings& settings) const {
    if (m_filePath.empty()) {
        return false;
    }
    return json_io::WriteJsonAtomically(m_filePath, Serialize(settings));
}

}  // namespace overlaydesk
