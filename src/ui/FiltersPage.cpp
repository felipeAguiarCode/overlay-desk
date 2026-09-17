// UI-SPEC.md "Filters" tab: one card per module, each with the ON/OFF state, the single
// intensity control, and the module-specific parameters behind a collapsible "Advanced".
//
// PRD section 9: when a module is OFF its advanced parameters stay visible but disabled, so
// the user can see what they dialled in without turning the effect back on to check.
// FILTERS-AND-EFFECTS.md section 5 backs that up on the data side - disabling never
// discards a value.

#include <imgui.h>

#include "core/RenderMath.h"
#include "ui/ControlPanel.h"

namespace overlaydesk::ui {
namespace {

// Defaults come from FILTERS-AND-EFFECTS.md section 6 by way of the settings structs, so a
// per-module reset and a double-click on a slider both land on the documented value rather
// than a number retyped here.
const FilterSettings kFilterDefaults;
const DistortionSettings kDistortionDefaults;
const VignetteSettings kVignetteDefaults;
const ScanlineSettings kScanlineDefaults;
const ChromaticAberrationSettings kChromaticDefaults;
const ColorCorrectionSettings kColorDefaults;
const ScopeSettings kScopeDefaults;
const BloomSettings kBloomDefaults;
const FalseColourSettings kFalseColourDefaults;
const EdgeGlowSettings kEdgeGlowDefaults;
const LensDirtSettings kLensDirtDefaults;
const LensSoftnessSettings kLensSoftnessDefaults;
const SharpenSettings kSharpenDefaults;

constexpr const char* kDistortionShapes[] = {"Radial", "CRT (per axis)", "Cylindrical",
                                             "Vertical", "Corner only"};
constexpr const char* kScanlineStyles[] = {"Hard", "Soft", "Sharp", "Aperture grille",
                                           "Slot mask"};
constexpr const char* kOrientations[] = {"Horizontal", "Vertical", "Grid"};
constexpr const char* kScaleModes[] = {"Relative to source", "Pixel perfect"};
constexpr const char* kChromaticModes[] = {"Radial", "Horizontal", "Vertical", "Edge",
                                           "Prism", "Barrel"};
constexpr const char* kScopeShapes[] = {"Circle", "Binocular", "Quad tube (NVG)",
                                        "Tube (CRT faceplate)"};
constexpr const char* kPalettes[] = {"White hot",      "Black hot",       "Ironbow",
                                     "Phosphor green", "White phosphor", "Cross-Com cyan"};

bool EnumCombo(const char* label, int* value, const char* const items[], int count) {
    return ImGui::Combo(label, value, items, count);
}

// --- Distortion (RF-011) ------------------------------------------------------------------

bool DrawDistortion(DistortionSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("DISTORTION", &s.enabled, &reset);
    if (reset) {
        s = kDistortionDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);

    // The bipolar control is the whole point of this module, so it sits above intensity
    // rather than behind Advanced.
    changed |= widgets::RangedSlider("Amount", &s.amount, -1.0f, 1.0f, kDistortionDefaults.amount,
                                     "%.2f");
    widgets::HelpText("-1.00 anti-fisheye  .  0.00 neutral  .  +1.00 fisheye");

    int shape = static_cast<int>(s.shape);
    if (EnumCombo("Shape", &shape, kDistortionShapes, IM_ARRAYSIZE(kDistortionShapes))) {
        s.shape = static_cast<DistortionShape>(shape);
        changed = true;
    }

    changed |= widgets::PercentSlider("Intensity", &s.intensity, kDistortionDefaults.intensity);

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        widgets::HelpText(
            "Radial bends everything toward the centre. CRT bends each axis by the other, the "
            "way tube geometry does, so lines near the axes stay straight. Cylindrical and "
            "Vertical bend one axis only. Corner only leaves the middle flat and pulls hard at "
            "the corners.");
        ImGui::Spacing();
        widgets::HelpText(
            "Positive amounts pin the extremes, so nothing is cropped. Negative amounts zoom in "
            "slightly and lose the corners: a pinch pulls the edges inward and the content that "
            "would replace them was never captured. Either way the frame stays full - no amount "
            "opens a transparent gap - and the vignette stays anchored to the overlay border.");
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

// --- Vignette (RF-012) --------------------------------------------------------------------

bool DrawVignette(VignetteSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("VIGNETTE", &s.enabled, &reset);
    if (reset) {
        s = kVignetteDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);
    changed |= widgets::PercentSlider("Intensity", &s.intensity, kVignetteDefaults.intensity);

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        changed |= widgets::PercentSlider("Size", &s.size, kVignetteDefaults.size);
        changed |= widgets::PercentSlider("Softness", &s.softness, kVignetteDefaults.softness);
        changed |= widgets::PercentSlider("Roundness", &s.roundness, kVignetteDefaults.roundness);
        widgets::HelpText(
            "Size is the area left untouched, softness the width of the falloff, and "
            "roundness morphs the shape from the overlay rectangle to an ellipse inside it.");
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

// --- Scanlines (RF-013) -------------------------------------------------------------------

bool DrawScanlines(ScanlineSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("SCANLINES", &s.enabled, &reset);
    if (reset) {
        s = kScanlineDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);

    int style = static_cast<int>(s.style);
    if (EnumCombo("Style", &style, kScanlineStyles, IM_ARRAYSIZE(kScanlineStyles))) {
        s.style = static_cast<ScanlineStyle>(style);
        changed = true;
    }

    changed |= widgets::PercentSlider("Intensity", &s.intensity, kScanlineDefaults.intensity);

    // The two mask styles build their own pattern out of RGB triads, so orientation has
    // nothing to say about them.
    const bool isMask = s.style == ScanlineStyle::ApertureGrille ||
                        s.style == ScanlineStyle::SlotMask;

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        changed |= widgets::RangedSlider("Thickness", &s.thickness, 0.5f, 8.0f,
                                         kScanlineDefaults.thickness, "%.1f px");
        changed |= widgets::RangedSlider("Spacing", &s.spacing, 1.0f, 16.0f,
                                         kScanlineDefaults.spacing, "%.1f px");

        ImGui::BeginDisabled(isMask);
        int orientation = static_cast<int>(s.orientation);
        if (EnumCombo("Orientation", &orientation, kOrientations, IM_ARRAYSIZE(kOrientations))) {
            s.orientation = static_cast<ScanlineOrientation>(orientation);
            changed = true;
        }
        ImGui::EndDisabled();

        int scaleMode = static_cast<int>(s.scaleMode);
        if (EnumCombo("Scale", &scaleMode, kScaleModes, IM_ARRAYSIZE(kScaleModes))) {
            s.scaleMode = static_cast<ScanlineScaleMode>(scaleMode);
            changed = true;
        }

        ImGui::Spacing();
        changed |= widgets::PercentSlider("Beam width", &s.beamWidth,
                                          kScanlineDefaults.beamWidth);
        widgets::HelpText(
            "How much a bright area widens its own line. On a tube the beam spreads as "
            "it is driven harder, so highlights close up and shadows keep their black "
            "gaps. At 0% the pattern is uniform, which reads as stripes laid over the "
            "picture rather than as a screen.");

        changed |= widgets::PercentSlider("Interlace", &s.interlace,
                                          kScanlineDefaults.interlace);
        widgets::HelpText(
            "Alternates the pattern half a line every field, the way 480i is drawn. "
            "0% is progressive.");

        ImGui::Spacing();
        widgets::HelpText(
            "Hard, Soft and Sharp dim every channel equally and follow the orientation. "
            "Aperture grille and Slot mask build RGB triads instead, which is where the "
            "colour fringing of a real shadow mask comes from - they set their own pattern, so "
            "orientation does not apply.");
        ImGui::Spacing();
        widgets::HelpText(
            "Pixel perfect measures spacing in overlay pixels. Relative measures it in "
            "source pixels, so a 240p source keeps 240 scanlines however far you scale the "
            "overlay up.");
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

// --- Chromatic aberration (RF-014) --------------------------------------------------------

bool DrawChromatic(ChromaticAberrationSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("CHROMATIC ABERRATION", &s.enabled, &reset);
    if (reset) {
        s = kChromaticDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);
    changed |= widgets::PercentSlider("Intensity", &s.intensity, kChromaticDefaults.intensity);

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        int mode = static_cast<int>(s.mode);
        if (EnumCombo("Mode", &mode, kChromaticModes, IM_ARRAYSIZE(kChromaticModes))) {
            s.mode = static_cast<ChromaticAberrationMode>(mode);
            changed = true;
        }

        changed |= widgets::PercentSlider("Edge bias", &s.edgeBias, kChromaticDefaults.edgeBias);
        changed |= widgets::RangedSlider("Red shift", &s.redShift, -2.0f, 2.0f,
                                         kChromaticDefaults.redShift, "%.2f");
        changed |= widgets::RangedSlider("Blue shift", &s.blueShift, -2.0f, 2.0f,
                                         kChromaticDefaults.blueShift, "%.2f");
        widgets::HelpText(
            "Green stays put and defines the frame; red and blue are displaced along the "
            "chosen axis. Radial and Edge follow the radius, Horizontal and Vertical use one "
            "axis, Barrel grows with the square of the radius the way a lens misfocuses, and "
            "Prism fans the two channels to different angles like glass splitting light.");
        ImGui::Spacing();
        widgets::HelpText(
            "Edge bias pushes the separation toward the rim and leaves the centre clean. "
            "Barrel ignores it - its falloff is fixed by the optics it imitates.");
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

// --- Colour correction (RF-015) -----------------------------------------------------------

bool DrawColorCorrection(ColorCorrectionSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("COLOR CORRECTION", &s.enabled, &reset);
    if (reset) {
        s = kColorDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);
    changed |= widgets::PercentSlider("Intensity", &s.intensity, kColorDefaults.intensity);

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        changed |= widgets::RangedSlider("Brightness", &s.brightness, -0.5f, 0.5f,
                                         kColorDefaults.brightness, "%.2f");
        changed |= widgets::RangedSlider("Contrast", &s.contrast, 0.0f, 3.0f,
                                         kColorDefaults.contrast, "%.2f");
        changed |= widgets::RangedSlider("Saturation", &s.saturation, 0.0f, 3.0f,
                                         kColorDefaults.saturation, "%.2f");
        changed |= widgets::RangedSlider("Gamma", &s.gamma, 0.2f, 3.0f, kColorDefaults.gamma,
                                         "%.2f");

        ImGui::Spacing();
        changed |= ImGui::ColorEdit3("Tint", s.tint, ImGuiColorEditFlags_NoInputs);
        changed |= widgets::PercentSlider("Tint amount", &s.tintAmount, kColorDefaults.tintAmount);
        widgets::HelpText(
            "The image is multiplied by the tint, so blacks stay black and only what was lit "
            "takes the colour - the way a phosphor or an LCD dye behaves. Drop saturation to "
            "0 first for a true monochrome in that colour: Game Boy green, night-vision "
            "phosphor, Virtual Boy red.");

        ImGui::Spacing();
        changed |= widgets::PercentSlider("Posterize", &s.posterize, kColorDefaults.posterize);
        widgets::HelpText(
            "Quantises each channel into steps, the way a display with too few bits does. At "
            "0% the image passes through with its full tonal range.");

        const bool neutral = s.brightness == kColorDefaults.brightness &&
                             s.contrast == kColorDefaults.contrast &&
                             s.saturation == kColorDefaults.saturation &&
                             s.gamma == kColorDefaults.gamma &&
                             s.tintAmount == kColorDefaults.tintAmount &&
                             s.posterize == kColorDefaults.posterize;
        if (neutral) {
            // The Milestone 5 gate is that neutral values reproduce the source untouched,
            // so it is worth saying out loud when the user is sitting on them.
            ImGui::TextDisabled("Neutral: the image passes through unchanged.");
        }
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

// --- Bloom (ADR-0009) ----------------------------------------------------------------------

bool DrawBloom(BloomSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("BLOOM", &s.enabled, &reset);
    if (reset) {
        s = kBloomDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);
    changed |= widgets::PercentSlider("Intensity", &s.intensity, kBloomDefaults.intensity);
    changed |= widgets::PercentSlider("Threshold", &s.threshold, kBloomDefaults.threshold);
    widgets::HelpText(
        "Only what is brighter than the threshold blooms, and the halo keeps the colour of "
        "the light that made it. Drop the threshold and the whole image starts to glow.");

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        changed |= widgets::PercentSlider("Radius", &s.radius, kBloomDefaults.radius);
        changed |= ImGui::ColorEdit3("Halo colour", s.tint, ImGuiColorEditFlags_NoInputs);
        widgets::HelpText(
            "White leaves the halo the colour of the light that made it. Warming it "
            "gives halation: light scattering inside the faceplate of a tube comes back "
            "red, which is why a white highlight on a CRT has a warm edge.");
        widgets::HelpText(
            "This is a glow built from twelve samples, not a true blur - a real one would "
            "need a second render pass the memory budget has no room for. At the widest "
            "radius the sampling pattern becomes visible, which is the trade being made.");
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

// --- False colour (ADR-0009) ---------------------------------------------------------------

bool DrawFalseColour(FalseColourSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("FALSE COLOUR", &s.enabled, &reset);
    if (reset) {
        s = kFalseColourDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);

    int palette = static_cast<int>(s.palette);
    if (EnumCombo("Palette", &palette, kPalettes, IM_ARRAYSIZE(kPalettes))) {
        s.palette = static_cast<FalseColourPalette>(palette);
        changed = true;
    }

    changed |= widgets::PercentSlider("Intensity", &s.intensity, kFalseColourDefaults.intensity);
    widgets::HelpText(
        "Maps brightness onto a palette, which is what a thermal sight actually does. Drop "
        "Color Correction's saturation to 0 first so the mapping reads brightness and not a "
        "colour cast. Intensity below 100% blends the palette with the graded image.");

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        changed |= widgets::PercentSlider("Levels", &s.levels, kFalseColourDefaults.levels);
        widgets::HelpText(
            "0% keeps the ramp continuous. Above that it is quantised, the way a field "
            "display with a handful of bits shows it.");
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

// --- Edge glow (ADR-0009) ------------------------------------------------------------------

bool DrawEdgeGlow(EdgeGlowSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("EDGE GLOW", &s.enabled, &reset);
    if (reset) {
        s = kEdgeGlowDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);
    changed |= widgets::PercentSlider("Intensity", &s.intensity, kEdgeGlowDefaults.intensity);
    changed |= ImGui::ColorEdit3("Colour", s.tint, ImGuiColorEditFlags_NoInputs);
    widgets::HelpText(
        "Outlines what the sensor is looking at. The edges come from the captured image "
        "itself, so scanlines and grain never register as contours.");

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        changed |= widgets::PercentSlider("Width", &s.width, kEdgeGlowDefaults.width);
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

// --- Lens softness (ADR-0011) --------------------------------------------------------------

bool DrawLensSoftness(LensSoftnessSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("LENS SOFTNESS", &s.enabled, &reset);
    if (reset) {
        s = kLensSoftnessDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);
    changed |= widgets::PercentSlider("Intensity", &s.intensity, kLensSoftnessDefaults.intensity);
    changed |= widgets::PercentSlider("Sharp zone", &s.center, kLensSoftnessDefaults.center);
    widgets::HelpText(
        "A lens is sharp in the middle and falls apart towards the corners, and the wider it "
        "is the earlier that starts. `Sharp zone` is how far out the crisp part reaches; "
        "beyond it the softening ramps up quadratically. Pair it with Distortion - it is what "
        "makes a fisheye read as glass instead of as a warped screenshot.");

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        widgets::HelpText(
            "Six samples on a ring, applied before Bloom because the defocus happens in the "
            "lens and the blooming happens at the sensor behind it.");
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

// --- Sharpen (ADR-0013) --------------------------------------------------------------------

bool DrawSharpen(SharpenSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("SHARPEN", &s.enabled, &reset);
    if (reset) {
        s = kSharpenDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);
    changed |= widgets::PercentSlider("Intensity", &s.intensity, kSharpenDefaults.intensity);
    changed |= widgets::PercentSlider("Radius", &s.radius, kSharpenDefaults.radius);
    widgets::HelpText(
        "The crunch a camera puts on its own footage. An action cam resolves badly through a "
        "very wide lens and answers that with an aggressive unsharp mask, hard enough that the "
        "halo around a high-contrast edge shows. `Radius` is how far the halo reaches. Pair it "
        "with Lens Softness: soft corners and a sharpened middle is what the format looks "
        "like.");

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        widgets::HelpText(
            "Four samples on a cross, read from the captured texture rather than from the "
            "running image, so it finds edges in the picture instead of edges in the grain and "
            "the scanlines. Applied after Bloom and before any grading: the lens defocuses, "
            "the sensor blooms, the ISP sharpens what it read, and only then is the picture "
            "graded.");
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

// --- Lens dirt (ADR-0009) ------------------------------------------------------------------

bool DrawLensDirt(LensDirtSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("LENS DIRT", &s.enabled, &reset);
    if (reset) {
        s = kLensDirtDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);
    changed |= widgets::PercentSlider("Intensity", &s.intensity, kLensDirtDefaults.intensity);

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        changed |= widgets::PercentSlider("Density", &s.density, kLensDirtDefaults.density);
        changed |= widgets::PercentSlider("Smear", &s.smear, kLensDirtDefaults.smear);
        widgets::HelpText(
            "Grease and spatter on the front element. It does not move - dirt on glass stays "
            "where it is, and animating it would read as rain instead.");
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

// --- Scope ---------------------------------------------------------------------------------

bool DrawScope(ScopeSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("SCOPE", &s.enabled, &reset);
    if (reset) {
        s = kScopeDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);

    int shape = static_cast<int>(s.shape);
    if (EnumCombo("Shape", &shape, kScopeShapes, IM_ARRAYSIZE(kScopeShapes))) {
        s.shape = static_cast<ScopeShape>(shape);
        changed = true;
    }

    changed |= widgets::PercentSlider("Intensity", &s.intensity, kScopeDefaults.intensity);
    changed |= widgets::PercentSlider("Aperture", &s.size, kScopeDefaults.size);
    changed |= widgets::RangedSlider("Magnification", &s.magnification, 1.0f, 4.0f,
                                     kScopeDefaults.magnification, "%.2fx");
    widgets::HelpText(
        "Aperture is the size of the glass; everything outside it is the body of the optic, "
        "drawn as solid black rather than fading out - a vignette would let the desktop show "
        "through where the tube should be.");

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        changed |= widgets::PercentSlider("Edge softness", &s.softness, kScopeDefaults.softness);
        changed |= widgets::PercentSlider("Reticle", &s.reticle, kScopeDefaults.reticle);
        widgets::HelpText(
            "Magnification zooms before the shake is applied, so the jitter effect is "
            "magnified with the picture - which is the part of a high-power optic that is "
            "hard to hold steady. Pair this with Distortion for the bulge a real scope adds.");
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

}  // namespace

void ControlPanel::DrawFiltersPage(AppState& state, const PanelActions& actions) {
    FilterSettings& filters = state.settings.filters;

    ImGui::Spacing();

    if (ImGui::SmallButton("Disable all")) {
        filters.distortion.enabled = false;
        filters.vignette.enabled = false;
        filters.scanlines.enabled = false;
        filters.chromaticAberration.enabled = false;
        filters.colorCorrection.enabled = false;
        filters.scope.enabled = false;
        filters.bloom.enabled = false;
        filters.falseColour.enabled = false;
        filters.edgeGlow.enabled = false;
        filters.lensDirt.enabled = false;
        filters.lensSoftness.enabled = false;
        filters.sharpen.enabled = false;
        state.settingsDirty = true;
        if (actions.refreshOverlay) {
            actions.refreshOverlay();
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset all")) {
        filters = kFilterDefaults;
        state.settingsDirty = true;
        if (actions.refreshOverlay) {
            actions.refreshOverlay();
        }
    }

    ImGui::Spacing();

    if (!ImGui::BeginChild("##filters", ImVec2(0, 0))) {
        ImGui::EndChild();
        return;
    }

    // Order follows the render pipeline in ADR-0003, ADR-0008 and ADR-0009, so the panel reads
    // the same way the shader runs.
    bool changed = false;
    changed |= DrawDistortion(filters.distortion, m_advancedDistortion);
    changed |= DrawChromatic(filters.chromaticAberration, m_advancedChromatic);
    changed |= DrawLensSoftness(filters.lensSoftness, m_advancedLensSoftness);
    changed |= DrawBloom(filters.bloom, m_advancedBloom);
    changed |= DrawSharpen(filters.sharpen, m_advancedSharpen);
    changed |= DrawColorCorrection(filters.colorCorrection, m_advancedColor);
    changed |= DrawFalseColour(filters.falseColour, m_advancedFalseColour);
    changed |= DrawEdgeGlow(filters.edgeGlow, m_advancedEdgeGlow);
    changed |= DrawScanlines(filters.scanlines, m_advancedScanlines);
    changed |= DrawVignette(filters.vignette, m_advancedVignette);
    changed |= DrawLensDirt(filters.lensDirt, m_advancedLensDirt);
    changed |= DrawScope(filters.scope, m_advancedScope);

    ImGui::EndChild();

    if (changed) {
        state.settingsDirty = true;
        // The new constants only reach the screen on the next rendered frame, and a paused
        // emulator will not produce one on its own.
        if (actions.refreshOverlay) {
            actions.refreshOverlay();
        }
    }
}

}  // namespace overlaydesk::ui
