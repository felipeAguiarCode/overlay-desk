#pragma once

// Transcribed from docs/specs/DATA-MODEL.md and ARCHITECTURE.md section 7. Field names and
// default values must stay in lockstep with config/settings.example.json - that file is
// the published contract for the on-disk format.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/Hotkeys.h"

namespace overlaydesk {

inline constexpr uint32_t kSettingsSchemaVersion = 1;

// The geometry a distortion follows. `amount` stays bipolar in every one of them, so the
// negative half of each shape is its anti-fisheye counterpart (RF-011).
enum class DistortionShape : int {
    Radial = 0,       // classic fisheye / pincushion, circular
    Crt = 1,          // separable per axis, the way real tube geometry bends
    Cylindrical = 2,  // horizontal only, like a curved ultrawide
    Vertical = 3,     // vertical only
    Corner = 4,       // quartic falloff: flat centre, corners pull hard
};

// The shape of the darkening, independent of orientation and spacing.
enum class ScanlineStyle : int {
    Hard = 0,            // square wave, crisp bands
    Soft = 1,            // sine falloff, gentle
    Sharp = 2,           // thin dark lines, bright between them
    ApertureGrille = 3,  // vertical RGB triads, Trinitron-style
    SlotMask = 4,        // staggered triads, the classic shadow-mask look
};

enum class ScanlineOrientation : int { Horizontal = 0, Vertical = 1, Grid = 2 };
enum class ScanlineScaleMode : int { Relative = 0, PixelPerfect = 1 };

enum class ScopeShape : int {
    Circle = 0,     // a rifle scope, a spotting scope, a peephole
    Binocular = 1,  // two overlapping circles
    QuadTube = 2,   // four overlapping circles: a panoramic four-tube NVG
    Tube = 3,       // the rounded rectangle of a CRT faceplate
};

// Luminance mapped onto a palette. This is the operation a tint cannot express: ironbow is
// not monotonic in hue, and black hot inverts before mapping (ADR-0009).
enum class FalseColourPalette : int {
    WhiteHot = 0,       // dark to light, the default of any IR sight
    BlackHot = 1,       // the inverse: hot reads dark
    Ironbow = 2,        // black -> purple -> red -> orange -> white
    Phosphor = 3,       // the P43 green of an image intensifier
    WhitePhosphor = 4,  // the blue-white of a modern tube
    CrossCom = 5,       // the cyan ramp of a sensor HUD
};

// Two shapes of the same event - a burst of damaged signal - and therefore an enum inside the
// glitch rather than a separate compression module (ADR-0009).
enum class GlitchStyle : int {
    Analog = 0,   // displaced bands and separated channels: tape and RF
    Digital = 1,  // macroblocks and banding steps: a compressed downlink falling apart
};

enum class ChromaticAberrationMode : int {
    Radial = 0,
    Horizontal = 1,
    Vertical = 2,
    Edge = 3,
    Prism = 4,   // each channel leaves at its own angle, like glass splitting light
    Barrel = 5,  // separation follows r^2, matching how a lens actually misfocuses
};

enum class FpsMode : int { MatchSource = 0, Cap60 = 1, Cap30 = 2, Unlimited = 3 };

// --- Overlay --------------------------------------------------------------------------

struct OverlaySettings {
    bool enabled = true;
    int x = 320;
    int y = 180;
    int width = 1280;
    int height = 720;
    bool resizable = true;
    bool lockPosition = false;
    bool lockSize = false;
    bool lockAspectRatio = true;
    std::string aspectRatio = "16:9";
    bool alwaysOnTop = true;
    bool clickThrough = true;
    float opacity = 1.0f;
    bool fullscreen = false;
    // Index into EnumerateMonitors(), which sorts primary-first then left-to-right, so the
    // value survives a reboot as long as the monitor layout does.
    int monitorIndex = 0;
};

// --- Filters --------------------------------------------------------------------------

// FILTERS-AND-EFFECTS.md section 1: every visual module carries these two, and nothing
// else is guaranteed.
struct FilterBase {
    bool enabled = false;
    float intensity = 1.0f;
};

struct DistortionSettings : FilterBase {
    // RF-011: bipolar. -1 anti-fisheye, 0 neutral, +1 fisheye.
    float amount = 0.0f;
    DistortionShape shape = DistortionShape::Radial;

    DistortionSettings() { intensity = 1.0f; }
};

struct VignetteSettings : FilterBase {
    float size = 0.70f;
    float softness = 0.75f;
    float roundness = 0.30f;

    VignetteSettings() { intensity = 0.35f; }
};

struct ScanlineSettings : FilterBase {
    float thickness = 1.0f;
    float spacing = 2.0f;
    ScanlineStyle style = ScanlineStyle::Hard;
    ScanlineOrientation orientation = ScanlineOrientation::Horizontal;
    ScanlineScaleMode scaleMode = ScanlineScaleMode::PixelPerfect;

    // How much a bright area widens its own scanline. On a real tube the beam spreads
    // as it is driven harder, so highlights have fat lines that merge while shadows keep
    // thin ones with black between - the single behaviour that separates a CRT from a
    // striped overlay. 0 is the uniform pattern every version before this one drew.
    float beamWidth = 0.0f;

    // Alternate fields offset by half the spacing, the way 480i is drawn. 0 is
    // progressive, and stays a function of time alone - no frame is remembered.
    float interlace = 0.0f;

    ScanlineSettings() { intensity = 0.25f; }
};

struct ChromaticAberrationSettings : FilterBase {
    ChromaticAberrationMode mode = ChromaticAberrationMode::Radial;
    float edgeBias = 0.70f;
    float redShift = 1.0f;
    float blueShift = -1.0f;

    ChromaticAberrationSettings() { intensity = 0.10f; }
};

// An optic aperture. Deliberately not a preset of the vignette: a vignette darkens towards
// transparency, while the body of a scope is solid, and the overlay has to be opaque there or
// the desktop shows through the part that is supposed to be a steel tube.
struct ScopeSettings : FilterBase {
    float size = 0.62f;          // aperture radius, as a fraction of the corner distance
    float softness = 0.05f;      // how far the glass fades into the body
    float magnification = 1.0f;  // 1.0 = none; above that the view narrows and zooms
    float reticle = 0.0f;        // 0 = no crosshair, 1 = fully opaque
    ScopeShape shape = ScopeShape::Circle;

    ScopeSettings() { intensity = 1.0f; }
};

struct ColorCorrectionSettings : FilterBase {
    float brightness = 0.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;
    float gamma = 1.0f;

    // Tint multiplies the image by a colour. It is what makes a monochrome look actually
    // monochrome IN a colour - Game Boy green, night-vision phosphor, Virtual Boy red -
    // which brightness/contrast/saturation/gamma alone cannot express at all.
    // White at 0% is a no-op, so neutral values still reproduce the source exactly.
    float tint[3]{1.0f, 1.0f, 1.0f};
    float tintAmount = 0.0f;

    // Tonal quantisation, the way a low bit-depth field display steps. 0 is identity; higher
    // values leave fewer steps. A parameter and not a filter of its own because it belongs to
    // the same category as brightness, contrast and gamma (ADR-0009).
    float posterize = 0.0f;

    ColorCorrectionSettings() { intensity = 1.0f; }
};

// The blooming of a saturated sensor or intensifier tube. A spiral of extra source taps above
// a threshold, summed - glow, not a gaussian, because a real blur would cost a full-size
// render target the ADR-0005 budget has no room for (ADR-0009 section 2).
struct BloomSettings : FilterBase {
    float threshold = 0.75f;  // luminance a pixel must reach before it blooms
    float radius = 0.35f;     // halo reach, as a fraction of the image

    // The halo is multiplied by this. White is a no-op and leaves the halo the colour of
    // the light that made it; warming it is how halation is reached - light scattering
    // inside the faceplate of a tube comes back red, which is why a bright white object
    // on a CRT has a warm edge rather than a white one.
    float tint[3]{1.0f, 1.0f, 1.0f};

    BloomSettings() { intensity = 0.35f; }
};

struct FalseColourSettings : FilterBase {
    FalseColourPalette palette = FalseColourPalette::WhiteHot;
    // 0 leaves the ramp continuous; above that it is quantised into that many steps.
    float levels = 0.0f;

    FalseColourSettings() { intensity = 1.0f; }
};

// A spatial derivative of the SOURCE texture, never of the processed result - the latter
// carries scanlines, grain and vignette, and the detector would find the edges of those
// patterns instead of the content's.
struct EdgeGlowSettings : FilterBase {
    float width = 0.50f;  // tap spacing, and therefore the thickness of the outline
    float tint[3]{0.35f, 0.80f, 1.00f};

    EdgeGlowSettings() { intensity = 0.45f; }
};

// Grease and spatter on the front element. Procedural like the grain: no texture, no permanent
// GPU resource, no repeating pattern.
struct LensDirtSettings : FilterBase {
    float density = 0.50f;  // how many smudges there are
    float smear = 0.40f;    // how much they streak rather than dot

    LensDirtSettings() { intensity = 0.25f; }
};

// A real ultra-wide lens does not resolve equally across the frame: it is sharp in the middle
// and falls apart towards the corners, and the wider it is the earlier that starts. Without it
// every fisheye reads as a geometric warp applied to a sharp digital image rather than as light
// that went through glass - which is the one thing the distortion filter cannot express on its
// own, however much it bends.
struct LensSoftnessSettings : FilterBase {
    float center = 0.45f;  // radius of the zone that stays sharp, as a fraction of the corner

    LensSoftnessSettings() { intensity = 0.40f; }
};

struct FilterSettings {
    DistortionSettings distortion;
    VignetteSettings vignette;
    ScanlineSettings scanlines;
    ChromaticAberrationSettings chromaticAberration;
    ColorCorrectionSettings colorCorrection;
    ScopeSettings scope;
    BloomSettings bloom;
    FalseColourSettings falseColour;
    EdgeGlowSettings edgeGlow;
    LensDirtSettings lensDirt;
    LensSoftnessSettings lensSoftness;
};

// --- Effects --------------------------------------------------------------------------

struct GlitchSettings : FilterBase {
    float frequency = 0.05f;
    float blockSize = 0.10f;
    float jitter = 0.05f;
    float rgbShift = 0.10f;
    // In Digital, blockSize is the macroblock edge rather than the band height.
    GlitchStyle style = GlitchStyle::Analog;

    GlitchSettings() { intensity = 0.15f; }
};

// Sensor and film grain. Procedural and time-driven like every other effect here - nothing
// is sampled from a texture and no frame is remembered (ADR-0003).
struct NoiseSettings : FilterBase {
    float grainSize = 0.5f;
    float speed = 0.6f;
    // 0 = monochrome grain, 1 = per-channel colour speckle.
    float colorAmount = 0.0f;

    NoiseSettings() { intensity = 0.12f; }
};

// Brightness oscillation, the way a tube or a projector lamp breathes.
struct FlickerSettings : FilterBase {
    float speed = 0.5f;

    FlickerSettings() { intensity = 0.15f; }
};

// Whole-image shake - an unstable signal or a camera that will not sit still. Distinct from
// the per-line jitter inside the glitch, which only fires during a burst.
struct JitterSettings : FilterBase {
    float speed = 0.5f;

    JitterSettings() { intensity = 0.10f; }
};

// A continuous, organic warp of the UV: hot air rising, or the field of an optical camouflage
// bending the light behind it. Distinct from jitter, which displaces the frame rigidly, and
// from glitch, which displaces bands - neither of those ripples.
struct ShimmerSettings : FilterBase {
    float speed = 0.50f;
    float scale = 0.50f;  // the size of the ripples

    ShimmerSettings() { intensity = 0.20f; }
};

// The shear of a CMOS sensor reading line by line while the camera moves. Not a jitter
// parameter: jitter displaces the frame rigidly, this shears as a function of the row, and one
// shared intensity would be ambiguous (ADR-0009).
struct RollingShutterSettings : FilterBase {
    float speed = 0.50f;

    RollingShutterSettings() { intensity = 0.15f; }
};

// The light bar of a scanning sensor. Applied just before the flicker: it is light from the
// display itself, so it breathes with it.
struct ScanSweepSettings : FilterBase {
    float speed = 0.40f;
    float width = 0.15f;  // bar thickness, as a fraction of the image

    ScanSweepSettings() { intensity = 0.30f; }
};

struct EffectSettings {
    GlitchSettings glitch;
    NoiseSettings noise;
    FlickerSettings flicker;
    JitterSettings jitter;
    ShimmerSettings shimmer;
    RollingShutterSettings rollingShutter;
    ScanSweepSettings scanSweep;
};

// --- Render / UI ----------------------------------------------------------------------

struct RenderSettings {
    FpsMode fpsMode = FpsMode::MatchSource;
    int fpsCap = 60;
    bool pauseWhenSourceMinimized = true;
};

struct UiSettings {
    std::string logLevel = "info";
    int activeTab = 0;
    int controlWindowWidth = 460;
    int controlWindowHeight = 720;
    bool showAdvanced = false;
    // Convenience only: the target is re-selected by title/executable at startup, never by
    // HWND or PID (CONFIGURATION.md forbids persisting those).
    bool restoreLastTarget = false;
    std::string lastTargetTitle;
    std::string lastTargetExecutable;
    // Name of the preset the visual parameters last came from. Empty, or stale once the
    // user edits anything, in which case the UI reports the state as custom.
    std::string activePreset;

    // Every built-in preset this installation has already offered. It is what lets a new
    // version add presets without resurrecting the ones the user deleted: a name absent from
    // this list has never been seen, a name present but missing from disk was thrown away on
    // purpose. Without it those two states are indistinguishable.
    std::vector<std::string> seededPresets;
};

// --- Root -----------------------------------------------------------------------------

struct AppSettings {
    uint32_t schemaVersion = kSettingsSchemaVersion;
    OverlaySettings overlay;
    FilterSettings filters;
    EffectSettings effects;
    RenderSettings render;
    UiSettings ui;
    // Global shortcuts. Deliberately not part of a preset: a preset is visual state only
    // (CONFIGURATION.md), and shortcuts belong to the installation, not to a look.
    HotkeySettings hotkeys;
};

// --- Persistence ----------------------------------------------------------------------

// CONFIGURATION.md: JSON, atomic writes, versioned schema, and a corrupt file must never
// block startup (AT-018).
class SettingsRepository {
public:
    explicit SettingsRepository(std::filesystem::path appDataDirectory);

    const std::filesystem::path& FilePath() const noexcept { return m_filePath; }

    // Returns defaults - and logs - when the file is missing, unreadable, malformed, or
    // written by a schema version we cannot migrate.
    AppSettings Load() const;

    // Writes to a sibling temp file and then replaces the target, so an interrupted write
    // cannot leave a half-written settings file behind. Returns false on failure.
    bool Save(const AppSettings& settings) const;

private:
    std::filesystem::path m_directory;
    std::filesystem::path m_filePath;
};

// Effective frame interval in seconds for the configured pacing mode. Returns 0 when
// frames should never be dropped (RNF-003 "unlimited").
double FrameIntervalSeconds(const RenderSettings& render) noexcept;

const char* ToString(FpsMode mode) noexcept;
const char* ToString(DistortionShape shape) noexcept;
const char* ToString(ScanlineStyle style) noexcept;
const char* ToString(ScanlineOrientation orientation) noexcept;
const char* ToString(ScanlineScaleMode mode) noexcept;
const char* ToString(ChromaticAberrationMode mode) noexcept;
const char* ToString(ScopeShape shape) noexcept;
const char* ToString(FalseColourPalette palette) noexcept;
const char* ToString(GlitchStyle style) noexcept;

}  // namespace overlaydesk
