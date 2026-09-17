#include "core/PresetRepository.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>

#include "core/SettingsJson.h"

namespace overlaydesk {
namespace {

using json_io::Child;
using json_io::json;

constexpr const char* kExtension = ".json";

bool NearlyEqual(float a, float b) noexcept {
    // Presets round-trip through six-decimal JSON, so an exact comparison would report a
    // freshly applied preset as already edited.
    return std::fabs(a - b) < 1e-5f;
}

bool SameBase(const FilterBase& a, const FilterBase& b) noexcept {
    return a.enabled == b.enabled && NearlyEqual(a.intensity, b.intensity);
}

// --- Built-ins ----------------------------------------------------------------------------
//
// Deliberately conservative. These are starting points, and a preset that arrives screaming
// gets turned off rather than tuned.

Preset MakeBuiltIn(const char* name) {
    Preset preset;
    preset.name = name;
    preset.builtIn = true;
    return preset;
}

Preset NeutralPreset() {
    // Everything off - the way back to an untouched image, and the reference the other
    // presets are judged against.
    return MakeBuiltIn("Neutral");
}

Preset SoftCrtPreset() {
    Preset p = MakeBuiltIn("Soft CRT");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.12f;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.22f;
    p.filters.scanlines.thickness = 1.0f;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.30f;
    p.filters.vignette.size = 0.72f;
    p.filters.vignette.softness = 0.65f;
    p.filters.vignette.roundness = 0.45f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.08f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Edge;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 1.05f;
    p.filters.colorCorrection.saturation = 1.05f;
    return p;
}

Preset ArcadeCrtPreset() {
    Preset p = MakeBuiltIn("Arcade CRT");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.18f;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.45f;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.45f;
    p.filters.vignette.size = 0.60f;
    p.filters.vignette.softness = 0.60f;
    p.filters.vignette.roundness = 0.50f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.15f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Edge;
    p.filters.chromaticAberration.edgeBias = 0.80f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.brightness = 0.02f;
    p.filters.colorCorrection.contrast = 1.15f;
    p.filters.colorCorrection.saturation = 1.15f;
    return p;
}

Preset CurvedCrtPreset() {
    Preset p = MakeBuiltIn("Curved CRT");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.55f;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.40f;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.55f;
    p.filters.vignette.size = 0.50f;
    p.filters.vignette.softness = 0.70f;
    p.filters.vignette.roundness = 0.75f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.22f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Edge;
    p.filters.chromaticAberration.edgeBias = 0.85f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 1.10f;
    return p;
}

Preset GameBoyPreset() {
    // The handheld look is a flat LCD with a visible pixel grid, so no distortion and a
    // grid rather than scanlines.
    //
    // Saturation goes to zero and the tint puts the DMG green back on top, which is the only
    // order that works: tinting a still-colourful image just biases it green, while tinting a
    // monochrome one reproduces a single-dye screen exactly.
    Preset p = MakeBuiltIn("Game Boy");
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.25f;
    p.filters.scanlines.thickness = 1.0f;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.scanlines.orientation = ScanlineOrientation::Grid;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.0f;
    p.filters.colorCorrection.contrast = 1.25f;
    p.filters.colorCorrection.gamma = 1.15f;
    p.filters.colorCorrection.brightness = 0.02f;
    p.filters.colorCorrection.tint[0] = 0.61f;  // #9BBC0F, the DMG's lightest shade
    p.filters.colorCorrection.tint[1] = 0.74f;
    p.filters.colorCorrection.tint[2] = 0.16f;
    p.filters.colorCorrection.tintAmount = 0.90f;
    return p;
}

Preset GameBoyAdvancePreset() {
    // The original GBA screen was famously dim and washed out; this leans that way rather
    // than flattering it.
    Preset p = MakeBuiltIn("Game Boy Advance");
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.18f;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.scanlines.orientation = ScanlineOrientation::Grid;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.90f;
    p.filters.colorCorrection.contrast = 0.95f;
    p.filters.colorCorrection.gamma = 1.10f;
    return p;
}

Preset SnesPreset() {
    Preset p = MakeBuiltIn("SNES");
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.28f;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.25f;
    p.filters.vignette.size = 0.75f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 1.05f;
    return p;
}

Preset MegaDrivePreset() {
    // A touch of horizontal channel separation stands in for the composite output most of
    // these consoles were actually played through.
    Preset p = MakeBuiltIn("Mega Drive");
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.32f;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.18f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Horizontal;
    p.filters.chromaticAberration.edgeBias = 0.20f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.25f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 1.10f;
    p.filters.colorCorrection.saturation = 1.10f;
    return p;
}

Preset PlayStationPreset() {
    Preset p = MakeBuiltIn("PlayStation");
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.20f;
    p.filters.scanlines.spacing = 2.0f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.12f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.30f;
    p.filters.vignette.size = 0.68f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.gamma = 1.05f;
    return p;
}

Preset BrokenSignalPreset() {
    // The one built-in that exercises the effects side.
    Preset p = MakeBuiltIn("Broken Signal");
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.35f;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.20f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Horizontal;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.40f;
    p.effects.glitch.enabled = true;
    p.effects.glitch.intensity = 0.55f;
    p.effects.glitch.frequency = 0.18f;
    p.effects.glitch.blockSize = 0.08f;
    p.effects.glitch.jitter = 0.35f;
    p.effects.glitch.rgbShift = 0.45f;
    return p;
}

// --- Anti-fisheye ---------------------------------------------------------------------------
//
// These correct a source that is ALREADY bulging - wide-angle capture, a curved-screen
// recording, an emulator shader that overdid its own barrel. On an undistorted source they
// pinch the image inward instead, which is the honest consequence of running a correction
// against something that needs none.
//
// Every one of them leaves the rest of the pipeline alone: correcting geometry is the job, and
// stacking scanlines on top would only fight it.

Preset AntiFisheyeLightPreset() {
    Preset p = MakeBuiltIn("Anti-Fisheye Light");
    p.filters.distortion.enabled = true;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.distortion.amount = -0.30f;
    return p;
}

Preset AntiFisheyeStrongPreset() {
    Preset p = MakeBuiltIn("Anti-Fisheye Strong");
    p.filters.distortion.enabled = true;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.distortion.amount = -0.75f;
    return p;
}

Preset LensCorrectionPreset() {
    // Wide-angle and action-cam footage bulges and fringes at once, because both come from the
    // same lens. Correcting only the geometry leaves the fringing behind and makes it more
    // obvious, so this pairs the debarrel with a counter-shift: redShift and blueShift are
    // swapped relative to the default, pulling the channels back together instead of apart.
    Preset p = MakeBuiltIn("Lens Correction");
    p.filters.distortion.enabled = true;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.distortion.amount = -0.50f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.10f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;
    p.filters.chromaticAberration.redShift = -1.0f;
    p.filters.chromaticAberration.blueShift = 1.0f;
    return p;
}

Preset AntiFisheyeHorizontalPreset() {
    // For a source that bows on one axis only - an ultrawide capture, or a curved monitor
    // recorded head-on. Radial would flatten the vertical axis too and overcorrect it.
    Preset p = MakeBuiltIn("Anti-Fisheye Horizontal");
    p.filters.distortion.enabled = true;
    p.filters.distortion.shape = DistortionShape::Cylindrical;
    p.filters.distortion.amount = -0.55f;
    return p;
}

Preset AntiFisheyeCornersPreset() {
    // Quartic falloff: the middle of the frame is left essentially untouched and the
    // correction concentrates where a wide lens actually bends things - the corners.
    Preset p = MakeBuiltIn("Anti-Fisheye Corners");
    p.filters.distortion.enabled = true;
    p.filters.distortion.shape = DistortionShape::Corner;
    p.filters.distortion.amount = -0.70f;
    return p;
}

// --- Shadow-mask CRTs -------------------------------------------------------------------------

Preset TrinitronPreset() {
    // An aperture grille has vertical wires and no horizontal mask, which is why Trinitrons
    // looked brighter and sharper than shadow-mask sets.
    Preset p = MakeBuiltIn("Trinitron");
    p.filters.distortion.enabled = true;
    p.filters.distortion.shape = DistortionShape::Cylindrical;
    p.filters.distortion.amount = 0.10f;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.style = ScanlineStyle::ApertureGrille;
    p.filters.scanlines.intensity = 0.35f;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.25f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 1.12f;
    p.filters.colorCorrection.saturation = 1.10f;
    return p;
}

Preset ShadowMaskPreset() {
    Preset p = MakeBuiltIn("Shadow Mask");
    p.filters.distortion.enabled = true;
    p.filters.distortion.shape = DistortionShape::Crt;
    p.filters.distortion.amount = 0.25f;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.style = ScanlineStyle::SlotMask;
    p.filters.scanlines.intensity = 0.40f;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.40f;
    p.filters.vignette.roundness = 0.55f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 1.15f;
    p.filters.colorCorrection.brightness = 0.03f;
    return p;
}

Preset SoftScanlinesPreset() {
    // The gentlest CRT hint in the set: a sine falloff instead of hard bands, so the pattern
    // never fights fine detail in the source.
    Preset p = MakeBuiltIn("Soft Scanlines");
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.style = ScanlineStyle::Soft;
    p.filters.scanlines.intensity = 0.20f;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 1.04f;
    return p;
}

// --- Effect-led -------------------------------------------------------------------------------

Preset VhsPreset() {
    // Tape: horizontal colour bleed, an unstable picture, grain, and dropouts. Each of those
    // is a different module, which is the point of the preset.
    Preset p = MakeBuiltIn("VHS");
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.style = ScanlineStyle::Soft;
    p.filters.scanlines.intensity = 0.30f;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.30f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Horizontal;
    p.filters.chromaticAberration.edgeBias = 0.15f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.35f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 1.20f;
    p.filters.colorCorrection.contrast = 0.95f;
    p.filters.colorCorrection.gamma = 1.08f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.10f;
    p.effects.noise.grainSize = 0.35f;
    p.effects.noise.speed = 0.8f;
    p.effects.jitter.enabled = true;
    p.effects.jitter.intensity = 0.08f;
    p.effects.jitter.speed = 0.7f;
    p.effects.glitch.enabled = true;
    p.effects.glitch.intensity = 0.30f;
    p.effects.glitch.frequency = 0.06f;
    return p;
}

Preset ProjectorPreset() {
    // A lamp breathing, a soft round falloff, and warm, slightly washed colour.
    Preset p = MakeBuiltIn("Projector");
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.45f;
    p.filters.vignette.size = 0.55f;
    p.filters.vignette.softness = 0.85f;
    p.filters.vignette.roundness = 0.90f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 0.92f;
    p.filters.colorCorrection.saturation = 0.95f;
    p.filters.colorCorrection.gamma = 1.12f;
    p.filters.colorCorrection.brightness = 0.03f;
    p.effects.flicker.enabled = true;
    p.effects.flicker.intensity = 0.22f;
    p.effects.flicker.speed = 0.35f;
    return p;
}

Preset FilmGrainPreset() {
    // Grain and nothing else, so it can be layered on top of any other look by hand.
    Preset p = MakeBuiltIn("Film Grain");
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.16f;
    p.effects.noise.grainSize = 0.30f;
    p.effects.noise.speed = 0.9f;
    p.effects.noise.colorAmount = 0.15f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 1.06f;
    p.filters.colorCorrection.saturation = 0.94f;
    return p;
}

Preset PrismLensPreset() {
    // Prism fans the channels to different angles instead of sliding them along one line,
    // which reads as cheap glass rather than a misaligned tube.
    Preset p = MakeBuiltIn("Prism Lens");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.20f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.45f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Prism;
    p.filters.chromaticAberration.edgeBias = 0.65f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.35f;
    p.filters.vignette.roundness = 0.80f;
    return p;
}

}  // namespace

// --- Fisheye family ---------------------------------------------------------------------
//
// The distortion filter pins the frame corners to themselves at every amount, so even the
// extreme settings here fill the overlay completely - no transparent wedges, no content
// pushed off the edge. What changes between these is the shape, and the shapes bend very
// differently: Radial is a lens, Crt is a tube, Corner leaves the middle alone entirely.

Preset FisheyeWidePreset() {
    Preset p = MakeBuiltIn("Fisheye Wide");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.55f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.30f;
    p.filters.vignette.size = 0.78f;
    p.filters.vignette.softness = 0.70f;
    p.filters.vignette.roundness = 0.70f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.10f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;
    return p;
}

Preset FisheyeExtremePreset() {
    // The full +1.0. Worth shipping as a preset precisely because it is the boundary: if
    // anything is going to break geometrically, it breaks here (AT-008).
    Preset p = MakeBuiltIn("Fisheye Extreme");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 1.00f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.45f;
    p.filters.vignette.size = 0.68f;
    p.filters.vignette.softness = 0.60f;
    p.filters.vignette.roundness = 0.90f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.16f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;
    return p;
}

Preset FisheyeCrtPreset() {
    // Crt shape rather than Radial: a tube bulges along each axis separately, so lines near
    // the middle of an edge stay straighter than a spherical lens would leave them.
    Preset p = MakeBuiltIn("Fisheye CRT");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.70f;
    p.filters.distortion.shape = DistortionShape::Crt;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.24f;
    p.filters.scanlines.style = ScanlineStyle::Soft;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.40f;
    p.filters.vignette.size = 0.74f;
    p.filters.vignette.softness = 0.65f;
    p.filters.vignette.roundness = 0.55f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.12f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Edge;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 1.08f;
    return p;
}

Preset PeepholePreset() {
    // A door viewer: violent bulge, and a hard round vignette doing the work of the barrel
    // the lens sits in.
    Preset p = MakeBuiltIn("Peephole");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.95f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.90f;
    p.filters.vignette.size = 0.46f;
    p.filters.vignette.softness = 0.30f;
    p.filters.vignette.roundness = 1.00f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.20f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.85f;
    p.filters.colorCorrection.contrast = 1.10f;
    p.filters.colorCorrection.gamma = 1.08f;
    return p;
}

Preset SecurityCamPreset() {
    Preset p = MakeBuiltIn("Security Cam");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.45f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.14f;
    p.filters.scanlines.style = ScanlineStyle::Hard;
    p.filters.scanlines.spacing = 4.0f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.45f;
    p.filters.vignette.size = 0.70f;
    p.filters.vignette.softness = 0.55f;
    p.filters.vignette.roundness = 0.60f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.35f;
    p.filters.colorCorrection.contrast = 1.12f;
    p.filters.colorCorrection.tint[0] = 0.82f;  // the cold cast of a cheap CMOS sensor
    p.filters.colorCorrection.tint[1] = 0.88f;
    p.filters.colorCorrection.tint[2] = 1.00f;
    p.filters.colorCorrection.tintAmount = 0.40f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.14f;
    p.effects.noise.grainSize = 0.40f;
    p.effects.noise.speed = 0.75f;
    return p;
}

// --- More consoles ----------------------------------------------------------------------

Preset NesPreset() {
    // Composite into a small tube: coarse lines, and the colour bleed approximated by a
    // horizontal channel shift rather than a real NTSC decode.
    Preset p = MakeBuiltIn("NES");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.10f;
    p.filters.distortion.shape = DistortionShape::Crt;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.32f;
    p.filters.scanlines.style = ScanlineStyle::Hard;
    p.filters.scanlines.thickness = 1.0f;
    p.filters.scanlines.spacing = 4.0f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.28f;
    p.filters.vignette.size = 0.76f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.14f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Horizontal;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 1.12f;
    p.filters.colorCorrection.contrast = 1.10f;
    return p;
}

Preset Nintendo64Preset() {
    // The N64's video encoder ran everything through a blur and a slight wash. The soft
    // scanlines and lifted gamma carry the milky, low-contrast half of that; the actual
    // softening is the lens softness stage the ADR-0011 added, dialled to reach almost to the
    // centre - the encoder blurred the whole picture, not just the corners.
    Preset p = MakeBuiltIn("Nintendo 64");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.08f;
    p.filters.distortion.shape = DistortionShape::Crt;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.16f;
    p.filters.scanlines.style = ScanlineStyle::Soft;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.24f;
    p.filters.vignette.size = 0.80f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.09f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Horizontal;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.92f;
    p.filters.colorCorrection.contrast = 0.94f;
    p.filters.colorCorrection.gamma = 1.14f;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.42f;
    p.filters.lensSoftness.center = 0.10f;
    p.filters.colorCorrection.brightness = 0.03f;
    return p;
}

Preset NeoGeoPreset() {
    // An arcade cabinet, so the aperture grille and a lot more contrast than a home set.
    Preset p = MakeBuiltIn("Neo Geo");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.16f;
    p.filters.distortion.shape = DistortionShape::Crt;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.34f;
    p.filters.scanlines.style = ScanlineStyle::ApertureGrille;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.34f;
    p.filters.vignette.size = 0.74f;
    p.filters.vignette.roundness = 0.40f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.10f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Edge;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 1.25f;
    p.filters.colorCorrection.contrast = 1.18f;
    p.filters.colorCorrection.gamma = 0.94f;
    return p;
}

Preset PcEnginePreset() {
    Preset p = MakeBuiltIn("PC Engine");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.12f;
    p.filters.distortion.shape = DistortionShape::Crt;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.28f;
    p.filters.scanlines.style = ScanlineStyle::SlotMask;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.26f;
    p.filters.vignette.size = 0.78f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 1.14f;
    p.filters.colorCorrection.contrast = 1.08f;
    return p;
}

Preset VirtualBoyPreset() {
    // A single red LED array and nothing else, which is exactly what saturation 0 plus a pure
    // red tint produces.
    Preset p = MakeBuiltIn("Virtual Boy");
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.30f;
    p.filters.scanlines.style = ScanlineStyle::Sharp;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.55f;
    p.filters.vignette.size = 0.66f;
    p.filters.vignette.softness = 0.50f;
    p.filters.vignette.roundness = 0.85f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.0f;
    p.filters.colorCorrection.contrast = 1.30f;
    p.filters.colorCorrection.gamma = 1.05f;
    p.filters.colorCorrection.tint[0] = 0.92f;
    p.filters.colorCorrection.tint[1] = 0.05f;
    p.filters.colorCorrection.tint[2] = 0.05f;
    p.filters.colorCorrection.tintAmount = 1.00f;
    return p;
}

// --- Tactical and body-worn camera ------------------------------------------------------
//
// These target the camera rather than the display: the wide lens, the sensor noise and the
// unsteady mount of body and helmet cameras, plus the two monochrome views a tactical game
// puts on screen. Nothing here reads the game - they are looks applied to whatever the
// overlay happens to be capturing.

Preset BodycamPreset() {
    Preset p = MakeBuiltIn("Bodycam");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.50f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.50f;
    p.filters.vignette.size = 0.68f;
    p.filters.vignette.softness = 0.60f;
    p.filters.vignette.roundness = 0.75f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.12f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.78f;
    p.filters.colorCorrection.contrast = 1.16f;
    p.filters.colorCorrection.gamma = 1.06f;
    p.filters.colorCorrection.tint[0] = 1.00f;
    p.filters.colorCorrection.tint[1] = 0.96f;
    p.filters.colorCorrection.tint[2] = 0.90f;
    p.filters.colorCorrection.tintAmount = 0.30f;
    p.effects.noise.intensity = 0.16f;
    p.effects.noise.grainSize = 0.35f;
    p.effects.noise.speed = 0.80f;
    p.effects.noise.colorAmount = 0.25f;
    // The two artefacts that give a cheap body-worn camera away (ADR-0009): the sensor shears
    // when the wearer moves, and the front element is never clean.
    p.effects.rollingShutter.intensity = 0.14f;
    p.effects.rollingShutter.speed = 0.45f;
    p.filters.lensDirt.enabled = true;
    p.filters.lensDirt.intensity = 0.18f;
    p.filters.lensDirt.density = 0.45f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.22f;
    p.filters.bloom.threshold = 0.72f;
    p.filters.bloom.radius = 0.28f;
    return p;
}

Preset HelmetCamPreset() {
    // The same camera, mounted on someone who is moving: wider still, and the jitter turned
    // on. Kept low - the shake is meant to be felt, not fought.
    Preset p = MakeBuiltIn("Helmet Cam");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.75f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.55f;
    p.filters.vignette.size = 0.64f;
    p.filters.vignette.softness = 0.55f;
    p.filters.vignette.roundness = 0.85f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.15f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.80f;
    p.filters.colorCorrection.contrast = 1.20f;
    p.effects.noise.intensity = 0.18f;
    p.effects.noise.grainSize = 0.30f;
    p.effects.noise.speed = 0.85f;
    p.effects.jitter.intensity = 0.08f;
    p.effects.jitter.speed = 0.55f;
    // Harder shear than the chest mount: a head moves faster and stops more abruptly.
    p.effects.rollingShutter.intensity = 0.24f;
    p.effects.rollingShutter.speed = 0.62f;
    p.filters.lensDirt.enabled = true;
    p.filters.lensDirt.intensity = 0.22f;
    p.filters.lensDirt.density = 0.55f;
    p.filters.lensDirt.smear = 0.55f;
    return p;
}

Preset NightVisionPreset() {
    // Saturation to zero first, then the P43 phosphor ramp on top, so this is a real
    // single-channel image rather than a green cast over a colour one. The heavy grain is the
    // point: an image intensifier is loud, and a clean green picture reads as a filter.
    //
    // The bloom is what ADR-0009 added for this preset specifically. A tube saturates on any
    // bright source and spills charge into its neighbours - without that halo the green reads
    // as a colour filter no matter how much grain is piled on.
    Preset p = MakeBuiltIn("Night Vision");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.30f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.12f;
    p.filters.scanlines.style = ScanlineStyle::Soft;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.85f;
    p.filters.vignette.size = 0.54f;
    p.filters.vignette.softness = 0.38f;
    p.filters.vignette.roundness = 1.00f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.0f;
    p.filters.colorCorrection.brightness = 0.10f;
    p.filters.colorCorrection.contrast = 1.25f;
    p.filters.colorCorrection.gamma = 0.80f;
    p.filters.falseColour.enabled = true;
    p.filters.falseColour.palette = FalseColourPalette::Phosphor;
    p.filters.falseColour.intensity = 1.00f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.55f;
    p.filters.bloom.threshold = 0.55f;
    p.filters.bloom.radius = 0.45f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.30f;
    p.effects.noise.grainSize = 0.25f;
    p.effects.noise.speed = 0.95f;
    p.effects.flicker.enabled = true;
    p.effects.flicker.intensity = 0.10f;
    p.effects.flicker.speed = 0.70f;
    return p;
}

Preset ThermalPreset() {
    // An ironbow IR view. This used to be an amber tint over a crushed monochrome, which is as
    // close as multiplication can get; ADR-0009 gave the pipeline a real luminance-to-palette
    // stage, and ironbow needs it - the ramp climbs through purple and red before it reaches
    // white, and hue that goes up and back down is not something a tint can express.
    //
    // Saturation to zero first, exactly as with the tint: the ramp has to read brightness, not
    // a colour cast left over from the source.
    Preset p = MakeBuiltIn("Thermal");
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.60f;
    p.filters.vignette.size = 0.62f;
    p.filters.vignette.softness = 0.45f;
    p.filters.vignette.roundness = 0.80f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.0f;
    p.filters.colorCorrection.contrast = 1.55f;
    p.filters.colorCorrection.gamma = 0.78f;
    p.filters.colorCorrection.brightness = 0.02f;
    p.filters.falseColour.enabled = true;
    p.filters.falseColour.palette = FalseColourPalette::Ironbow;
    p.filters.falseColour.intensity = 1.00f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.12f;
    p.effects.noise.grainSize = 0.60f;
    p.effects.noise.speed = 0.50f;
    return p;
}

Preset BreachPreset() {
    // No lens at all - this one is about the room. Crushed, desaturated and unstable, for the
    // moment the light goes.
    Preset p = MakeBuiltIn("Breach");
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.70f;
    p.filters.vignette.size = 0.60f;
    p.filters.vignette.softness = 0.50f;
    p.filters.vignette.roundness = 0.55f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.14f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Edge;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.55f;
    p.filters.colorCorrection.contrast = 1.35f;
    p.filters.colorCorrection.gamma = 0.88f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.14f;
    p.effects.noise.grainSize = 0.40f;
    p.effects.noise.speed = 0.90f;
    p.effects.flicker.enabled = true;
    p.effects.flicker.intensity = 0.28f;
    p.effects.flicker.speed = 0.85f;
    // A room lit by whatever is left of the lights blows out around every one of them.
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.40f;
    p.filters.bloom.threshold = 0.62f;
    p.filters.bloom.radius = 0.40f;
    return p;
}

// --- Optics ------------------------------------------------------------------------------
//
// The scope filter supplies the aperture; the bulge is the distortion filter doing what it
// already did. Keeping them separate is what lets these be tuned independently - a wider
// aperture without a flatter image, or the other way round.

Preset SniperScopePreset() {
    Preset p = MakeBuiltIn("Sniper Scope");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.60f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.scope.enabled = true;
    p.filters.scope.size = 0.60f;
    p.filters.scope.softness = 0.04f;
    p.filters.scope.magnification = 1.35f;
    p.filters.scope.reticle = 0.85f;
    p.filters.scope.shape = ScopeShape::Circle;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.50f;
    p.filters.vignette.size = 0.50f;
    p.filters.vignette.softness = 0.45f;
    p.filters.vignette.roundness = 1.00f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.10f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.85f;
    p.filters.colorCorrection.contrast = 1.12f;
    return p;
}

Preset ScopeCamPreset() {
    // A camera filming down someone else's optic: no reticle of its own, a much harder bulge,
    // and the washed-out, slightly warm cast of a phone held up to the eyepiece.
    Preset p = MakeBuiltIn("Scope Cam");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.90f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.scope.enabled = true;
    p.filters.scope.size = 0.72f;
    p.filters.scope.softness = 0.10f;
    p.filters.scope.magnification = 1.00f;
    p.filters.scope.reticle = 0.0f;
    p.filters.scope.shape = ScopeShape::Circle;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.12f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.55f;
    p.filters.colorCorrection.contrast = 1.10f;
    p.filters.colorCorrection.gamma = 1.06f;
    p.filters.colorCorrection.tint[0] = 1.00f;
    p.filters.colorCorrection.tint[1] = 0.98f;
    p.filters.colorCorrection.tint[2] = 0.94f;
    p.filters.colorCorrection.tintAmount = 0.35f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.10f;
    p.effects.noise.grainSize = 0.35f;
    p.effects.noise.speed = 0.75f;
    return p;
}

Preset BinocularsPreset() {
    Preset p = MakeBuiltIn("Binoculars");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.35f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.scope.enabled = true;
    p.filters.scope.size = 0.52f;
    p.filters.scope.softness = 0.05f;
    p.filters.scope.magnification = 1.60f;
    p.filters.scope.reticle = 0.0f;
    p.filters.scope.shape = ScopeShape::Binocular;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.40f;
    p.filters.vignette.size = 0.55f;
    p.filters.vignette.softness = 0.50f;
    p.filters.vignette.roundness = 1.00f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.09f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 1.06f;
    return p;
}

Preset SpotterScopePreset() {
    // The shooting partner's view: more magnification than the rifle optic, a wider aperture,
    // and the shake that comes with both.
    Preset p = MakeBuiltIn("Spotter Scope");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.45f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.scope.enabled = true;
    p.filters.scope.size = 0.66f;
    p.filters.scope.softness = 0.06f;
    p.filters.scope.magnification = 2.20f;
    p.filters.scope.reticle = 0.35f;
    p.filters.scope.shape = ScopeShape::Circle;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.45f;
    p.filters.vignette.size = 0.52f;
    p.filters.vignette.softness = 0.45f;
    p.filters.vignette.roundness = 1.00f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.90f;
    p.filters.colorCorrection.contrast = 1.10f;
    p.effects.jitter.enabled = true;
    p.effects.jitter.intensity = 0.06f;
    p.effects.jitter.speed = 0.35f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.08f;
    p.effects.noise.grainSize = 0.45f;
    p.effects.noise.speed = 0.60f;
    return p;
}

// --- Body-worn and SWAT -------------------------------------------------------------------
//
// The tactical family above describes cameras in general; this one describes a specific job.
// Everything here is a look applied to whatever the overlay happens to be capturing - nothing
// reads the game, and none of these know what is on screen.

Preset ChestCamPreset() {
    // The chest mount: lower than the eye, wider than the eye, and permanently slightly dirty.
    Preset p = MakeBuiltIn("Chest Cam");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.62f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.48f;
    p.filters.vignette.size = 0.66f;
    p.filters.vignette.softness = 0.62f;
    p.filters.vignette.roundness = 0.80f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.13f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.82f;
    p.filters.colorCorrection.contrast = 1.14f;
    p.filters.colorCorrection.gamma = 1.04f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.26f;
    p.filters.bloom.threshold = 0.70f;
    p.filters.bloom.radius = 0.30f;
    p.filters.lensDirt.enabled = true;
    p.filters.lensDirt.intensity = 0.24f;
    p.filters.lensDirt.density = 0.50f;
    p.filters.lensDirt.smear = 0.45f;
    p.effects.noise.intensity = 0.15f;
    p.effects.noise.grainSize = 0.35f;
    p.effects.noise.speed = 0.80f;
    p.effects.noise.colorAmount = 0.22f;
    p.effects.rollingShutter.intensity = 0.18f;
    p.effects.rollingShutter.speed = 0.50f;
    return p;
}

Preset EntryTeamPreset() {
    // The seconds after the door goes: no lens character at all, just a room that is too dark,
    // lights that are too bright, and a camera fighting both.
    Preset p = MakeBuiltIn("Entry Team");
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.62f;
    p.filters.vignette.size = 0.58f;
    p.filters.vignette.softness = 0.52f;
    p.filters.vignette.roundness = 0.60f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.62f;
    p.filters.colorCorrection.contrast = 1.42f;
    p.filters.colorCorrection.gamma = 0.86f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.48f;
    p.filters.bloom.threshold = 0.58f;
    p.filters.bloom.radius = 0.42f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.20f;
    p.effects.noise.grainSize = 0.32f;
    p.effects.noise.speed = 0.90f;
    p.effects.flicker.enabled = true;
    p.effects.flicker.intensity = 0.18f;
    p.effects.flicker.speed = 0.75f;
    p.effects.rollingShutter.enabled = true;
    p.effects.rollingShutter.intensity = 0.22f;
    p.effects.rollingShutter.speed = 0.70f;
    return p;
}

Preset NightOpsPreset() {
    // Four tubes. The overlapping circles are the whole reason ScopeShape gained quadTube: a
    // panoramic goggle shows one continuous scene through four apertures, and the seams
    // between them are what nobody who has not looked through a pair expects.
    Preset p = MakeBuiltIn("Night Ops");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.28f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.scope.enabled = true;
    p.filters.scope.size = 0.78f;
    p.filters.scope.softness = 0.06f;
    p.filters.scope.magnification = 1.0f;
    p.filters.scope.reticle = 0.0f;
    p.filters.scope.shape = ScopeShape::QuadTube;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.0f;
    p.filters.colorCorrection.brightness = 0.12f;
    p.filters.colorCorrection.contrast = 1.28f;
    p.filters.colorCorrection.gamma = 0.78f;
    p.filters.falseColour.enabled = true;
    p.filters.falseColour.palette = FalseColourPalette::Phosphor;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.62f;
    p.filters.bloom.threshold = 0.50f;
    p.filters.bloom.radius = 0.48f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.32f;
    p.effects.noise.grainSize = 0.22f;
    p.effects.noise.speed = 0.95f;
    p.effects.flicker.enabled = true;
    p.effects.flicker.intensity = 0.12f;
    p.effects.flicker.speed = 0.72f;
    return p;
}

Preset WhitePhosphorPreset() {
    // The modern tube. White phosphor is not "night vision without the green" - the ramp is
    // faintly blue and the contrast curve is gentler, which is why it is easier to read faces
    // through than P43 is.
    Preset p = MakeBuiltIn("White Phosphor");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.22f;
    p.filters.scope.enabled = true;
    p.filters.scope.size = 0.70f;
    p.filters.scope.softness = 0.05f;
    p.filters.scope.shape = ScopeShape::Binocular;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.0f;
    p.filters.colorCorrection.brightness = 0.10f;
    p.filters.colorCorrection.contrast = 1.16f;
    p.filters.colorCorrection.gamma = 0.86f;
    p.filters.falseColour.enabled = true;
    p.filters.falseColour.palette = FalseColourPalette::WhitePhosphor;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.52f;
    p.filters.bloom.threshold = 0.54f;
    p.filters.bloom.radius = 0.44f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.24f;
    p.effects.noise.grainSize = 0.26f;
    p.effects.noise.speed = 0.92f;
    return p;
}

Preset FlashbangPreset() {
    // Everything the sensor had, gone at once. The threshold is low enough that ordinary
    // midtones bloom, which is what an overexposed sensor does - it is not only the highlights
    // that spill once the well is full.
    Preset p = MakeBuiltIn("Flashbang");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.35f;
    p.filters.colorCorrection.brightness = 0.16f;
    p.filters.colorCorrection.contrast = 1.20f;
    p.filters.colorCorrection.gamma = 0.72f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.90f;
    p.filters.bloom.threshold = 0.30f;
    p.filters.bloom.radius = 0.70f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.30f;
    p.filters.vignette.size = 0.72f;
    p.effects.flicker.enabled = true;
    p.effects.flicker.intensity = 0.45f;
    p.effects.flicker.speed = 0.90f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.16f;
    p.effects.noise.grainSize = 0.30f;
    p.effects.noise.speed = 0.95f;
    return p;
}

Preset EvidenceCamPreset() {
    // The clip that ends up in a case file: recorded cheap, compressed hard, kept forever.
    // Digital glitch rather than analog, because this footage was never on tape.
    Preset p = MakeBuiltIn("Evidence Cam");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.55f;
    p.filters.colorCorrection.contrast = 1.22f;
    p.filters.colorCorrection.gamma = 1.02f;
    p.filters.colorCorrection.posterize = 0.55f;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.14f;
    p.filters.scanlines.style = ScanlineStyle::Soft;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.38f;
    p.filters.vignette.size = 0.72f;
    p.effects.glitch.enabled = true;
    p.effects.glitch.style = GlitchStyle::Digital;
    p.effects.glitch.intensity = 0.35f;
    p.effects.glitch.frequency = 0.10f;
    p.effects.glitch.blockSize = 0.14f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.10f;
    p.effects.noise.grainSize = 0.55f;
    p.effects.noise.speed = 0.70f;
    return p;
}

Preset ShieldCamPreset() {
    // Filming through the polycarbonate of a ballistic shield: scratched, hazed, and slightly
    // out of true. The dirt does most of the work here - it is the only module that produces
    // the streaked haze of a surface that has been wiped a thousand times.
    Preset p = MakeBuiltIn("Shield Cam");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.30f;
    p.filters.distortion.shape = DistortionShape::Cylindrical;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.18f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Edge;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.72f;
    p.filters.colorCorrection.contrast = 1.10f;
    p.filters.colorCorrection.gamma = 1.06f;
    p.filters.lensDirt.enabled = true;
    p.filters.lensDirt.intensity = 0.62f;
    p.filters.lensDirt.density = 0.72f;
    p.filters.lensDirt.smear = 0.78f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.42f;
    p.filters.vignette.size = 0.68f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.30f;
    p.filters.bloom.threshold = 0.66f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.12f;
    p.effects.noise.grainSize = 0.40f;
    return p;
}

Preset TacLightPreset() {
    // A weapon light in a dark room. The blacks are crushed because there is genuinely nothing
    // there, and the cone blows out because a hand-held light is far brighter than anything
    // the sensor was metering for.
    Preset p = MakeBuiltIn("Tac Light");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.70f;
    p.filters.colorCorrection.brightness = -0.08f;
    p.filters.colorCorrection.contrast = 1.55f;
    p.filters.colorCorrection.gamma = 0.82f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.68f;
    p.filters.bloom.threshold = 0.52f;
    p.filters.bloom.radius = 0.55f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.72f;
    p.filters.vignette.size = 0.54f;
    p.filters.vignette.softness = 0.60f;
    p.filters.vignette.roundness = 0.85f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.26f;
    p.effects.noise.grainSize = 0.30f;
    p.effects.noise.speed = 0.88f;
    p.effects.noise.colorAmount = 0.30f;
    return p;
}

Preset GasMaskPreset() {
    // Two eyepieces and a lens that fogs. The binocular aperture supplies the eyepieces; the
    // dirt, turned up and smeared, supplies the condensation.
    Preset p = MakeBuiltIn("Gas Mask");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.34f;
    p.filters.scope.enabled = true;
    p.filters.scope.size = 0.74f;
    p.filters.scope.softness = 0.12f;
    p.filters.scope.shape = ScopeShape::Binocular;
    p.filters.lensDirt.enabled = true;
    p.filters.lensDirt.intensity = 0.44f;
    p.filters.lensDirt.density = 0.30f;
    p.filters.lensDirt.smear = 0.85f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.78f;
    p.filters.colorCorrection.contrast = 1.08f;
    p.filters.colorCorrection.gamma = 1.08f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.34f;
    p.filters.bloom.threshold = 0.60f;
    p.filters.bloom.radius = 0.50f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.10f;
    p.effects.noise.grainSize = 0.45f;
    return p;
}

Preset CsGasPreset() {
    // The room after the canister. Contrast collapses because the light is scattering off
    // particulate before it ever reaches the lens - it is a haze, not a blur.
    Preset p = MakeBuiltIn("CS Gas");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.58f;
    p.filters.colorCorrection.brightness = 0.14f;
    p.filters.colorCorrection.contrast = 0.72f;
    p.filters.colorCorrection.gamma = 1.10f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.55f;
    p.filters.bloom.threshold = 0.34f;
    p.filters.bloom.radius = 0.62f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.34f;
    p.filters.vignette.size = 0.70f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.22f;
    p.effects.noise.grainSize = 0.28f;
    p.effects.noise.speed = 0.85f;
    p.effects.flicker.enabled = true;
    p.effects.flicker.intensity = 0.10f;
    p.effects.flicker.speed = 0.40f;
    return p;
}

Preset SuspectCamPreset() {
    // A handheld phone or a fixed interview camera: mild lens, honest sensor, nothing tactical
    // about it. Useful as the plain end of this family.
    Preset p = MakeBuiltIn("Suspect Cam");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.24f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.88f;
    p.filters.colorCorrection.contrast = 1.12f;
    p.filters.colorCorrection.gamma = 1.02f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.32f;
    p.filters.vignette.size = 0.74f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.14f;
    p.effects.noise.grainSize = 0.38f;
    p.effects.noise.speed = 0.80f;
    p.effects.noise.colorAmount = 0.30f;
    p.effects.rollingShutter.enabled = true;
    p.effects.rollingShutter.intensity = 0.16f;
    p.effects.rollingShutter.speed = 0.55f;
    p.effects.jitter.enabled = true;
    p.effects.jitter.intensity = 0.07f;
    p.effects.jitter.speed = 0.45f;
    return p;
}

// --- Realistic body-worn footage ------------------------------------------------------------
//
// The family above is about the job; this one is about the hardware. These lean on the three
// artefacts that separate real body-worn footage from a game camera: an extremely wide lens,
// a sensor that shears, and glass nobody cleans.

Preset ActionCamPreset() {
    // The action camera bolted to a helmet or a chest rig. The lens is the whole personality:
    // wide enough that straight lines bow visibly, with the colour pushed the way these
    // cameras push it out of the box.
    Preset p = MakeBuiltIn("Action Cam");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.88f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.17f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 1.22f;
    p.filters.colorCorrection.contrast = 1.20f;
    p.filters.colorCorrection.gamma = 0.96f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.40f;
    p.filters.vignette.size = 0.70f;
    p.filters.vignette.roundness = 0.90f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.28f;
    p.filters.bloom.threshold = 0.72f;
    p.effects.rollingShutter.intensity = 0.26f;
    p.effects.rollingShutter.speed = 0.68f;
    p.effects.noise.intensity = 0.09f;
    p.effects.noise.grainSize = 0.40f;
    return p;
}

Preset DutyCamPreset() {
    // The issued body camera: a cooler cast than a consumer action cam, less saturation, and a
    // lens that is wide without being a fisheye.
    Preset p = MakeBuiltIn("Duty Cam");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.55f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.11f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.80f;
    p.filters.colorCorrection.contrast = 1.16f;
    p.filters.colorCorrection.gamma = 1.02f;
    p.filters.colorCorrection.tint[0] = 0.92f;
    p.filters.colorCorrection.tint[1] = 0.97f;
    p.filters.colorCorrection.tint[2] = 1.00f;
    p.filters.colorCorrection.tintAmount = 0.35f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.45f;
    p.filters.vignette.size = 0.68f;
    p.filters.lensDirt.enabled = true;
    p.filters.lensDirt.intensity = 0.20f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.24f;
    p.filters.bloom.threshold = 0.70f;
    p.effects.noise.intensity = 0.14f;
    p.effects.noise.grainSize = 0.34f;
    p.effects.noise.speed = 0.82f;
    p.effects.noise.colorAmount = 0.25f;
    p.effects.rollingShutter.intensity = 0.17f;
    p.effects.rollingShutter.speed = 0.52f;
    return p;
}

Preset LowLightSensorPreset() {
    // A small sensor pushed past where it should be. The blacks lift rather than crush - that
    // is the giveaway of a high ISO, and it is the opposite of what Tac Light does.
    Preset p = MakeBuiltIn("Low Light Sensor");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.68f;
    p.filters.colorCorrection.brightness = 0.10f;
    p.filters.colorCorrection.contrast = 0.88f;
    p.filters.colorCorrection.gamma = 1.14f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.38f;
    p.filters.bloom.threshold = 0.56f;
    p.filters.bloom.radius = 0.46f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.44f;
    p.filters.vignette.size = 0.66f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.38f;
    p.effects.noise.grainSize = 0.24f;
    p.effects.noise.speed = 0.92f;
    p.effects.noise.colorAmount = 0.55f;
    return p;
}

Preset CheapSensorPreset() {
    // Every corner cut at once: too few bits, a readout that shears, and channels that never
    // quite line up.
    Preset p = MakeBuiltIn("Cheap Sensor");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.48f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.26f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Prism;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.90f;
    p.filters.colorCorrection.contrast = 1.24f;
    p.filters.colorCorrection.posterize = 0.62f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.46f;
    p.filters.vignette.size = 0.66f;
    p.effects.rollingShutter.enabled = true;
    p.effects.rollingShutter.intensity = 0.42f;
    p.effects.rollingShutter.speed = 0.72f;
    p.effects.glitch.enabled = true;
    p.effects.glitch.style = GlitchStyle::Digital;
    p.effects.glitch.intensity = 0.30f;
    p.effects.glitch.frequency = 0.06f;
    p.effects.glitch.blockSize = 0.12f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.20f;
    p.effects.noise.grainSize = 0.30f;
    p.effects.noise.colorAmount = 0.40f;
    return p;
}

Preset DirtyLensPreset() {
    // The dirt on its own, with just enough lens and grain under it to have something to sit
    // on. This is the one to start from when tuning the module.
    Preset p = MakeBuiltIn("Dirty Lens");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.30f;
    p.filters.lensDirt.enabled = true;
    p.filters.lensDirt.intensity = 0.70f;
    p.filters.lensDirt.density = 0.62f;
    p.filters.lensDirt.smear = 0.58f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.36f;
    p.filters.bloom.threshold = 0.62f;
    p.filters.bloom.radius = 0.44f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.86f;
    p.filters.colorCorrection.contrast = 1.06f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.36f;
    p.filters.vignette.size = 0.70f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.10f;
    p.effects.noise.grainSize = 0.42f;
    return p;
}

Preset NightPatrolPreset() {
    // Street lighting, a cold sensor and nothing else. No night vision here - this is what an
    // ordinary camera does after dark, which is a different picture entirely.
    Preset p = MakeBuiltIn("Night Patrol");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.50f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.66f;
    p.filters.colorCorrection.brightness = -0.04f;
    p.filters.colorCorrection.contrast = 1.30f;
    p.filters.colorCorrection.gamma = 0.94f;
    p.filters.colorCorrection.tint[0] = 0.80f;
    p.filters.colorCorrection.tint[1] = 0.90f;
    p.filters.colorCorrection.tint[2] = 1.00f;
    p.filters.colorCorrection.tintAmount = 0.55f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.58f;
    p.filters.bloom.threshold = 0.54f;
    p.filters.bloom.radius = 0.52f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.58f;
    p.filters.vignette.size = 0.62f;
    p.filters.lensDirt.enabled = true;
    p.filters.lensDirt.intensity = 0.22f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.30f;
    p.effects.noise.grainSize = 0.26f;
    p.effects.noise.speed = 0.90f;
    p.effects.noise.colorAmount = 0.45f;
    return p;
}

Preset GoProBodycamPreset() {
    // The action camera with its own lens circle showing. What separates this from Action Cam
    // is the crop: an ultra-wide lens does not project a circle big enough to fill a 16:9
    // sensor, so the four corners fall outside the image it forms and read as black. Every
    // other module here is in service of that one fact - this is a camera looking through a
    // small piece of glass, not a picture with a filter on it.
    Preset p = MakeBuiltIn("GoPro Bodycam");

    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.85f;
    p.filters.distortion.shape = DistortionShape::Radial;

    // The porthole, and the reason this preset exists. `size` is a fraction of the distance to
    // the corner, so anything above roughly 0.87 on a 16:9 overlay still lets the picture touch
    // all four edges at their midpoints and cuts only the corners - which is exactly what the
    // reference footage shows. Below that it would close into a circle in the middle of a black
    // frame, which is a peephole and a different preset. The threshold moves with the aspect
    // ratio (0.80 at 4:3, 0.92 at 21:9) and 0.94 clears all three.
    p.filters.scope.enabled = true;
    p.filters.scope.shape = ScopeShape::Circle;
    p.filters.scope.size = 0.94f;
    p.filters.scope.softness = 0.03f;
    p.filters.scope.magnification = 1.0f;
    p.filters.scope.reticle = 0.0f;

    // Soft corners and a sharpened middle, which is the pair that reads as a small sensor
    // behind a wide lens. Either one alone reads as a mistake.
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.45f;
    p.filters.lensSoftness.center = 0.40f;
    p.filters.sharpen.enabled = true;
    p.filters.sharpen.intensity = 0.52f;
    p.filters.sharpen.radius = 0.35f;

    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.16f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;

    // A low threshold on purpose: the veiling glare in this footage is not a halo around a
    // lamp, it is the whole picture going slightly milky whenever a window is in frame.
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.32f;
    p.filters.bloom.threshold = 0.62f;
    p.filters.bloom.radius = 0.36f;

    // Desaturated, cold, and contrastier than it was. With the grain gone the picture has
    // nothing breaking up its flat areas, so it needs the tonal separation to stop reading as a
    // clean render with a lens bent over it. Blacks sit down rather than lifted: a tactical
    // interior is lit by windows and practicals with a lot of nothing in between.
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.78f;
    p.filters.colorCorrection.brightness = 0.01f;
    p.filters.colorCorrection.contrast = 1.20f;
    p.filters.colorCorrection.gamma = 0.98f;
    p.filters.colorCorrection.tint[0] = 0.92f;
    p.filters.colorCorrection.tint[1] = 0.96f;
    p.filters.colorCorrection.tint[2] = 1.00f;
    p.filters.colorCorrection.tintAmount = 0.38f;

    // Inside the aperture, not instead of it: the picture is already dimming well before it
    // reaches the black corners.
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.52f;
    p.filters.vignette.size = 0.70f;
    p.filters.vignette.softness = 0.66f;
    p.filters.vignette.roundness = 0.85f;

    p.filters.lensDirt.enabled = true;
    p.filters.lensDirt.intensity = 0.20f;
    p.filters.lensDirt.density = 0.40f;
    p.filters.lensDirt.smear = 0.50f;

    // Grain and shear are both off. They are the two things that read as an effect rather
    // than as footage, and without them the optics and the grade carry the look on their own.
    // The values stay dialled in so either can be switched back on from the panel.
    p.effects.noise.intensity = 0.14f;
    p.effects.noise.grainSize = 0.34f;
    p.effects.noise.speed = 0.85f;
    p.effects.noise.colorAmount = 0.25f;

    p.effects.rollingShutter.intensity = 0.18f;
    p.effects.rollingShutter.speed = 0.55f;

    // Jitter stays off too. On an overlay that is up for hours a wandering frame wears thin
    // fast, and Helmet Cam is already the one for a camera on someone who is moving.
    return p;
}

// --- Recon and drone --------------------------------------------------------------------------
//
// Views from something that is not a person: a downlink, a thermal sight, an optic on a
// tripod. The scan sweep and the digital glitch belong here more than anywhere else.

Preset DroneFeedPreset() {
    // A compressed downlink. Digital glitch, not analog, because there is no tape anywhere in
    // this chain - when it fails it loses blocks, and the blocks that survive are untouched.
    Preset p = MakeBuiltIn("Drone Feed");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.72f;
    p.filters.colorCorrection.contrast = 1.18f;
    p.filters.colorCorrection.posterize = 0.42f;
    p.filters.colorCorrection.tint[0] = 0.82f;
    p.filters.colorCorrection.tint[1] = 0.96f;
    p.filters.colorCorrection.tint[2] = 1.00f;
    p.filters.colorCorrection.tintAmount = 0.40f;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.12f;
    p.filters.scanlines.style = ScanlineStyle::Soft;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.38f;
    p.filters.vignette.size = 0.72f;
    p.effects.glitch.enabled = true;
    p.effects.glitch.style = GlitchStyle::Digital;
    p.effects.glitch.intensity = 0.42f;
    p.effects.glitch.frequency = 0.12f;
    p.effects.glitch.blockSize = 0.16f;
    p.effects.scanSweep.enabled = true;
    p.effects.scanSweep.intensity = 0.22f;
    p.effects.scanSweep.speed = 0.30f;
    p.effects.scanSweep.width = 0.20f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.10f;
    p.effects.noise.grainSize = 0.50f;
    return p;
}

Preset UavThermalPreset() {
    // White hot from altitude: the plain palette, a tight aperture and the sweep of a sensor
    // working its way across the ground.
    Preset p = MakeBuiltIn("UAV Thermal");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.0f;
    p.filters.colorCorrection.contrast = 1.45f;
    p.filters.colorCorrection.gamma = 0.82f;
    p.filters.falseColour.enabled = true;
    p.filters.falseColour.palette = FalseColourPalette::WhiteHot;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.55f;
    p.filters.vignette.size = 0.62f;
    p.filters.vignette.roundness = 0.85f;
    p.effects.scanSweep.enabled = true;
    p.effects.scanSweep.intensity = 0.26f;
    p.effects.scanSweep.speed = 0.25f;
    p.effects.scanSweep.width = 0.14f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.14f;
    p.effects.noise.grainSize = 0.55f;
    p.effects.noise.speed = 0.45f;
    return p;
}

Preset BlackHotPreset() {
    // The same sensor with the polarity flipped. Operators switch to black hot because a hot
    // figure against a cool background is easier to pick out as a dark silhouette.
    Preset p = MakeBuiltIn("Black Hot");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.0f;
    p.filters.colorCorrection.contrast = 1.50f;
    p.filters.colorCorrection.gamma = 0.85f;
    p.filters.falseColour.enabled = true;
    p.filters.falseColour.palette = FalseColourPalette::BlackHot;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.42f;
    p.filters.vignette.size = 0.68f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.12f;
    p.effects.noise.grainSize = 0.55f;
    p.effects.noise.speed = 0.45f;
    return p;
}

Preset IronbowPreset() {
    // The palette on its own, quantised into visible steps the way a handheld thermal monitor
    // shows it.
    Preset p = MakeBuiltIn("Ironbow");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.0f;
    p.filters.colorCorrection.contrast = 1.35f;
    p.filters.colorCorrection.gamma = 0.88f;
    p.filters.falseColour.enabled = true;
    p.filters.falseColour.palette = FalseColourPalette::Ironbow;
    p.filters.falseColour.levels = 0.35f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.40f;
    p.filters.vignette.size = 0.70f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.10f;
    p.effects.noise.grainSize = 0.60f;
    p.effects.noise.speed = 0.40f;
    return p;
}

Preset ReconOpticPreset() {
    // A spotting scope with a sensor behind it: the optic supplies the aperture and the
    // magnification, the edge glow supplies the part that is a computer rather than glass.
    Preset p = MakeBuiltIn("Recon Optic");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.42f;
    p.filters.scope.enabled = true;
    p.filters.scope.size = 0.64f;
    p.filters.scope.softness = 0.05f;
    p.filters.scope.magnification = 1.80f;
    p.filters.scope.reticle = 0.45f;
    p.filters.scope.shape = ScopeShape::Circle;
    p.filters.edgeGlow.enabled = true;
    p.filters.edgeGlow.intensity = 0.28f;
    p.filters.edgeGlow.width = 0.40f;
    p.filters.edgeGlow.tint[0] = 0.45f;
    p.filters.edgeGlow.tint[1] = 0.90f;
    p.filters.edgeGlow.tint[2] = 0.85f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.82f;
    p.filters.colorCorrection.contrast = 1.14f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.42f;
    p.filters.vignette.size = 0.54f;
    p.filters.vignette.roundness = 1.00f;
    p.effects.jitter.enabled = true;
    p.effects.jitter.intensity = 0.05f;
    p.effects.jitter.speed = 0.30f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.09f;
    p.effects.noise.grainSize = 0.45f;
    return p;
}

Preset AzureReconPreset() {
    // No instrument at all: the desaturated, cool grade that open-world military games settle
    // into. Useful precisely because it does nothing gimmicky.
    Preset p = MakeBuiltIn("Azure Recon");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.74f;
    p.filters.colorCorrection.contrast = 1.16f;
    p.filters.colorCorrection.gamma = 0.98f;
    p.filters.colorCorrection.tint[0] = 0.86f;
    p.filters.colorCorrection.tint[1] = 0.96f;
    p.filters.colorCorrection.tint[2] = 1.00f;
    p.filters.colorCorrection.tintAmount = 0.45f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.34f;
    p.filters.vignette.size = 0.74f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.20f;
    p.filters.bloom.threshold = 0.76f;
    return p;
}

Preset SyncShotPreset() {
    // Targets tagged and outlined. The image drops back so the outlines carry it, which is how
    // a tagging overlay reads in the games that do it.
    Preset p = MakeBuiltIn("Sync Shot");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.42f;
    p.filters.colorCorrection.brightness = -0.06f;
    p.filters.colorCorrection.contrast = 1.24f;
    p.filters.edgeGlow.enabled = true;
    p.filters.edgeGlow.intensity = 0.72f;
    p.filters.edgeGlow.width = 0.55f;
    p.filters.edgeGlow.tint[0] = 0.20f;
    p.filters.edgeGlow.tint[1] = 0.85f;
    p.filters.edgeGlow.tint[2] = 1.00f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.44f;
    p.filters.vignette.size = 0.68f;
    p.effects.scanSweep.enabled = true;
    p.effects.scanSweep.intensity = 0.18f;
    p.effects.scanSweep.speed = 0.55f;
    p.effects.scanSweep.width = 0.10f;
    return p;
}

// --- Sensor and augmented views ---------------------------------------------------------------
//
// The near-future end of the tactical family: outlines, palettes and sweeps that are all
// computer rather than glass. The edge glow does most of the work in this group, which is what
// it was added for.

Preset CrossComPreset() {
    // The cyan sensor HUD: a palette that saturates blue first, an outline on everything, and
    // a sweep to say the instrument is alive.
    Preset p = MakeBuiltIn("Cross-Com");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.0f;
    p.filters.colorCorrection.contrast = 1.30f;
    p.filters.colorCorrection.gamma = 0.90f;
    p.filters.falseColour.enabled = true;
    p.filters.falseColour.palette = FalseColourPalette::CrossCom;
    p.filters.falseColour.intensity = 0.90f;
    p.filters.edgeGlow.enabled = true;
    p.filters.edgeGlow.intensity = 0.50f;
    p.filters.edgeGlow.width = 0.42f;
    p.filters.edgeGlow.tint[0] = 0.40f;
    p.filters.edgeGlow.tint[1] = 0.95f;
    p.filters.edgeGlow.tint[2] = 1.00f;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.16f;
    p.filters.scanlines.style = ScanlineStyle::Soft;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.48f;
    p.filters.vignette.size = 0.66f;
    p.filters.vignette.roundness = 0.75f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.34f;
    p.filters.bloom.threshold = 0.60f;
    p.effects.scanSweep.enabled = true;
    p.effects.scanSweep.intensity = 0.24f;
    p.effects.scanSweep.speed = 0.40f;
    p.effects.scanSweep.width = 0.16f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.10f;
    p.effects.noise.grainSize = 0.40f;
    return p;
}

Preset MagneticViewPreset() {
    // The sensor sweep that shows shapes through the scene rather than the scene itself:
    // polarity inverted, tonal range crushed to a handful of steps, and everything outlined.
    Preset p = MakeBuiltIn("Magnetic View");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.0f;
    p.filters.colorCorrection.contrast = 1.60f;
    p.filters.colorCorrection.gamma = 0.82f;
    p.filters.colorCorrection.tint[0] = 0.55f;
    p.filters.colorCorrection.tint[1] = 0.80f;
    p.filters.colorCorrection.tint[2] = 1.00f;
    p.filters.colorCorrection.tintAmount = 0.80f;
    p.filters.colorCorrection.posterize = 0.70f;
    p.filters.edgeGlow.enabled = true;
    p.filters.edgeGlow.intensity = 0.85f;
    p.filters.edgeGlow.width = 0.62f;
    p.filters.edgeGlow.tint[0] = 0.30f;
    p.filters.edgeGlow.tint[1] = 0.70f;
    p.filters.edgeGlow.tint[2] = 1.00f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.60f;
    p.filters.vignette.size = 0.60f;
    p.filters.vignette.roundness = 0.90f;
    p.effects.scanSweep.enabled = true;
    p.effects.scanSweep.intensity = 0.34f;
    p.effects.scanSweep.speed = 0.48f;
    p.effects.scanSweep.width = 0.12f;
    return p;
}

Preset OpticalCamoPreset() {
    // A cloaking field: the scene is still there, bent. This is the only preset built around
    // the shimmer, and it is deliberately gentle - past about a third the ripple stops reading
    // as refraction and starts reading as a broken shader.
    Preset p = MakeBuiltIn("Optical Camo");
    p.effects.shimmer.enabled = true;
    p.effects.shimmer.intensity = 0.30f;
    p.effects.shimmer.speed = 0.42f;
    p.effects.shimmer.scale = 0.60f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.16f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Prism;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.78f;
    p.filters.colorCorrection.contrast = 1.08f;
    p.filters.colorCorrection.tint[0] = 0.88f;
    p.filters.colorCorrection.tint[1] = 0.98f;
    p.filters.colorCorrection.tint[2] = 1.00f;
    p.filters.colorCorrection.tintAmount = 0.30f;
    p.filters.edgeGlow.enabled = true;
    p.filters.edgeGlow.intensity = 0.22f;
    p.filters.edgeGlow.width = 0.30f;
    p.filters.edgeGlow.tint[0] = 0.55f;
    p.filters.edgeGlow.tint[1] = 0.95f;
    p.filters.edgeGlow.tint[2] = 1.00f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.24f;
    p.filters.bloom.threshold = 0.70f;
    return p;
}

Preset WarhoundFeedPreset() {
    // The camera on a walking machine: the downlink of Drone Feed, plus the dirt and the shear
    // of something that is physically stomping around.
    Preset p = MakeBuiltIn("Warhound Feed");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.60f;
    p.filters.colorCorrection.contrast = 1.26f;
    p.filters.colorCorrection.gamma = 0.94f;
    p.filters.colorCorrection.posterize = 0.35f;
    p.filters.lensDirt.enabled = true;
    p.filters.lensDirt.intensity = 0.34f;
    p.filters.lensDirt.density = 0.55f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.50f;
    p.filters.vignette.size = 0.64f;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.10f;
    p.filters.scanlines.style = ScanlineStyle::Soft;
    p.filters.scanlines.spacing = 4.0f;
    p.effects.glitch.enabled = true;
    p.effects.glitch.style = GlitchStyle::Digital;
    p.effects.glitch.intensity = 0.34f;
    p.effects.glitch.frequency = 0.09f;
    p.effects.glitch.blockSize = 0.18f;
    p.effects.rollingShutter.enabled = true;
    p.effects.rollingShutter.intensity = 0.20f;
    p.effects.rollingShutter.speed = 0.60f;
    p.effects.scanSweep.enabled = true;
    p.effects.scanSweep.intensity = 0.16f;
    p.effects.scanSweep.speed = 0.35f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.14f;
    p.effects.noise.grainSize = 0.38f;
    return p;
}

Preset EmpBurstPreset() {
    // The instrument taking a hit. Frequency high and intensity high together, which is the
    // one combination the glitch's two independent controls are usually kept away from.
    Preset p = MakeBuiltIn("EMP Burst");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.50f;
    p.filters.colorCorrection.contrast = 1.34f;
    p.filters.colorCorrection.tint[0] = 0.72f;
    p.filters.colorCorrection.tint[1] = 0.90f;
    p.filters.colorCorrection.tint[2] = 1.00f;
    p.filters.colorCorrection.tintAmount = 0.60f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.55f;
    p.filters.bloom.threshold = 0.50f;
    p.filters.bloom.radius = 0.55f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.52f;
    p.filters.vignette.size = 0.62f;
    p.effects.glitch.enabled = true;
    p.effects.glitch.style = GlitchStyle::Digital;
    p.effects.glitch.intensity = 0.62f;
    p.effects.glitch.frequency = 0.45f;
    p.effects.glitch.blockSize = 0.20f;
    p.effects.flicker.enabled = true;
    p.effects.flicker.intensity = 0.42f;
    p.effects.flicker.speed = 0.88f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.22f;
    p.effects.noise.grainSize = 0.28f;
    p.effects.noise.colorAmount = 0.50f;
    return p;
}

Preset GhostModePreset() {
    // Dark, cold and quiet: the look a squad-tactics game settles into at night when nothing
    // is happening yet. The outline is barely there on purpose.
    Preset p = MakeBuiltIn("Ghost Mode");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.48f;
    p.filters.colorCorrection.brightness = -0.05f;
    p.filters.colorCorrection.contrast = 1.28f;
    p.filters.colorCorrection.gamma = 0.92f;
    p.filters.colorCorrection.tint[0] = 0.70f;
    p.filters.colorCorrection.tint[1] = 0.94f;
    p.filters.colorCorrection.tint[2] = 0.98f;
    p.filters.colorCorrection.tintAmount = 0.55f;
    p.filters.edgeGlow.enabled = true;
    p.filters.edgeGlow.intensity = 0.18f;
    p.filters.edgeGlow.width = 0.35f;
    p.filters.edgeGlow.tint[0] = 0.40f;
    p.filters.edgeGlow.tint[1] = 0.90f;
    p.filters.edgeGlow.tint[2] = 0.80f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.32f;
    p.filters.bloom.threshold = 0.62f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.62f;
    p.filters.vignette.size = 0.60f;
    p.filters.vignette.roundness = 0.70f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.18f;
    p.effects.noise.grainSize = 0.32f;
    p.effects.noise.speed = 0.85f;
    return p;
}

// --- More fisheye ----------------------------------------------------------------------------
//
// The first fisheye family varied one thing: how much. These vary what kind of lens it is,
// which is mostly a question of what comes with the bulge - how soft the corners go, how the
// colour is graded, and whether the image is a rectangle at all.
//
// The lens softness of ADR-0011 does most of the separating here. Without it every one of these
// would be the same geometric warp of a uniformly sharp image, and the only difference between
// a skate camera and a drone would be the colour.

Preset CircularFisheyePreset() {
    // A true circular fisheye: the image circle is smaller than the sensor, so the corners are
    // simply not exposed. The scope aperture supplies that - it is the one module that paints
    // solid rather than fading, which is what the unexposed part of a frame looks like.
    Preset p = MakeBuiltIn("Circular Fisheye");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 1.00f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.scope.enabled = true;
    p.filters.scope.size = 0.66f;
    p.filters.scope.softness = 0.02f;
    p.filters.scope.magnification = 1.0f;
    p.filters.scope.reticle = 0.0f;
    p.filters.scope.shape = ScopeShape::Circle;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.55f;
    p.filters.lensSoftness.center = 0.40f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.18f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.40f;
    p.filters.vignette.size = 0.55f;
    p.filters.vignette.roundness = 1.00f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 1.10f;
    p.filters.colorCorrection.saturation = 1.05f;
    return p;
}

Preset FisheyeSoftPreset() {
    // The plainest demonstration of what a lens does that a warp does not: a moderate bulge
    // with the corners genuinely losing resolution. Start here when tuning Lens Softness.
    Preset p = MakeBuiltIn("Fisheye Soft");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.60f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.75f;
    p.filters.lensSoftness.center = 0.30f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.14f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.34f;
    p.filters.vignette.size = 0.70f;
    p.filters.vignette.roundness = 0.85f;
    return p;
}

Preset DashcamPreset() {
    // Suction-cupped to a windscreen: wide, cold, compressed, and always slightly dirty. The
    // posterize is the bitrate, not the sensor.
    Preset p = MakeBuiltIn("Dashcam");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.58f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.45f;
    p.filters.lensSoftness.center = 0.50f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.15f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.84f;
    p.filters.colorCorrection.contrast = 1.18f;
    p.filters.colorCorrection.posterize = 0.38f;
    p.filters.colorCorrection.tint[0] = 0.88f;
    p.filters.colorCorrection.tint[1] = 0.96f;
    p.filters.colorCorrection.tint[2] = 1.00f;
    p.filters.colorCorrection.tintAmount = 0.35f;
    p.filters.lensDirt.enabled = true;
    p.filters.lensDirt.intensity = 0.26f;
    p.filters.lensDirt.density = 0.40f;
    p.filters.lensDirt.smear = 0.70f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.40f;
    p.filters.vignette.size = 0.72f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.11f;
    p.effects.noise.grainSize = 0.45f;
    return p;
}

Preset DroneFpvPreset() {
    // An FPV camera: about as wide as a lens gets before it goes circular, colour pushed hard,
    // and a rolling shutter that never settles because the airframe never does.
    Preset p = MakeBuiltIn("Drone FPV");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.92f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.50f;
    p.filters.lensSoftness.center = 0.42f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.20f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 1.30f;
    p.filters.colorCorrection.contrast = 1.26f;
    p.filters.colorCorrection.gamma = 0.94f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.44f;
    p.filters.vignette.size = 0.68f;
    p.filters.vignette.roundness = 0.90f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.30f;
    p.filters.bloom.threshold = 0.70f;
    p.effects.rollingShutter.enabled = true;
    p.effects.rollingShutter.intensity = 0.30f;
    p.effects.rollingShutter.speed = 0.80f;
    p.effects.jitter.enabled = true;
    p.effects.jitter.intensity = 0.06f;
    p.effects.jitter.speed = 0.75f;
    return p;
}

Preset SkateCamPreset() {
    // The camcorder-and-fisheye-adapter look: a warm tape-era grade, a hard bulge from a screw-on
    // element that was never as good as the lens behind it, and corners to match.
    Preset p = MakeBuiltIn("Skate Cam");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.85f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.80f;
    p.filters.lensSoftness.center = 0.34f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.22f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Barrel;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 1.12f;
    p.filters.colorCorrection.contrast = 1.14f;
    p.filters.colorCorrection.gamma = 1.04f;
    p.filters.colorCorrection.tint[0] = 1.00f;
    p.filters.colorCorrection.tint[1] = 0.95f;
    p.filters.colorCorrection.tint[2] = 0.86f;
    p.filters.colorCorrection.tintAmount = 0.40f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.46f;
    p.filters.vignette.size = 0.66f;
    p.filters.vignette.roundness = 0.90f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.16f;
    p.effects.noise.grainSize = 0.40f;
    p.effects.noise.speed = 0.70f;
    p.effects.noise.colorAmount = 0.20f;
    return p;
}

Preset BubbleLensPreset() {
    // Shooting through a glass sphere. A ball is not a corrected lens: it splits colour badly
    // and everything but the middle goes to mush, so the prism mode and a very early softness
    // fall-off do more of the work here than the bulge does.
    Preset p = MakeBuiltIn("Bubble Lens");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 1.00f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 1.00f;
    p.filters.lensSoftness.center = 0.18f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.42f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Prism;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.44f;
    p.filters.bloom.threshold = 0.58f;
    p.filters.bloom.radius = 0.50f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.52f;
    p.filters.vignette.size = 0.58f;
    p.filters.vignette.roundness = 1.00f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 1.15f;
    p.filters.colorCorrection.contrast = 1.06f;
    return p;
}

Preset BugEyePreset() {
    // Corner shape at full: the middle stays flat and readable while the outer quarter pulls
    // violently. It is the least photographic member of the family and the most useful when the
    // point is that something is looking at you.
    Preset p = MakeBuiltIn("Bug Eye");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 1.00f;
    p.filters.distortion.shape = DistortionShape::Corner;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.85f;
    p.filters.lensSoftness.center = 0.55f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.26f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Edge;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.55f;
    p.filters.vignette.size = 0.62f;
    p.filters.vignette.roundness = 0.95f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 1.08f;
    p.filters.colorCorrection.contrast = 1.12f;
    return p;
}

Preset CockpitGlassPreset() {
    // A curved instrument cover: the CRT shape bends each axis separately, which is what a
    // cylindrical canopy does, and the green is the anti-reflective coating rather than a tube.
    Preset p = MakeBuiltIn("Cockpit Glass");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.52f;
    p.filters.distortion.shape = DistortionShape::Crt;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.38f;
    p.filters.lensSoftness.center = 0.55f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.12f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Edge;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.90f;
    p.filters.colorCorrection.contrast = 1.10f;
    p.filters.colorCorrection.tint[0] = 0.88f;
    p.filters.colorCorrection.tint[1] = 1.00f;
    p.filters.colorCorrection.tint[2] = 0.92f;
    p.filters.colorCorrection.tintAmount = 0.30f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.26f;
    p.filters.bloom.threshold = 0.72f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.38f;
    p.filters.vignette.size = 0.72f;
    p.filters.lensDirt.enabled = true;
    p.filters.lensDirt.intensity = 0.16f;
    p.filters.lensDirt.smear = 0.60f;
    return p;
}

Preset PanoramicPreset() {
    // Cylindrical at full: only the horizontal bends, so verticals stay vertical. That is what
    // a panorama projection does and what a spherical fisheye conspicuously does not.
    Preset p = MakeBuiltIn("Panoramic");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 1.00f;
    p.filters.distortion.shape = DistortionShape::Cylindrical;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.40f;
    p.filters.lensSoftness.center = 0.50f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.12f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Horizontal;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.30f;
    p.filters.vignette.size = 0.76f;
    p.filters.vignette.roundness = 0.20f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 1.08f;
    p.filters.colorCorrection.saturation = 1.04f;
    return p;
}

Preset DoorbellCamPreset() {
    // The modern doorbell: absurdly wide, tone-mapped flat so the porch and the street are both
    // visible, and cold. Low contrast is the giveaway - it is what HDR on a tiny sensor costs.
    Preset p = MakeBuiltIn("Doorbell Cam");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.96f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.60f;
    p.filters.lensSoftness.center = 0.38f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.72f;
    p.filters.colorCorrection.brightness = 0.06f;
    p.filters.colorCorrection.contrast = 0.82f;
    p.filters.colorCorrection.gamma = 1.10f;
    p.filters.colorCorrection.tint[0] = 0.86f;
    p.filters.colorCorrection.tint[1] = 0.95f;
    p.filters.colorCorrection.tint[2] = 1.00f;
    p.filters.colorCorrection.tintAmount = 0.40f;
    p.filters.colorCorrection.posterize = 0.30f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.48f;
    p.filters.vignette.size = 0.64f;
    p.filters.vignette.roundness = 0.95f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.18f;
    p.effects.noise.grainSize = 0.32f;
    p.effects.noise.colorAmount = 0.35f;
    return p;
}

// --- More anti-fisheye -----------------------------------------------------------------------
//
// Worth adding now rather than earlier: until ADR-0010 the negative half opened a transparent
// border, so a preset that leaned on it was shipping a hole.

Preset UltrawideCorrectPreset() {
    // Undoing a very wide lens. Nothing else is on: the point is to see the geometry alone, and
    // the crop that comes with it - a pinch has to take the corners from somewhere.
    Preset p = MakeBuiltIn("Ultrawide Correct");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = -0.85f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 1.04f;
    return p;
}

Preset PincushionTubePreset() {
    // A monitor with its geometry mis-set the other way: the CRT shape's negative half, which
    // pinches each axis by the square of the other. Badly converged sets really did look like
    // this, and it is the one distortion that reads as a fault rather than as a lens.
    Preset p = MakeBuiltIn("Pincushion Tube");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = -0.70f;
    p.filters.distortion.shape = DistortionShape::Crt;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.22f;
    p.filters.scanlines.style = ScanlineStyle::Soft;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.14f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Edge;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.30f;
    p.filters.vignette.size = 0.76f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 1.08f;
    p.filters.colorCorrection.saturation = 0.96f;
    return p;
}

// --- More entry-team work --------------------------------------------------------------------
//
// The tactical group already covers the cameras. These cover the situations: the corridor
// before the door, the seconds after the bang, the equipment that is not a camera at all.

Preset UnderDoorCamPreset() {
    // The pole camera slid under a door. A tiny sensor behind a mirror and a lens far too wide
    // for it: circular image, no colour worth speaking of, and detail that gives up almost
    // immediately outside the middle.
    Preset p = MakeBuiltIn("Under Door Cam");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.95f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.scope.enabled = true;
    p.filters.scope.size = 0.72f;
    p.filters.scope.softness = 0.06f;
    p.filters.scope.shape = ScopeShape::Circle;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.90f;
    p.filters.lensSoftness.center = 0.22f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.30f;
    p.filters.colorCorrection.contrast = 1.24f;
    p.filters.colorCorrection.gamma = 0.94f;
    p.filters.colorCorrection.posterize = 0.55f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.50f;
    p.filters.vignette.size = 0.60f;
    p.filters.vignette.roundness = 1.00f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.26f;
    p.effects.noise.grainSize = 0.30f;
    p.effects.noise.speed = 0.90f;
    return p;
}

Preset StackUpPreset() {
    // The corridor before the door goes. Nothing is happening yet, which is the point: cold,
    // underexposed, and grainy because the camera is trying to find light that is not there.
    Preset p = MakeBuiltIn("Stack Up");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.52f;
    p.filters.colorCorrection.brightness = -0.07f;
    p.filters.colorCorrection.contrast = 1.34f;
    p.filters.colorCorrection.gamma = 0.90f;
    p.filters.colorCorrection.tint[0] = 0.80f;
    p.filters.colorCorrection.tint[1] = 0.92f;
    p.filters.colorCorrection.tint[2] = 1.00f;
    p.filters.colorCorrection.tintAmount = 0.50f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.68f;
    p.filters.vignette.size = 0.58f;
    p.filters.vignette.softness = 0.58f;
    p.filters.vignette.roundness = 0.70f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.34f;
    p.filters.bloom.threshold = 0.64f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.30f;
    p.effects.noise.grainSize = 0.28f;
    p.effects.noise.speed = 0.88f;
    p.effects.noise.colorAmount = 0.40f;
    return p;
}

Preset ConcussionPreset() {
    // The seconds after the bang. The shimmer is doing something it was not built for - it is
    // meant to be hot air - but a slow, large-scale warp is exactly what a rattled inner ear
    // does to a picture, and no other module bends the image without also tearing it.
    Preset p = MakeBuiltIn("Concussion");
    p.effects.shimmer.enabled = true;
    p.effects.shimmer.intensity = 0.38f;
    p.effects.shimmer.speed = 0.20f;
    p.effects.shimmer.scale = 0.90f;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.70f;
    p.filters.lensSoftness.center = 0.25f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.42f;
    p.filters.colorCorrection.brightness = 0.10f;
    p.filters.colorCorrection.contrast = 0.86f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.72f;
    p.filters.bloom.threshold = 0.40f;
    p.filters.bloom.radius = 0.62f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.56f;
    p.filters.vignette.size = 0.60f;
    p.effects.flicker.enabled = true;
    p.effects.flicker.intensity = 0.30f;
    p.effects.flicker.speed = 0.25f;
    return p;
}

Preset IrIlluminatorPreset() {
    // Night vision with the infrared lamp on. The lamp is a narrow cone the tube can see and
    // nothing else can, so the middle is blown out and everything past the cone falls to
    // nothing - a much harder vignette than the ambient-light Night Vision preset wants.
    Preset p = MakeBuiltIn("IR Illuminator");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.30f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.0f;
    p.filters.colorCorrection.brightness = 0.18f;
    p.filters.colorCorrection.contrast = 1.40f;
    p.filters.colorCorrection.gamma = 0.72f;
    p.filters.falseColour.enabled = true;
    p.filters.falseColour.palette = FalseColourPalette::Phosphor;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.78f;
    p.filters.bloom.threshold = 0.44f;
    p.filters.bloom.radius = 0.52f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 1.00f;
    p.filters.vignette.size = 0.40f;
    p.filters.vignette.softness = 0.30f;
    p.filters.vignette.roundness = 1.00f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.34f;
    p.effects.noise.grainSize = 0.24f;
    p.effects.noise.speed = 0.95f;
    return p;
}

Preset DispatchFeedPreset() {
    // The monitor wall: several cameras multiplexed down one cheap link, which is why the
    // resolution and the bitrate are both worse than any single camera in the building.
    Preset p = MakeBuiltIn("Dispatch Feed");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.48f;
    p.filters.colorCorrection.contrast = 1.20f;
    p.filters.colorCorrection.posterize = 0.68f;
    p.filters.colorCorrection.tint[0] = 0.88f;
    p.filters.colorCorrection.tint[1] = 0.98f;
    p.filters.colorCorrection.tint[2] = 1.00f;
    p.filters.colorCorrection.tintAmount = 0.30f;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.26f;
    p.filters.scanlines.style = ScanlineStyle::Hard;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.55f;
    p.filters.lensSoftness.center = 0.15f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.42f;
    p.filters.vignette.size = 0.70f;
    p.effects.glitch.enabled = true;
    p.effects.glitch.style = GlitchStyle::Digital;
    p.effects.glitch.intensity = 0.30f;
    p.effects.glitch.frequency = 0.08f;
    p.effects.glitch.blockSize = 0.15f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.12f;
    p.effects.noise.grainSize = 0.52f;
    return p;
}

Preset InterviewRoomPreset() {
    // Institutional fluorescent: flat, faintly green, and pulsing at a rate you notice only
    // once someone points it out. The opposite of every other preset in this group - nothing
    // here is tactical, and that is what makes it useful next to them.
    Preset p = MakeBuiltIn("Interview Room");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.78f;
    p.filters.colorCorrection.brightness = 0.05f;
    p.filters.colorCorrection.contrast = 0.88f;
    p.filters.colorCorrection.gamma = 1.06f;
    p.filters.colorCorrection.tint[0] = 0.93f;
    p.filters.colorCorrection.tint[1] = 1.00f;
    p.filters.colorCorrection.tint[2] = 0.90f;
    p.filters.colorCorrection.tintAmount = 0.45f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.24f;
    p.filters.vignette.size = 0.78f;
    p.effects.flicker.enabled = true;
    p.effects.flicker.intensity = 0.14f;
    p.effects.flicker.speed = 0.95f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.08f;
    p.effects.noise.grainSize = 0.45f;
    return p;
}

Preset TaserArcPreset() {
    // The arc. Frequency high and duration short is what an electrical discharge looks like on
    // camera, so this is one of the few places the glitch's two independent controls are pushed
    // apart rather than together: it fires often, and each burst is brief and blue.
    Preset p = MakeBuiltIn("Taser Arc");
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.60f;
    p.filters.colorCorrection.contrast = 1.30f;
    p.filters.colorCorrection.tint[0] = 0.72f;
    p.filters.colorCorrection.tint[1] = 0.88f;
    p.filters.colorCorrection.tint[2] = 1.00f;
    p.filters.colorCorrection.tintAmount = 0.55f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.80f;
    p.filters.bloom.threshold = 0.46f;
    p.filters.bloom.radius = 0.48f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.50f;
    p.filters.vignette.size = 0.64f;
    p.effects.flicker.enabled = true;
    p.effects.flicker.intensity = 0.60f;
    p.effects.flicker.speed = 1.00f;
    p.effects.glitch.enabled = true;
    p.effects.glitch.style = GlitchStyle::Analog;
    p.effects.glitch.intensity = 0.28f;
    p.effects.glitch.frequency = 0.40f;
    p.effects.glitch.blockSize = 0.06f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.18f;
    p.effects.noise.grainSize = 0.26f;
    return p;
}

Preset RedDotPreset() {
    // Looking through a red dot sight. Unlike the sniper optic there is no magnification - the
    // whole point of the sight is that there is not - so the aperture is wide, the glass has the
    // faint warm cast of its coating, and nothing is zoomed.
    Preset p = MakeBuiltIn("Red Dot");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.20f;
    p.filters.scope.enabled = true;
    p.filters.scope.size = 0.80f;
    p.filters.scope.softness = 0.04f;
    p.filters.scope.magnification = 1.0f;
    p.filters.scope.reticle = 0.0f;
    p.filters.scope.shape = ScopeShape::Circle;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.30f;
    p.filters.lensSoftness.center = 0.60f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.92f;
    p.filters.colorCorrection.contrast = 1.10f;
    p.filters.colorCorrection.tint[0] = 1.00f;
    p.filters.colorCorrection.tint[1] = 0.94f;
    p.filters.colorCorrection.tint[2] = 0.88f;
    p.filters.colorCorrection.tintAmount = 0.28f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.10f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Edge;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.34f;
    p.filters.vignette.size = 0.68f;
    p.filters.vignette.roundness = 1.00f;
    return p;
}

// --- More tubes --------------------------------------------------------------------------------
//
// The first CRT family varied the mask. These vary the set: a broadcast monitor and a cheap
// living-room television are not the same device wearing different stripes, and the beam-width
// modulation is what finally lets them differ - a bright line on a tube is a fat line, and until
// that existed every one of these would have worn the same uniform stripes.

Preset PvmPreset() {
    // The broadcast reference monitor. Everything a consumer set does badly, this does well:
    // geometry close to flat, convergence tight, phosphor barely glowing. The scanlines are
    // crisp and the beam only just spreads, because a professional tube was driven properly.
    Preset p = MakeBuiltIn("PVM");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.06f;
    p.filters.distortion.shape = DistortionShape::Crt;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.34f;
    p.filters.scanlines.style = ScanlineStyle::ApertureGrille;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.scanlines.beamWidth = 0.35f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 1.10f;
    p.filters.colorCorrection.saturation = 1.06f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.16f;
    p.filters.bloom.threshold = 0.82f;
    p.filters.bloom.radius = 0.16f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.14f;
    p.filters.vignette.size = 0.86f;
    return p;
}

Preset ConsumerTvPreset() {
    // The set that was actually in the room: bowed, soft, misconverged, and glowing. The
    // aberration is standing in for convergence error, which is a different fault with the same
    // symptom - the channels do not land on top of each other.
    Preset p = MakeBuiltIn("Consumer TV");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.30f;
    p.filters.distortion.shape = DistortionShape::Crt;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.30f;
    p.filters.scanlines.style = ScanlineStyle::Soft;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.scanlines.beamWidth = 0.75f;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.44f;
    p.filters.lensSoftness.center = 0.20f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.16f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Edge;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 1.14f;
    p.filters.colorCorrection.contrast = 1.04f;
    p.filters.colorCorrection.gamma = 1.06f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.40f;
    p.filters.bloom.threshold = 0.60f;
    p.filters.bloom.radius = 0.34f;
    p.filters.bloom.tint[0] = 1.00f;
    p.filters.bloom.tint[1] = 0.86f;
    p.filters.bloom.tint[2] = 0.74f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.32f;
    p.filters.vignette.size = 0.76f;
    return p;
}

Preset HalationPreset() {
    // The bloom tint on its own, with just enough tube under it to have something to bleed. The
    // warm edge on a white highlight is light scattering inside the faceplate and coming back
    // red, and it is the single cue that separates a photograph of a CRT from a screenshot.
    Preset p = MakeBuiltIn("Halation");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.14f;
    p.filters.distortion.shape = DistortionShape::Crt;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.24f;
    p.filters.scanlines.style = ScanlineStyle::Soft;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.scanlines.beamWidth = 0.60f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.62f;
    p.filters.bloom.threshold = 0.52f;
    p.filters.bloom.radius = 0.58f;
    p.filters.bloom.tint[0] = 1.00f;
    p.filters.bloom.tint[1] = 0.52f;
    p.filters.bloom.tint[2] = 0.34f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 1.08f;
    p.filters.colorCorrection.saturation = 1.04f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.26f;
    p.filters.vignette.size = 0.80f;
    return p;
}

Preset InterlacedPreset() {
    // 480i. The two fields sit half a line apart and swap at the field rate, which is why an
    // interlaced picture shimmers on a progressive display and a progressive one does not.
    // Spacing of 2 so the alternation is visible rather than lost inside a fat gap.
    Preset p = MakeBuiltIn("Interlaced");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.12f;
    p.filters.distortion.shape = DistortionShape::Crt;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.40f;
    p.filters.scanlines.style = ScanlineStyle::Hard;
    p.filters.scanlines.spacing = 2.0f;
    p.filters.scanlines.beamWidth = 0.45f;
    p.filters.scanlines.interlace = 1.00f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 1.08f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.24f;
    p.filters.bloom.threshold = 0.70f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.22f;
    p.filters.vignette.size = 0.82f;
    return p;
}

Preset TubeBezelPreset() {
    // The glass itself. The scope's tube shape is a rounded rectangle rather than a circle, so
    // the corners of the picture are cut by the faceplate the way they are on a real set - and
    // solid black, because the body of a television is not a fade.
    Preset p = MakeBuiltIn("Tube Bezel");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.34f;
    p.filters.distortion.shape = DistortionShape::Crt;
    p.filters.scope.enabled = true;
    p.filters.scope.size = 0.92f;
    p.filters.scope.softness = 0.02f;
    p.filters.scope.magnification = 1.0f;
    p.filters.scope.reticle = 0.0f;
    p.filters.scope.shape = ScopeShape::Tube;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.28f;
    p.filters.scanlines.style = ScanlineStyle::Soft;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.scanlines.beamWidth = 0.55f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.30f;
    p.filters.bloom.threshold = 0.66f;
    p.filters.bloom.tint[0] = 1.00f;
    p.filters.bloom.tint[1] = 0.80f;
    p.filters.bloom.tint[2] = 0.68f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.34f;
    p.filters.vignette.size = 0.74f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.contrast = 1.08f;
    return p;
}

Preset VectorMonitorPreset() {
    // A vector display has no raster at all - no lines, no mask, nothing scanned. What it has is
    // an enormous amount of glow, because the beam dwells on the same phosphor instead of
    // sweeping past it. So: no scanlines, threshold on the floor, and radius wide open.
    Preset p = MakeBuiltIn("Vector Monitor");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.16f;
    p.filters.distortion.shape = DistortionShape::Radial;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.brightness = -0.06f;
    p.filters.colorCorrection.contrast = 1.62f;
    p.filters.colorCorrection.saturation = 1.30f;
    p.filters.colorCorrection.gamma = 0.82f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.95f;
    p.filters.bloom.threshold = 0.28f;
    p.filters.bloom.radius = 0.78f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.46f;
    p.filters.vignette.size = 0.70f;
    p.filters.vignette.roundness = 0.60f;
    p.effects.flicker.enabled = true;
    p.effects.flicker.intensity = 0.10f;
    p.effects.flicker.speed = 0.55f;
    return p;
}

Preset GreenTerminalPreset() {
    // A P1 phosphor terminal. Saturation to zero first and then the palette, the same recipe the
    // night-vision presets use, because it is the same physical situation: one phosphor, one
    // colour, and everything else decided by brightness.
    Preset p = MakeBuiltIn("Green Terminal");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.24f;
    p.filters.distortion.shape = DistortionShape::Crt;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.30f;
    p.filters.scanlines.style = ScanlineStyle::Sharp;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.scanlines.beamWidth = 0.70f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.0f;
    p.filters.colorCorrection.contrast = 1.34f;
    p.filters.colorCorrection.gamma = 0.86f;
    p.filters.falseColour.enabled = true;
    p.filters.falseColour.palette = FalseColourPalette::Phosphor;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.52f;
    p.filters.bloom.threshold = 0.46f;
    p.filters.bloom.radius = 0.44f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.44f;
    p.filters.vignette.size = 0.72f;
    return p;
}

Preset AmberTerminalPreset() {
    // The same tube with P3 in it. Amber was sold as the easier one to read for hours, which is
    // why so many terminals had it; here it is the tint doing the work rather than the palette,
    // because amber is a warm cast over a monochrome rather than a ramp of its own.
    Preset p = MakeBuiltIn("Amber Terminal");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.24f;
    p.filters.distortion.shape = DistortionShape::Crt;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.28f;
    p.filters.scanlines.style = ScanlineStyle::Sharp;
    p.filters.scanlines.spacing = 3.0f;
    p.filters.scanlines.beamWidth = 0.70f;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.0f;
    p.filters.colorCorrection.contrast = 1.30f;
    p.filters.colorCorrection.gamma = 0.88f;
    p.filters.colorCorrection.tint[0] = 1.00f;
    p.filters.colorCorrection.tint[1] = 0.66f;
    p.filters.colorCorrection.tint[2] = 0.16f;
    p.filters.colorCorrection.tintAmount = 1.00f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.50f;
    p.filters.bloom.threshold = 0.48f;
    p.filters.bloom.radius = 0.42f;
    p.filters.bloom.tint[0] = 1.00f;
    p.filters.bloom.tint[1] = 0.70f;
    p.filters.bloom.tint[2] = 0.30f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.44f;
    p.filters.vignette.size = 0.72f;
    return p;
}

Preset PortableTvPreset() {
    // A five-inch set with a coat-hanger for an aerial. Everything is worse: the geometry, the
    // convergence, the signal. The noise here is reception rather than sensor, which is why it
    // is coarse and colourless.
    Preset p = MakeBuiltIn("Portable TV");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.46f;
    p.filters.distortion.shape = DistortionShape::Crt;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.34f;
    p.filters.scanlines.style = ScanlineStyle::Soft;
    p.filters.scanlines.spacing = 4.0f;
    p.filters.scanlines.beamWidth = 0.80f;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.58f;
    p.filters.lensSoftness.center = 0.12f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.22f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Horizontal;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.86f;
    p.filters.colorCorrection.contrast = 0.94f;
    p.filters.colorCorrection.gamma = 1.10f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.46f;
    p.filters.vignette.size = 0.68f;
    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.22f;
    p.effects.noise.grainSize = 0.55f;
    p.effects.noise.speed = 0.95f;
    p.effects.flicker.enabled = true;
    p.effects.flicker.intensity = 0.12f;
    p.effects.flicker.speed = 0.80f;
    return p;
}

Preset ProjectionTvPreset() {
    // Rear projection: three tubes throwing onto a screen from behind. Contrast collapses
    // because ambient light lands on that screen too, convergence is never quite right because
    // three separate tubes have to agree, and the whole picture is soft at the edges.
    Preset p = MakeBuiltIn("Projection TV");
    p.filters.distortion.enabled = true;
    p.filters.distortion.amount = 0.20f;
    p.filters.distortion.shape = DistortionShape::Crt;
    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.16f;
    p.filters.scanlines.style = ScanlineStyle::Soft;
    p.filters.scanlines.spacing = 5.0f;
    p.filters.scanlines.beamWidth = 0.90f;
    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.66f;
    p.filters.lensSoftness.center = 0.28f;
    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.24f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Prism;
    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.saturation = 0.88f;
    p.filters.colorCorrection.brightness = 0.06f;
    p.filters.colorCorrection.contrast = 0.78f;
    p.filters.colorCorrection.gamma = 1.12f;
    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.36f;
    p.filters.bloom.threshold = 0.56f;
    p.filters.bloom.radius = 0.50f;
    p.filters.bloom.tint[0] = 1.00f;
    p.filters.bloom.tint[1] = 0.88f;
    p.filters.bloom.tint[2] = 0.80f;
    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.40f;
    p.filters.vignette.size = 0.70f;
    return p;
}

std::vector<Preset> BuiltInPresets() {
    std::vector<Preset> presets;
    presets.reserve(113);

    // The category is stamped here rather than inside each preset function: the families were
    // already the organising principle of this file, and grouping them once means a preset
    // cannot be filed under one heading while sitting in another's block.
    const auto family = [&presets](const char* category, std::initializer_list<Preset> members) {
        for (const Preset& member : members) {
            Preset item = member;
            item.category = category;
            presets.push_back(std::move(item));
        }
    };

    family(kCategoryBase, {NeutralPreset()});

    family(kCategoryCrt,
           {SoftCrtPreset(), ArcadeCrtPreset(), CurvedCrtPreset(), SoftScanlinesPreset(),
            TrinitronPreset(), ShadowMaskPreset(), PvmPreset(), ConsumerTvPreset(),
            HalationPreset(), InterlacedPreset(), TubeBezelPreset(), VectorMonitorPreset(),
            GreenTerminalPreset(), AmberTerminalPreset(), PortableTvPreset(),
            ProjectionTvPreset()});

    family(kCategoryConsoles,
           {GameBoyPreset(), GameBoyAdvancePreset(), SnesPreset(), MegaDrivePreset(),
            PlayStationPreset(), NesPreset(), Nintendo64Preset(), NeoGeoPreset(),
            PcEnginePreset(), VirtualBoyPreset()});

    family(kCategoryFisheye,
           {FisheyeWidePreset(), FisheyeExtremePreset(), FisheyeCrtPreset(), PeepholePreset(),
            SecurityCamPreset(), CircularFisheyePreset(), FisheyeSoftPreset(), DashcamPreset(),
            DroneFpvPreset(), SkateCamPreset(), BubbleLensPreset(), BugEyePreset(),
            CockpitGlassPreset(), PanoramicPreset(), DoorbellCamPreset()});

    family(kCategoryAntiFisheye,
           {AntiFisheyeLightPreset(), AntiFisheyeStrongPreset(), LensCorrectionPreset(),
            AntiFisheyeHorizontalPreset(), AntiFisheyeCornersPreset(), UltrawideCorrectPreset(),
            PincushionTubePreset()});

    family(kCategoryOptics, {SniperScopePreset(), ScopeCamPreset(), SpotterScopePreset(),
                             BinocularsPreset(), RedDotPreset()});

    family(kCategoryTactical,
           {NightVisionPreset(), ThermalPreset(), BreachPreset(), EntryTeamPreset(),
            NightOpsPreset(), WhitePhosphorPreset(), FlashbangPreset(), EvidenceCamPreset(),
            ShieldCamPreset(), TacLightPreset(), GasMaskPreset(), CsGasPreset(),
            SuspectCamPreset(), UnderDoorCamPreset(), StackUpPreset(), ConcussionPreset(),
            IrIlluminatorPreset(), DispatchFeedPreset(), InterviewRoomPreset(),
            TaserArcPreset()});

    // The six actual cameras, in one place. They were split between Tactical and Body-worn,
    // which meant the look this product gets asked for most was scattered across two headings
    // among two dozen presets that have nothing to do with it.
    //
    // Nothing in this family moves the frame, and nothing in it grains. Jitter wanders the
    // whole picture, rolling shutter shears it line by line, and grain crawls over it - on an
    // overlay that stays up for hours all three read as an effect running on top of the game
    // rather than as footage, and they are the first things anyone asks to turn off.
    //
    // Every parameter is left dialled in behind the disabled flag, so any of them can be
    // switched back on from the panel without being re-tuned (FILTERS-AND-EFFECTS.md 5).
    family(kCategoryBodycamGoPro,
           {GoProBodycamPreset(), BodycamPreset(), ChestCamPreset(), HelmetCamPreset(),
            ActionCamPreset(), DutyCamPreset()});

    family(kCategoryBodyWorn, {LowLightSensorPreset(), CheapSensorPreset(), DirtyLensPreset(),
                               NightPatrolPreset()});

    family(kCategoryRecon, {DroneFeedPreset(), UavThermalPreset(), BlackHotPreset(),
                            IronbowPreset(), ReconOpticPreset(), AzureReconPreset(),
                            SyncShotPreset()});

    family(kCategorySensor, {CrossComPreset(), MagneticViewPreset(), OpticalCamoPreset(),
                             WarhoundFeedPreset(), EmpBurstPreset(), GhostModePreset()});

    family(kCategoryEffect, {VhsPreset(), ProjectorPreset(), FilmGrainPreset(), PrismLensPreset(),
                             BrokenSignalPreset()});

    return presets;
}

int PresetCategoryRank(std::string_view category) noexcept {
    // Roughly display order: the neutral reference, then the display families, then the optical
    // ones, then the tactical block, then the purely effect-led. Anything unrecognised - which
    // includes Custom - sorts last.
    constexpr std::string_view kOrder[] = {
        kCategoryBase,     kCategoryCrt,      kCategoryConsoles, kCategoryFisheye,
        kCategoryAntiFisheye, kCategoryOptics, kCategoryTactical, kCategoryBodycamGoPro,
        kCategoryBodyWorn, kCategoryRecon,  kCategorySensor,   kCategoryEffect,
    };

    int rank = static_cast<int>(std::size(kOrder)) + 1;
    for (size_t i = 0; i < std::size(kOrder); ++i) {
        if (kOrder[i] == category) {
            rank = static_cast<int>(i);
            break;
        }
    }
    return rank;
}

std::string CategoryForBuiltIn(std::string_view name) {
    // Built once: the lookup runs per preset on load, and rebuilding ninety-odd presets each
    // time would turn a directory listing into something measurable.
    static const std::vector<Preset> shipped = BuiltInPresets();

    std::string result;
    for (const Preset& preset : shipped) {
        if (preset.name == name) {
            result = preset.category;
            break;
        }
    }
    return result;
}

bool PresetMatches(const Preset& preset, const FilterSettings& f,
                   const EffectSettings& e) noexcept {
    const FilterSettings& p = preset.filters;

    if (!SameBase(p.distortion, f.distortion) ||
        !NearlyEqual(p.distortion.amount, f.distortion.amount) ||
        p.distortion.shape != f.distortion.shape) {
        return false;
    }
    if (!SameBase(p.vignette, f.vignette) || !NearlyEqual(p.vignette.size, f.vignette.size) ||
        !NearlyEqual(p.vignette.softness, f.vignette.softness) ||
        !NearlyEqual(p.vignette.roundness, f.vignette.roundness)) {
        return false;
    }
    if (!SameBase(p.scanlines, f.scanlines) ||
        !NearlyEqual(p.scanlines.thickness, f.scanlines.thickness) ||
        !NearlyEqual(p.scanlines.spacing, f.scanlines.spacing) ||
        p.scanlines.style != f.scanlines.style ||
        p.scanlines.orientation != f.scanlines.orientation ||
        p.scanlines.scaleMode != f.scanlines.scaleMode ||
        !NearlyEqual(p.scanlines.beamWidth, f.scanlines.beamWidth) ||
        !NearlyEqual(p.scanlines.interlace, f.scanlines.interlace)) {
        return false;
    }
    if (!SameBase(p.chromaticAberration, f.chromaticAberration) ||
        p.chromaticAberration.mode != f.chromaticAberration.mode ||
        !NearlyEqual(p.chromaticAberration.edgeBias, f.chromaticAberration.edgeBias) ||
        !NearlyEqual(p.chromaticAberration.redShift, f.chromaticAberration.redShift) ||
        !NearlyEqual(p.chromaticAberration.blueShift, f.chromaticAberration.blueShift)) {
        return false;
    }
    if (!SameBase(p.colorCorrection, f.colorCorrection) ||
        !NearlyEqual(p.colorCorrection.brightness, f.colorCorrection.brightness) ||
        !NearlyEqual(p.colorCorrection.contrast, f.colorCorrection.contrast) ||
        !NearlyEqual(p.colorCorrection.saturation, f.colorCorrection.saturation) ||
        !NearlyEqual(p.colorCorrection.gamma, f.colorCorrection.gamma) ||
        !NearlyEqual(p.colorCorrection.tintAmount, f.colorCorrection.tintAmount) ||
        !NearlyEqual(p.colorCorrection.tint[0], f.colorCorrection.tint[0]) ||
        !NearlyEqual(p.colorCorrection.tint[1], f.colorCorrection.tint[1]) ||
        !NearlyEqual(p.colorCorrection.tint[2], f.colorCorrection.tint[2]) ||
        !NearlyEqual(p.colorCorrection.posterize, f.colorCorrection.posterize)) {
        return false;
    }
    if (!SameBase(p.bloom, f.bloom) || !NearlyEqual(p.bloom.threshold, f.bloom.threshold) ||
        !NearlyEqual(p.bloom.radius, f.bloom.radius) ||
        !NearlyEqual(p.bloom.tint[0], f.bloom.tint[0]) ||
        !NearlyEqual(p.bloom.tint[1], f.bloom.tint[1]) ||
        !NearlyEqual(p.bloom.tint[2], f.bloom.tint[2])) {
        return false;
    }
    if (!SameBase(p.falseColour, f.falseColour) || p.falseColour.palette != f.falseColour.palette ||
        !NearlyEqual(p.falseColour.levels, f.falseColour.levels)) {
        return false;
    }
    if (!SameBase(p.edgeGlow, f.edgeGlow) || !NearlyEqual(p.edgeGlow.width, f.edgeGlow.width) ||
        !NearlyEqual(p.edgeGlow.tint[0], f.edgeGlow.tint[0]) ||
        !NearlyEqual(p.edgeGlow.tint[1], f.edgeGlow.tint[1]) ||
        !NearlyEqual(p.edgeGlow.tint[2], f.edgeGlow.tint[2])) {
        return false;
    }
    if (!SameBase(p.lensDirt, f.lensDirt) || !NearlyEqual(p.lensDirt.density, f.lensDirt.density) ||
        !NearlyEqual(p.lensDirt.smear, f.lensDirt.smear)) {
        return false;
    }
    if (!SameBase(p.lensSoftness, f.lensSoftness) ||
        !NearlyEqual(p.lensSoftness.center, f.lensSoftness.center)) {
        return false;
    }
    if (!SameBase(p.sharpen, f.sharpen) || !NearlyEqual(p.sharpen.radius, f.sharpen.radius)) {
        return false;
    }
    if (!SameBase(p.scope, f.scope) || p.scope.shape != f.scope.shape ||
        !NearlyEqual(p.scope.size, f.scope.size) ||
        !NearlyEqual(p.scope.softness, f.scope.softness) ||
        !NearlyEqual(p.scope.magnification, f.scope.magnification) ||
        !NearlyEqual(p.scope.reticle, f.scope.reticle)) {
        return false;
    }

    const GlitchSettings& pg = preset.effects.glitch;
    const GlitchSettings& eg = e.glitch;
    if (!SameBase(pg, eg) || !NearlyEqual(pg.frequency, eg.frequency) ||
        !NearlyEqual(pg.blockSize, eg.blockSize) || !NearlyEqual(pg.jitter, eg.jitter) ||
        !NearlyEqual(pg.rgbShift, eg.rgbShift) || pg.style != eg.style) {
        return false;
    }

    if (!SameBase(preset.effects.noise, e.noise) ||
        !NearlyEqual(preset.effects.noise.grainSize, e.noise.grainSize) ||
        !NearlyEqual(preset.effects.noise.speed, e.noise.speed) ||
        !NearlyEqual(preset.effects.noise.colorAmount, e.noise.colorAmount)) {
        return false;
    }
    if (!SameBase(preset.effects.flicker, e.flicker) ||
        !NearlyEqual(preset.effects.flicker.speed, e.flicker.speed)) {
        return false;
    }
    if (!SameBase(preset.effects.jitter, e.jitter) ||
        !NearlyEqual(preset.effects.jitter.speed, e.jitter.speed)) {
        return false;
    }
    if (!SameBase(preset.effects.shimmer, e.shimmer) ||
        !NearlyEqual(preset.effects.shimmer.speed, e.shimmer.speed) ||
        !NearlyEqual(preset.effects.shimmer.scale, e.shimmer.scale)) {
        return false;
    }
    if (!SameBase(preset.effects.rollingShutter, e.rollingShutter) ||
        !NearlyEqual(preset.effects.rollingShutter.speed, e.rollingShutter.speed)) {
        return false;
    }
    return SameBase(preset.effects.scanSweep, e.scanSweep) &&
           NearlyEqual(preset.effects.scanSweep.speed, e.scanSweep.speed) &&
           NearlyEqual(preset.effects.scanSweep.width, e.scanSweep.width);
}

// --------------------------------------------------------------------------------------

PresetRepository::PresetRepository(std::filesystem::path appDataDirectory) {
    if (!appDataDirectory.empty()) {
        m_directory = appDataDirectory / L"presets";
    }
}

std::string PresetRepository::SanitizeName(std::string_view name) {
    std::string result;
    result.reserve(name.size());

    bool pendingSpace = false;
    for (const char c : name) {
        const auto uc = static_cast<unsigned char>(c);
        if (std::isspace(uc) != 0) {
            pendingSpace = !result.empty();
            continue;
        }
        // Control characters would make the file unreadable and the list unreadable with it.
        if (uc < 0x20) {
            continue;
        }
        if (pendingSpace) {
            result.push_back(' ');
            pendingSpace = false;
        }
        result.push_back(c);
    }
    return result;
}

std::string PresetRepository::MakeFileStem(std::string_view name) {
    std::string stem;
    stem.reserve(name.size());

    bool pendingSeparator = false;
    for (const char c : name) {
        const auto uc = static_cast<unsigned char>(c);
        if ((uc >= 'a' && uc <= 'z') || (uc >= '0' && uc <= '9')) {
            if (pendingSeparator && !stem.empty()) {
                stem.push_back('-');
            }
            pendingSeparator = false;
            stem.push_back(static_cast<char>(c));
        } else if (uc >= 'A' && uc <= 'Z') {
            if (pendingSeparator && !stem.empty()) {
                stem.push_back('-');
            }
            pendingSeparator = false;
            stem.push_back(static_cast<char>(uc - 'A' + 'a'));
        } else {
            pendingSeparator = true;
        }
    }

    // A name made entirely of characters we drop - punctuation, or a non-Latin script -
    // still needs somewhere to live.
    return stem.empty() ? std::string("preset") : stem;
}

std::string PresetRepository::MakeUniqueName(std::string_view desired,
                                             const std::vector<Preset>& taken) {
    const std::string base = SanitizeName(desired);
    const std::string fallback = base.empty() ? std::string("Preset") : base;

    const auto isTaken = [&taken](const std::string& candidate) {
        return std::any_of(taken.begin(), taken.end(),
                           [&candidate](const Preset& p) { return p.name == candidate; });
    };

    if (!isTaken(fallback)) {
        return fallback;
    }
    for (int suffix = 2; suffix < 1000; ++suffix) {
        std::string candidate = std::format("{} ({})", fallback, suffix);
        if (!isTaken(candidate)) {
            return candidate;
        }
    }
    return fallback;
}

std::filesystem::path PresetRepository::FilePathFor(std::string_view name) const {
    if (m_directory.empty()) {
        return {};
    }
    return m_directory / (MakeFileStem(name) + kExtension);
}

std::vector<std::string> PresetRepository::SeedMissingBuiltIns(
    const std::vector<std::string>& alreadySeeded) const {
    std::vector<std::string> offered;
    if (m_directory.empty()) {
        return offered;
    }

    std::error_code ec;
    std::filesystem::create_directories(m_directory, ec);
    if (ec) {
        LogError("Presets: cannot create {}: {}", Utf8FromWide(m_directory.wstring()),
                 ec.message());
        return offered;
    }

    size_t writes = 0;
    for (const Preset& preset : BuiltInPresets()) {
        const bool seen = std::find(alreadySeeded.begin(), alreadySeeded.end(), preset.name) !=
                          alreadySeeded.end();
        if (seen) {
            continue;
        }

        // A file that is already there is left exactly as it is - it may carry the user's
        // edits. Reporting it as offered anyway is what keeps this from being reconsidered
        // on the next launch.
        std::error_code exists;
        if (std::filesystem::exists(FilePathFor(preset.name), exists)) {
            offered.push_back(preset.name);
            continue;
        }

        if (Save(preset)) {
            offered.push_back(preset.name);
            ++writes;
        }
    }

    if (writes > 0) {
        LogInfo("Presets: added {} new built-in preset(s) to {}.", writes,
                Utf8FromWide(m_directory.wstring()));
    }
    return offered;
}

std::vector<Preset> PresetRepository::LoadAll() const {
    std::vector<Preset> presets;
    if (m_directory.empty()) {
        return presets;
    }

    std::error_code ec;
    if (!std::filesystem::exists(m_directory, ec)) {
        return presets;
    }

    for (const auto& entry : std::filesystem::directory_iterator(m_directory, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file() || entry.path().extension() != kExtension) {
            continue;
        }

        const json root = json_io::ParseJsonFile(entry.path());
        if (root.is_discarded() || !root.is_object()) {
            // One bad file must not take the whole list with it.
            LogWarn("Presets: skipping {}, not valid JSON.",
                    Utf8FromWide(entry.path().filename().wstring()));
            continue;
        }

        uint32_t schemaVersion = kPresetSchemaVersion;
        if (const json* node = Child(root, "schemaVersion");
            node != nullptr && node->is_number_unsigned()) {
            schemaVersion = node->get<uint32_t>();
        }
        if (schemaVersion > kPresetSchemaVersion) {
            LogWarn("Presets: skipping {}, schemaVersion {} is newer than supported {}.",
                    Utf8FromWide(entry.path().filename().wstring()), schemaVersion,
                    kPresetSchemaVersion);
            continue;
        }

        Preset preset;
        preset.file = entry.path();
        preset.name = entry.path().stem().string();
        json_io::Read(root, "name", preset.name);
        json_io::Read(root, "builtIn", preset.builtIn);
        json_io::Read(root, "category", preset.category);

        // A file written before the field existed carries no category. Recovering it from the
        // name is what keeps an upgrade from filing every shipped preset under Custom, since
        // SeedMissingBuiltIns will not rewrite a file that is already on disk.
        if (preset.category.empty()) {
            preset.category = CategoryForBuiltIn(preset.name);
        }
        if (preset.category.empty()) {
            preset.category = kCategoryCustom;
        }

        if (const json* filters = Child(root, "filters"); filters != nullptr) {
            json_io::ReadFilters(*filters, preset.filters);
        }
        if (const json* effects = Child(root, "effects"); effects != nullptr) {
            json_io::ReadEffects(*effects, preset.effects);
        }

        presets.push_back(std::move(preset));
    }

    // Built-ins first so the shipped starting points stay together at the top, then
    // alphabetical within each group.
    // Grouped by category first, so the panel can emit a heading whenever the category changes
    // and never has to sort or bucket anything itself.
    std::sort(presets.begin(), presets.end(), [](const Preset& a, const Preset& b) {
        const int rankA = PresetCategoryRank(a.category);
        const int rankB = PresetCategoryRank(b.category);
        if (rankA != rankB) {
            return rankA < rankB;
        }
        if (a.category != b.category) {
            return a.category < b.category;
        }
        if (a.builtIn != b.builtIn) {
            return a.builtIn;
        }
        return a.name < b.name;
    });
    return presets;
}

bool PresetRepository::Save(const Preset& preset) const {
    const std::filesystem::path path =
        preset.file.empty() ? FilePathFor(preset.name) : preset.file;
    if (path.empty()) {
        return false;
    }

    json root;
    root["schemaVersion"] = kPresetSchemaVersion;
    root["name"] = preset.name;
    root["builtIn"] = preset.builtIn;
    root["category"] = preset.category.empty() ? kCategoryCustom : preset.category;
    root["filters"] = json_io::SerializeFilters(preset.filters);
    root["effects"] = json_io::SerializeEffects(preset.effects);

    return json_io::WriteJsonAtomically(path, root);
}

bool PresetRepository::Delete(const Preset& preset) const {
    const std::filesystem::path path =
        preset.file.empty() ? FilePathFor(preset.name) : preset.file;
    if (path.empty()) {
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::remove(path, ec) || ec) {
        LogError("Presets: cannot delete {}: {}", Utf8FromWide(path.wstring()),
                 ec ? ec.message() : "file not found");
        return false;
    }
    LogInfo("Presets: deleted '{}'.", preset.name);
    return true;
}

bool PresetRepository::Rename(const Preset& preset, std::string_view newName) const {
    const std::string sanitized = SanitizeName(newName);
    if (sanitized.empty() || sanitized == preset.name) {
        return false;
    }

    Preset renamed = preset;
    renamed.name = sanitized;
    renamed.file = FilePathFor(sanitized);

    // Write the new file first: if that fails there is nothing to undo, and the original is
    // still sitting there intact.
    if (!Save(renamed)) {
        return false;
    }

    if (!preset.file.empty() && preset.file != renamed.file) {
        std::error_code ec;
        std::filesystem::remove(preset.file, ec);
    }

    LogInfo("Presets: renamed '{}' to '{}'.", preset.name, sanitized);
    return true;
}

}  // namespace overlaydesk
