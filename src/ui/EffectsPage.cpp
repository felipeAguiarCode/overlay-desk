// UI-SPEC.md "Effects" tab. Effects differ from filters in that they introduce dynamic
// behaviour rather than a continuous transformation (FILTERS-AND-EFFECTS.md section 3), so
// every card here has a speed or a frequency alongside its intensity.
//
// All of them are procedural and driven by time. None keeps a previous frame, which is what
// ADR-0003 requires and what keeps the renderer single-pass. The one exception to "driven by
// time" is deliberate and documented on its own card: lens dirt, over on the Filters tab, does
// not move, because dirt on glass does not.

#include <imgui.h>

#include "ui/ControlPanel.h"

namespace overlaydesk::ui {
namespace {

const GlitchSettings kGlitchDefaults;
const NoiseSettings kNoiseDefaults;
const FlickerSettings kFlickerDefaults;
const JitterSettings kJitterDefaults;
const ShimmerSettings kShimmerDefaults;
const RollingShutterSettings kRollingShutterDefaults;
const ScanSweepSettings kScanSweepDefaults;

constexpr const char* kGlitchStyles[] = {"Analog (tape / RF)", "Digital (macroblocks)"};

bool DrawGlitch(GlitchSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("GLITCH", &s.enabled, &reset);
    if (reset) {
        s = kGlitchDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);

    int style = static_cast<int>(s.style);
    if (ImGui::Combo("Style", &style, kGlitchStyles, IM_ARRAYSIZE(kGlitchStyles))) {
        s.style = static_cast<GlitchStyle>(style);
        changed = true;
    }
    widgets::HelpText(
        "Analog tears the picture into displaced bands and separates the channels, the way "
        "tape and RF fail. Digital loses whole macroblocks instead - inside a block the image "
        "is untouched, which is what a compressed drone downlink looks like breaking up.");

    // FILTERS-AND-EFFECTS.md 3.1 is explicit that these two mean different things and are
    // independent of each other, so they sit together at the top rather than one of them
    // being buried under Advanced.
    changed |= widgets::PercentSlider("Intensity", &s.intensity, kGlitchDefaults.intensity);
    changed |= widgets::PercentSlider("Frequency", &s.frequency, kGlitchDefaults.frequency);
    widgets::HelpText(
        "Frequency is how often a burst happens; intensity is how hard it hits when it "
        "does. Changing one never changes the other.");

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        changed |= widgets::PercentSlider("Block size", &s.blockSize, kGlitchDefaults.blockSize);
        changed |= widgets::PercentSlider("Jitter", &s.jitter, kGlitchDefaults.jitter);
        changed |= widgets::PercentSlider("RGB shift", &s.rgbShift, kGlitchDefaults.rgbShift);
        widgets::HelpText(
            "Block size sets the height of the displaced bands and, with it, how fine the "
            "per-line jitter is. RGB shift separates the channels horizontally during a "
            "burst.");
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

bool DrawNoise(NoiseSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("NOISE", &s.enabled, &reset);
    if (reset) {
        s = kNoiseDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);
    changed |= widgets::PercentSlider("Intensity", &s.intensity, kNoiseDefaults.intensity);

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        changed |= widgets::PercentSlider("Grain size", &s.grainSize, kNoiseDefaults.grainSize);
        changed |= widgets::PercentSlider("Speed", &s.speed, kNoiseDefaults.speed);
        changed |= widgets::PercentSlider("Colour", &s.colorAmount, kNoiseDefaults.colorAmount);
        widgets::HelpText(
            "Grain size makes the speckles chunkier. Speed is how often the pattern re-rolls "
            "- it holds still between rolls, the way film grain does, rather than crawling. "
            "Colour at 0% is monochrome grain; at 100% each channel gets its own speckle.");
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

bool DrawFlicker(FlickerSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("FLICKER", &s.enabled, &reset);
    if (reset) {
        s = kFlickerDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);
    changed |= widgets::PercentSlider("Intensity", &s.intensity, kFlickerDefaults.intensity);
    changed |= widgets::PercentSlider("Speed", &s.speed, kFlickerDefaults.speed);

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        widgets::HelpText(
            "The whole image breathes, the way a tube or a projector lamp does. Two "
            "unrelated rates are mixed so the pulse never settles into a recognisable loop. "
            "The swing is capped at half the slider - a full-range one would be a strobe, "
            "not a flicker.");
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

bool DrawJitter(JitterSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("JITTER", &s.enabled, &reset);
    if (reset) {
        s = kJitterDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);
    changed |= widgets::PercentSlider("Intensity", &s.intensity, kJitterDefaults.intensity);
    changed |= widgets::PercentSlider("Speed", &s.speed, kJitterDefaults.speed);

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        widgets::HelpText(
            "The whole frame wanders, like an unstable signal or an unsteady camera. This is "
            "not the same as the glitch's own jitter, which only exists during a burst and "
            "displaces individual lines rather than the picture as a whole.");
        ImGui::Spacing();
        widgets::HelpText(
            "At high intensity the edges of the frame come into view as the picture moves off "
            "them - that is the shake being honest, not a bug.");
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

bool DrawShimmer(ShimmerSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("SHIMMER", &s.enabled, &reset);
    if (reset) {
        s = kShimmerDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);
    changed |= widgets::PercentSlider("Intensity", &s.intensity, kShimmerDefaults.intensity);
    changed |= widgets::PercentSlider("Speed", &s.speed, kShimmerDefaults.speed);
    widgets::HelpText(
        "The image ripples, the way it does through hot air or behind an optical camouflage "
        "field. Unlike Jitter, which moves the whole frame as one piece, this bends it.");

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        changed |= widgets::PercentSlider("Scale", &s.scale, kShimmerDefaults.scale);
        widgets::HelpText(
            "Low scale is the fine boil right above hot metal; high scale is a slow swell "
            "across the whole picture.");
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

bool DrawRollingShutter(RollingShutterSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("ROLLING SHUTTER", &s.enabled, &reset);
    if (reset) {
        s = kRollingShutterDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);
    changed |= widgets::PercentSlider("Intensity", &s.intensity, kRollingShutterDefaults.intensity);
    changed |= widgets::PercentSlider("Speed", &s.speed, kRollingShutterDefaults.speed);

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        widgets::HelpText(
            "A cheap sensor reads the image one row at a time. Move the camera during that "
            "readout and the rows stop lining up, so the picture leans. It pivots about the "
            "middle row - shifting every row by the same amount would be Jitter instead.");
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

bool DrawScanSweep(ScanSweepSettings& s, bool& advancedOpen) {
    bool reset = false;
    bool changed = widgets::BeginModuleCard("SCAN SWEEP", &s.enabled, &reset);
    if (reset) {
        s = kScanSweepDefaults;
        changed = true;
    }

    ImGui::BeginDisabled(!s.enabled);
    changed |= widgets::PercentSlider("Intensity", &s.intensity, kScanSweepDefaults.intensity);
    changed |= widgets::PercentSlider("Speed", &s.speed, kScanSweepDefaults.speed);

    if (widgets::BeginAdvanced("adv", &advancedOpen)) {
        changed |= widgets::PercentSlider("Width", &s.width, kScanSweepDefaults.width);
        widgets::HelpText(
            "A bar of light crossing the picture, the way a scanning sensor sweeps. It "
            "brightens the instant it arrives and fades behind itself, which is what a beam "
            "does - a symmetric bar would read as a moving stripe.");
        widgets::EndAdvanced();
    }

    ImGui::EndDisabled();
    widgets::EndModuleCard();
    return changed;
}

}  // namespace

void ControlPanel::DrawEffectsPage(AppState& state, const PanelActions& actions) {
    EffectSettings& effects = state.settings.effects;

    ImGui::Spacing();

    if (ImGui::SmallButton("Disable all")) {
        effects.glitch.enabled = false;
        effects.noise.enabled = false;
        effects.flicker.enabled = false;
        effects.jitter.enabled = false;
        effects.shimmer.enabled = false;
        effects.rollingShutter.enabled = false;
        effects.scanSweep.enabled = false;
        state.settingsDirty = true;
        if (actions.refreshOverlay) {
            actions.refreshOverlay();
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset all")) {
        effects = EffectSettings{};
        state.settingsDirty = true;
        if (actions.refreshOverlay) {
            actions.refreshOverlay();
        }
    }

    ImGui::Spacing();

    if (!ImGui::BeginChild("##effects", ImVec2(0, 0))) {
        ImGui::EndChild();
        return;
    }

    // Order follows the render pipeline in ADR-0007 and ADR-0009: the three that displace the
    // UV come first, in the order the light meets them, then the ones that act on the
    // finished colour.
    bool changed = false;
    changed |= DrawJitter(effects.jitter, m_advancedJitter);
    changed |= DrawShimmer(effects.shimmer, m_advancedShimmer);
    changed |= DrawRollingShutter(effects.rollingShutter, m_advancedRollingShutter);
    changed |= DrawGlitch(effects.glitch, m_advancedGlitch);
    changed |= DrawNoise(effects.noise, m_advancedNoise);
    changed |= DrawScanSweep(effects.scanSweep, m_advancedScanSweep);
    changed |= DrawFlicker(effects.flicker, m_advancedFlicker);

    ImGui::EndChild();

    if (changed) {
        state.settingsDirty = true;
        // The new constants only reach the screen on the next rendered frame, and a paused
        // source will not produce one on its own.
        if (actions.refreshOverlay) {
            actions.refreshOverlay();
        }
    }
}

}  // namespace overlaydesk::ui
