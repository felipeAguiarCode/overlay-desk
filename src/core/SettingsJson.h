#pragma once

// Internal JSON plumbing shared by SettingsRepository and PresetRepository.
//
// Not part of any public interface: it is included only by core/Settings.cpp and
// core/PresetRepository.cpp, which keeps nlohmann/json - a ~950 KB header - out of every
// translation unit that merely wants to read a filter value.
//
// The filter and effect blocks live here rather than in Settings.cpp because a preset is
// exactly those two blocks and nothing else (CONFIGURATION.md: "Preset contem apenas estado
// visual"). Sharing the code is what guarantees a preset and settings.json describe the same
// parameters with the same names.

#include <Windows.h>

#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <system_error>

#include "core/RenderMath.h"
#include "core/Settings.h"
#include "util/Log.h"
#include "util/Win32Helpers.h"

namespace overlaydesk::json_io {

// ordered_json, not json: the default keeps keys in a std::map and would emit them
// alphabetically. ADR-0004 chose JSON because it is readable and easy to debug by hand, so
// the file keeps the order the schema is written in.
using json = nlohmann::ordered_json;

// A float widened to double serialises as its exact binary value - 0.1f becomes
// 0.10000000149011612. Rounding to six decimals keeps the file hand-editable without losing
// any precision a slider can produce.
inline double Num(float value) {
    return std::round(static_cast<double>(value) * 1e6) / 1e6;
}

// --- Tolerant readers -------------------------------------------------------------------
//
// A key that is missing or of the wrong type leaves the default in place rather than
// aborting the load: CONFIGURATION.md wants a hand-edited file with one bad field to still
// boot the application (AT-018).

inline const json* Child(const json& parent, const char* key) {
    if (!parent.is_object()) {
        return nullptr;
    }
    const auto it = parent.find(key);
    return it != parent.end() ? &(*it) : nullptr;
}

inline void Read(const json& parent, const char* key, bool& out) {
    if (const json* node = Child(parent, key); node != nullptr && node->is_boolean()) {
        out = node->get<bool>();
    }
}

inline void Read(const json& parent, const char* key, int& out) {
    if (const json* node = Child(parent, key); node != nullptr && node->is_number_integer()) {
        out = node->get<int>();
    }
}

inline void Read(const json& parent, const char* key, float& out) {
    if (const json* node = Child(parent, key); node != nullptr && node->is_number()) {
        out = node->get<float>();
    }
}

inline void Read(const json& parent, const char* key, std::string& out) {
    if (const json* node = Child(parent, key); node != nullptr && node->is_string()) {
        out = node->get<std::string>();
    }
}

inline void Read(const json& parent, const char* key, std::vector<std::string>& out) {
    const json* node = Child(parent, key);
    if (node == nullptr || !node->is_array()) {
        return;
    }
    out.clear();
    for (const json& item : *node) {
        if (item.is_string()) {
            out.push_back(item.get<std::string>());
        }
    }
}

inline std::string ReadEnumName(const json& parent, const char* key, const char* fallback) {
    std::string value = fallback;
    Read(parent, key, value);
    return value;
}

// --- Enum names --------------------------------------------------------------------------

inline ScopeShape ParseScopeShape(std::string_view name) {
    if (name == "binocular") return ScopeShape::Binocular;
    if (name == "quadTube") return ScopeShape::QuadTube;
    if (name == "tube") return ScopeShape::Tube;
    return ScopeShape::Circle;
}

inline FalseColourPalette ParseFalseColourPalette(std::string_view name) {
    if (name == "blackHot") return FalseColourPalette::BlackHot;
    if (name == "ironbow") return FalseColourPalette::Ironbow;
    if (name == "phosphor") return FalseColourPalette::Phosphor;
    if (name == "whitePhosphor") return FalseColourPalette::WhitePhosphor;
    if (name == "crossCom") return FalseColourPalette::CrossCom;
    return FalseColourPalette::WhiteHot;
}

inline GlitchStyle ParseGlitchStyle(std::string_view name) {
    if (name == "digital") return GlitchStyle::Digital;
    return GlitchStyle::Analog;
}

inline DistortionShape ParseDistortionShape(std::string_view name) {
    if (name == "crt") return DistortionShape::Crt;
    if (name == "cylindrical") return DistortionShape::Cylindrical;
    if (name == "vertical") return DistortionShape::Vertical;
    if (name == "corner") return DistortionShape::Corner;
    return DistortionShape::Radial;
}

inline ScanlineStyle ParseScanlineStyle(std::string_view name) {
    if (name == "soft") return ScanlineStyle::Soft;
    if (name == "sharp") return ScanlineStyle::Sharp;
    if (name == "apertureGrille") return ScanlineStyle::ApertureGrille;
    if (name == "slotMask") return ScanlineStyle::SlotMask;
    return ScanlineStyle::Hard;
}

inline ScanlineOrientation ParseOrientation(std::string_view name) {
    if (name == "vertical") return ScanlineOrientation::Vertical;
    if (name == "grid") return ScanlineOrientation::Grid;
    return ScanlineOrientation::Horizontal;
}

inline ScanlineScaleMode ParseScaleMode(std::string_view name) {
    if (name == "relative") return ScanlineScaleMode::Relative;
    return ScanlineScaleMode::PixelPerfect;
}

inline ChromaticAberrationMode ParseChromaticMode(std::string_view name) {
    if (name == "horizontal") return ChromaticAberrationMode::Horizontal;
    if (name == "vertical") return ChromaticAberrationMode::Vertical;
    if (name == "edge") return ChromaticAberrationMode::Edge;
    if (name == "prism") return ChromaticAberrationMode::Prism;
    if (name == "barrel") return ChromaticAberrationMode::Barrel;
    return ChromaticAberrationMode::Radial;
}

inline FpsMode ParseFpsMode(std::string_view name) {
    if (name == "cap60" || name == "60") return FpsMode::Cap60;
    if (name == "cap30" || name == "30") return FpsMode::Cap30;
    if (name == "unlimited") return FpsMode::Unlimited;
    return FpsMode::MatchSource;
}

// --- Filters and effects -----------------------------------------------------------------

inline void ReadFilterBase(const json& parent, FilterBase& out) {
    Read(parent, "enabled", out.enabled);
    Read(parent, "intensity", out.intensity);
    out.intensity = ClampIntensity(out.intensity);
}

inline void WriteFilterBase(json& out, const FilterBase& value) {
    out["enabled"] = value.enabled;
    out["intensity"] = Num(value.intensity);
}

// `filters` is the node itself, not its parent, so the same code serves settings.json and a
// preset file even though they nest it differently.
inline void ReadFilters(const json& filters, FilterSettings& out) {
    if (const json* n = Child(filters, "distortion"); n != nullptr) {
        ReadFilterBase(*n, out.distortion);
        Read(*n, "amount", out.distortion.amount);
        out.distortion.amount = ClampBipolar(out.distortion.amount);
        out.distortion.shape = ParseDistortionShape(ReadEnumName(*n, "shape", "radial"));
    }
    if (const json* n = Child(filters, "scope"); n != nullptr) {
        ReadFilterBase(*n, out.scope);
        Read(*n, "size", out.scope.size);
        Read(*n, "softness", out.scope.softness);
        Read(*n, "magnification", out.scope.magnification);
        Read(*n, "reticle", out.scope.reticle);
        out.scope.reticle = ClampIntensity(out.scope.reticle);
        out.scope.shape = ParseScopeShape(ReadEnumName(*n, "shape", "circle"));
    }
    if (const json* n = Child(filters, "vignette"); n != nullptr) {
        ReadFilterBase(*n, out.vignette);
        Read(*n, "size", out.vignette.size);
        Read(*n, "softness", out.vignette.softness);
        Read(*n, "roundness", out.vignette.roundness);
    }
    if (const json* n = Child(filters, "scanlines"); n != nullptr) {
        ReadFilterBase(*n, out.scanlines);
        Read(*n, "thickness", out.scanlines.thickness);
        Read(*n, "spacing", out.scanlines.spacing);
        out.scanlines.style = ParseScanlineStyle(ReadEnumName(*n, "style", "hard"));
        out.scanlines.orientation = ParseOrientation(ReadEnumName(*n, "orientation", "horizontal"));
        out.scanlines.scaleMode = ParseScaleMode(ReadEnumName(*n, "scaleMode", "pixelPerfect"));
        Read(*n, "beamWidth", out.scanlines.beamWidth);
        out.scanlines.beamWidth = ClampIntensity(out.scanlines.beamWidth);
        Read(*n, "interlace", out.scanlines.interlace);
        out.scanlines.interlace = ClampIntensity(out.scanlines.interlace);
    }
    if (const json* n = Child(filters, "chromaticAberration"); n != nullptr) {
        ReadFilterBase(*n, out.chromaticAberration);
        out.chromaticAberration.mode = ParseChromaticMode(ReadEnumName(*n, "mode", "radial"));
        Read(*n, "edgeBias", out.chromaticAberration.edgeBias);
        Read(*n, "redShift", out.chromaticAberration.redShift);
        Read(*n, "blueShift", out.chromaticAberration.blueShift);
    }
    if (const json* n = Child(filters, "colorCorrection"); n != nullptr) {
        ReadFilterBase(*n, out.colorCorrection);
        Read(*n, "brightness", out.colorCorrection.brightness);
        Read(*n, "contrast", out.colorCorrection.contrast);
        Read(*n, "saturation", out.colorCorrection.saturation);
        Read(*n, "gamma", out.colorCorrection.gamma);
        Read(*n, "tintR", out.colorCorrection.tint[0]);
        Read(*n, "tintG", out.colorCorrection.tint[1]);
        Read(*n, "tintB", out.colorCorrection.tint[2]);
        Read(*n, "tintAmount", out.colorCorrection.tintAmount);
        out.colorCorrection.tintAmount = ClampIntensity(out.colorCorrection.tintAmount);
        Read(*n, "posterize", out.colorCorrection.posterize);
        out.colorCorrection.posterize = ClampIntensity(out.colorCorrection.posterize);
    }
    if (const json* n = Child(filters, "bloom"); n != nullptr) {
        ReadFilterBase(*n, out.bloom);
        Read(*n, "threshold", out.bloom.threshold);
        out.bloom.threshold = ClampIntensity(out.bloom.threshold);
        Read(*n, "radius", out.bloom.radius);
        out.bloom.radius = ClampIntensity(out.bloom.radius);
        Read(*n, "tintR", out.bloom.tint[0]);
        Read(*n, "tintG", out.bloom.tint[1]);
        Read(*n, "tintB", out.bloom.tint[2]);
    }
    if (const json* n = Child(filters, "falseColour"); n != nullptr) {
        ReadFilterBase(*n, out.falseColour);
        out.falseColour.palette =
            ParseFalseColourPalette(ReadEnumName(*n, "palette", "whiteHot"));
        Read(*n, "levels", out.falseColour.levels);
        out.falseColour.levels = ClampIntensity(out.falseColour.levels);
    }
    if (const json* n = Child(filters, "edgeGlow"); n != nullptr) {
        ReadFilterBase(*n, out.edgeGlow);
        Read(*n, "width", out.edgeGlow.width);
        out.edgeGlow.width = ClampIntensity(out.edgeGlow.width);
        Read(*n, "tintR", out.edgeGlow.tint[0]);
        Read(*n, "tintG", out.edgeGlow.tint[1]);
        Read(*n, "tintB", out.edgeGlow.tint[2]);
    }
    if (const json* n = Child(filters, "lensSoftness"); n != nullptr) {
        ReadFilterBase(*n, out.lensSoftness);
        Read(*n, "center", out.lensSoftness.center);
        out.lensSoftness.center = ClampIntensity(out.lensSoftness.center);
    }
    if (const json* n = Child(filters, "lensDirt"); n != nullptr) {
        ReadFilterBase(*n, out.lensDirt);
        Read(*n, "density", out.lensDirt.density);
        out.lensDirt.density = ClampIntensity(out.lensDirt.density);
        Read(*n, "smear", out.lensDirt.smear);
        out.lensDirt.smear = ClampIntensity(out.lensDirt.smear);
    }
}

inline json SerializeFilters(const FilterSettings& s) {
    json filters;
    {
        json& n = filters["distortion"];
        WriteFilterBase(n, s.distortion);
        n["amount"] = Num(s.distortion.amount);
        n["shape"] = ToString(s.distortion.shape);
    }
    {
        json& n = filters["scope"];
        WriteFilterBase(n, s.scope);
        n["size"] = Num(s.scope.size);
        n["softness"] = Num(s.scope.softness);
        n["magnification"] = Num(s.scope.magnification);
        n["reticle"] = Num(s.scope.reticle);
        n["shape"] = ToString(s.scope.shape);
    }
    {
        json& n = filters["vignette"];
        WriteFilterBase(n, s.vignette);
        n["size"] = Num(s.vignette.size);
        n["softness"] = Num(s.vignette.softness);
        n["roundness"] = Num(s.vignette.roundness);
    }
    {
        json& n = filters["scanlines"];
        WriteFilterBase(n, s.scanlines);
        n["thickness"] = Num(s.scanlines.thickness);
        n["spacing"] = Num(s.scanlines.spacing);
        n["style"] = ToString(s.scanlines.style);
        n["orientation"] = ToString(s.scanlines.orientation);
        n["scaleMode"] = ToString(s.scanlines.scaleMode);
        n["beamWidth"] = Num(s.scanlines.beamWidth);
        n["interlace"] = Num(s.scanlines.interlace);
    }
    {
        json& n = filters["chromaticAberration"];
        WriteFilterBase(n, s.chromaticAberration);
        n["mode"] = ToString(s.chromaticAberration.mode);
        n["edgeBias"] = Num(s.chromaticAberration.edgeBias);
        n["redShift"] = Num(s.chromaticAberration.redShift);
        n["blueShift"] = Num(s.chromaticAberration.blueShift);
    }
    {
        json& n = filters["colorCorrection"];
        WriteFilterBase(n, s.colorCorrection);
        n["brightness"] = Num(s.colorCorrection.brightness);
        n["contrast"] = Num(s.colorCorrection.contrast);
        n["saturation"] = Num(s.colorCorrection.saturation);
        n["gamma"] = Num(s.colorCorrection.gamma);
        n["tintR"] = Num(s.colorCorrection.tint[0]);
        n["tintG"] = Num(s.colorCorrection.tint[1]);
        n["tintB"] = Num(s.colorCorrection.tint[2]);
        n["tintAmount"] = Num(s.colorCorrection.tintAmount);
        n["posterize"] = Num(s.colorCorrection.posterize);
    }
    {
        json& n = filters["bloom"];
        WriteFilterBase(n, s.bloom);
        n["threshold"] = Num(s.bloom.threshold);
        n["radius"] = Num(s.bloom.radius);
        n["tintR"] = Num(s.bloom.tint[0]);
        n["tintG"] = Num(s.bloom.tint[1]);
        n["tintB"] = Num(s.bloom.tint[2]);
    }
    {
        json& n = filters["falseColour"];
        WriteFilterBase(n, s.falseColour);
        n["palette"] = ToString(s.falseColour.palette);
        n["levels"] = Num(s.falseColour.levels);
    }
    {
        json& n = filters["edgeGlow"];
        WriteFilterBase(n, s.edgeGlow);
        n["width"] = Num(s.edgeGlow.width);
        n["tintR"] = Num(s.edgeGlow.tint[0]);
        n["tintG"] = Num(s.edgeGlow.tint[1]);
        n["tintB"] = Num(s.edgeGlow.tint[2]);
    }
    {
        json& n = filters["lensSoftness"];
        WriteFilterBase(n, s.lensSoftness);
        n["center"] = Num(s.lensSoftness.center);
    }
    {
        json& n = filters["lensDirt"];
        WriteFilterBase(n, s.lensDirt);
        n["density"] = Num(s.lensDirt.density);
        n["smear"] = Num(s.lensDirt.smear);
    }
    return filters;
}

inline void ReadEffects(const json& effects, EffectSettings& out) {
    if (const json* n = Child(effects, "glitch"); n != nullptr) {
        ReadFilterBase(*n, out.glitch);
        Read(*n, "frequency", out.glitch.frequency);
        Read(*n, "blockSize", out.glitch.blockSize);
        Read(*n, "jitter", out.glitch.jitter);
        Read(*n, "rgbShift", out.glitch.rgbShift);
        out.glitch.style = ParseGlitchStyle(ReadEnumName(*n, "style", "analog"));
    }
    if (const json* n = Child(effects, "noise"); n != nullptr) {
        ReadFilterBase(*n, out.noise);
        Read(*n, "grainSize", out.noise.grainSize);
        Read(*n, "speed", out.noise.speed);
        Read(*n, "colorAmount", out.noise.colorAmount);
    }
    if (const json* n = Child(effects, "flicker"); n != nullptr) {
        ReadFilterBase(*n, out.flicker);
        Read(*n, "speed", out.flicker.speed);
    }
    if (const json* n = Child(effects, "jitter"); n != nullptr) {
        ReadFilterBase(*n, out.jitter);
        Read(*n, "speed", out.jitter.speed);
    }
    if (const json* n = Child(effects, "shimmer"); n != nullptr) {
        ReadFilterBase(*n, out.shimmer);
        Read(*n, "speed", out.shimmer.speed);
        Read(*n, "scale", out.shimmer.scale);
        out.shimmer.scale = ClampIntensity(out.shimmer.scale);
    }
    if (const json* n = Child(effects, "rollingShutter"); n != nullptr) {
        ReadFilterBase(*n, out.rollingShutter);
        Read(*n, "speed", out.rollingShutter.speed);
    }
    if (const json* n = Child(effects, "scanSweep"); n != nullptr) {
        ReadFilterBase(*n, out.scanSweep);
        Read(*n, "speed", out.scanSweep.speed);
        Read(*n, "width", out.scanSweep.width);
        out.scanSweep.width = ClampIntensity(out.scanSweep.width);
    }
}

inline json SerializeEffects(const EffectSettings& s) {
    json effects;
    json& glitch = effects["glitch"];
    WriteFilterBase(glitch, s.glitch);
    glitch["frequency"] = Num(s.glitch.frequency);
    glitch["blockSize"] = Num(s.glitch.blockSize);
    glitch["jitter"] = Num(s.glitch.jitter);
    glitch["rgbShift"] = Num(s.glitch.rgbShift);
    glitch["style"] = ToString(s.glitch.style);

    json& noise = effects["noise"];
    WriteFilterBase(noise, s.noise);
    noise["grainSize"] = Num(s.noise.grainSize);
    noise["speed"] = Num(s.noise.speed);
    noise["colorAmount"] = Num(s.noise.colorAmount);

    json& flicker = effects["flicker"];
    WriteFilterBase(flicker, s.flicker);
    flicker["speed"] = Num(s.flicker.speed);

    json& jitter = effects["jitter"];
    WriteFilterBase(jitter, s.jitter);
    jitter["speed"] = Num(s.jitter.speed);

    json& shimmer = effects["shimmer"];
    WriteFilterBase(shimmer, s.shimmer);
    shimmer["speed"] = Num(s.shimmer.speed);
    shimmer["scale"] = Num(s.shimmer.scale);

    json& rollingShutter = effects["rollingShutter"];
    WriteFilterBase(rollingShutter, s.rollingShutter);
    rollingShutter["speed"] = Num(s.rollingShutter.speed);

    json& scanSweep = effects["scanSweep"];
    WriteFilterBase(scanSweep, s.scanSweep);
    scanSweep["speed"] = Num(s.scanSweep.speed);
    scanSweep["width"] = Num(s.scanSweep.width);

    return effects;
}

// --- File I/O ------------------------------------------------------------------------------

// Returns a discarded json on any failure; the caller decides what "no usable file" means.
inline json ParseJsonFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return json(json::value_t::discarded);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return json::parse(buffer.str(), nullptr, /*allow_exceptions=*/false, /*ignore_comments=*/true);
}

// CONFIGURATION.md requires atomic writes: the file is fully materialised under a temporary
// name and only then swapped in, so an interrupted write cannot leave a half-written file
// where a valid one used to be.
inline bool WriteJsonAtomically(const std::filesystem::path& path, const json& document) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        LogError("Json: cannot create {}: {}", Utf8FromWide(path.parent_path().wstring()),
                 ec.message());
        return false;
    }

    std::string text;
    try {
        text = document.dump(2);
        text.push_back('\n');
    } catch (const std::exception& e) {
        LogError("Json: serialising {} failed: {}", Utf8FromWide(path.wstring()), e.what());
        return false;
    }

    const std::filesystem::path tempPath = path.wstring() + L".tmp";
    {
        std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            LogError("Json: cannot open {} for writing.", Utf8FromWide(tempPath.wstring()));
            return false;
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.flush();
        if (!out) {
            LogError("Json: write to {} failed.", Utf8FromWide(tempPath.wstring()));
            return false;
        }
    }

    if (::MoveFileExW(tempPath.c_str(), path.c_str(),
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        LogError("Json: replacing {} failed: {}", Utf8FromWide(path.wstring()),
                 FormatWin32Error(::GetLastError()));
        std::filesystem::remove(tempPath, ec);
        return false;
    }

    return true;
}

}  // namespace overlaydesk::json_io
