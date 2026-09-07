#pragma once

// RF-017 / ADR-0004: presets live as individual JSON files under
// %APPDATA%\OverlayDesk\presets\, separate from settings.json.
//
// CONFIGURATION.md is explicit about what a preset may and may not hold: visual state only.
// No HWND, no target position, no PID - a preset describes a look, not a session, so the
// same file is meaningful on another machine with another emulator.

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "core/Settings.h"

namespace overlaydesk {

inline constexpr uint32_t kPresetSchemaVersion = 1;

// Group headings in the Presets tab. Declared here rather than in the UI because the category
// is persisted, so these strings are part of the on-disk format.
inline constexpr const char* kCategoryBase = "Base";
inline constexpr const char* kCategoryCrt = "CRT";
inline constexpr const char* kCategoryConsoles = "Consoles";
inline constexpr const char* kCategoryFisheye = "Fisheye";
inline constexpr const char* kCategoryAntiFisheye = "Anti-fisheye";
inline constexpr const char* kCategoryOptics = "Optics";
inline constexpr const char* kCategoryTactical = "Tactical";
inline constexpr const char* kCategoryBodyWorn = "Body-worn";
inline constexpr const char* kCategoryRecon = "Recon and drone";
inline constexpr const char* kCategorySensor = "Sensor";
inline constexpr const char* kCategoryEffect = "Effect";
inline constexpr const char* kCategoryCustom = "Custom";

struct Preset {
    std::string name;
    FilterSettings filters;
    EffectSettings effects;

    // Which group the preset appears under. Persisted, so a preset the user renames stays in
    // its group and one they create can be filed somewhere of their own.
    //
    // Not visual state, so PresetMatches ignores it entirely: moving a preset between groups
    // must never make the panel report the current look as edited.
    std::string category;

    // Shipped with the application rather than created by the user. Purely informational -
    // built-ins can be edited and deleted like any other preset.
    bool builtIn = false;

    // Runtime only, never serialised: where this preset was loaded from.
    std::filesystem::path file;
};

// Display order for the groups. Lower comes first; anything unrecognised sorts last, so a
// category the user invented lands at the bottom rather than in the middle of the shipped ones.
int PresetCategoryRank(std::string_view category) noexcept;

// The category a shipped preset belongs to, or an empty string when the name is not one of
// them. This is what stops an upgrade from dumping every existing preset into "Custom": files
// written before the field existed carry no category, and SeedMissingBuiltIns deliberately
// never rewrites a file that is already there, so the group has to be recovered from the name.
std::string CategoryForBuiltIn(std::string_view name);

class PresetRepository {
public:
    explicit PresetRepository(std::filesystem::path appDataDirectory);

    const std::filesystem::path& Directory() const noexcept { return m_directory; }

    // Writes any shipped preset this installation has not offered before, and returns the
    // names it now considers offered, so the caller can pass them back as `alreadySeeded`.
    //
    // The seeded list, rather than "has the directory been created yet", is what makes this
    // safe to run on every launch. A name in the list is left alone entirely: if its file is
    // gone the user deleted it on purpose, and it stays deleted. A name missing from the list
    // is only written when no file of that name exists - an existing file is either the user's
    // own work or a build that predates the list, and either way overwriting it would throw
    // away edits. Without both halves a new version would either never deliver its new presets
    // or resurrect the ones the user threw away, and both have been shipped by real
    // applications.
    //
    // The returned names include the ones skipped because their file was already there, which
    // is what stops that check from having to be made again on every subsequent launch.
    std::vector<std::string> SeedMissingBuiltIns(
        const std::vector<std::string>& alreadySeeded) const;

    // Sorted with the built-ins first, then alphabetically. Unreadable files are logged and
    // skipped rather than aborting the whole listing.
    std::vector<Preset> LoadAll() const;

    bool Save(const Preset& preset) const;
    bool Delete(const Preset& preset) const;

    // Renames both the display name and the underlying file. Returns false and leaves
    // everything untouched if the new file could not be written.
    bool Rename(const Preset& preset, std::string_view newName) const;

    std::filesystem::path FilePathFor(std::string_view name) const;

    // "Soft CRT" -> "soft-crt". Falls back to "preset" when nothing usable survives.
    static std::string MakeFileStem(std::string_view name);

    // Trims and collapses whitespace; returns empty for a name that is only whitespace.
    static std::string SanitizeName(std::string_view name);

    // Appends " (2)", " (3)" ... until the name is free. Used by Duplicate and Save As.
    static std::string MakeUniqueName(std::string_view desired, const std::vector<Preset>& taken);

private:
    std::filesystem::path m_directory;
};

// The presets PRD section 11 suggests, as starting points the user is expected to tune.
std::vector<Preset> BuiltInPresets();

// True when `filters`/`effects` match the preset exactly. Lets the UI tell "this is Soft
// CRT" from "this started as Soft CRT and has been edited since".
bool PresetMatches(const Preset& preset, const FilterSettings& filters,
                   const EffectSettings& effects) noexcept;

}  // namespace overlaydesk
