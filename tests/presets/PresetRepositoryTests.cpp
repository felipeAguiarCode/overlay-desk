// Round-trip and naming tests for PresetRepository.
//
// AT-013 says: save a preset, change the values, reapply it, and the saved values come back.
// Applying is a UI gesture, but the half that can silently rot - every parameter surviving
// the trip through JSON and back - is exactly what belongs in a test.
//
// These write to a scratch directory under the system temp path and clean it up afterwards.

#include "core/PresetRepository.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what, int line) {
    if (!condition) {
        ++g_failures;
        std::printf("FAIL (line %d): %s\n", line, what);
    }
}

bool Near(float a, float b, float tolerance = 1e-5f) {
    return std::fabs(a - b) <= tolerance;
}

#define CHECK(expr) Check((expr), #expr, __LINE__)

using namespace overlaydesk;

std::filesystem::path ScratchDirectory() {
    return std::filesystem::temp_directory_path() / "overlaydesk-preset-tests";
}

void ResetScratch() {
    std::error_code ec;
    std::filesystem::remove_all(ScratchDirectory(), ec);
    std::filesystem::create_directories(ScratchDirectory(), ec);
}

// A preset with every field pushed off its default, so a dropped field cannot pass by
// coincidentally matching the default.
Preset MakeDistinctivePreset(std::string name) {
    Preset p;
    p.name = std::move(name);
    p.builtIn = false;
    p.category = "Test Group";

    p.filters.distortion.enabled = true;
    p.filters.distortion.intensity = 0.61f;
    p.filters.distortion.amount = -0.73f;

    p.filters.vignette.enabled = true;
    p.filters.vignette.intensity = 0.42f;
    p.filters.vignette.size = 0.31f;
    p.filters.vignette.softness = 0.83f;
    p.filters.vignette.roundness = 0.17f;

    p.filters.distortion.shape = DistortionShape::Corner;

    p.filters.scanlines.enabled = true;
    p.filters.scanlines.intensity = 0.55f;
    p.filters.scanlines.thickness = 2.5f;
    p.filters.scanlines.spacing = 7.0f;
    p.filters.scanlines.style = ScanlineStyle::SlotMask;
    p.filters.scanlines.orientation = ScanlineOrientation::Grid;
    p.filters.scanlines.scaleMode = ScanlineScaleMode::Relative;
    p.filters.scanlines.beamWidth = 0.73f;
    p.filters.scanlines.interlace = 0.41f;

    p.filters.chromaticAberration.enabled = true;
    p.filters.chromaticAberration.intensity = 0.28f;
    p.filters.chromaticAberration.mode = ChromaticAberrationMode::Vertical;
    p.filters.chromaticAberration.edgeBias = 0.36f;
    p.filters.chromaticAberration.redShift = -1.4f;
    p.filters.chromaticAberration.blueShift = 0.9f;

    p.filters.colorCorrection.enabled = true;
    p.filters.colorCorrection.intensity = 0.77f;
    p.filters.colorCorrection.brightness = -0.12f;
    p.filters.colorCorrection.contrast = 1.42f;
    p.filters.colorCorrection.saturation = 0.34f;
    p.filters.colorCorrection.gamma = 1.63f;
    p.filters.colorCorrection.tint[0] = 0.19f;
    p.filters.colorCorrection.tint[1] = 0.71f;
    p.filters.colorCorrection.tint[2] = 0.43f;
    p.filters.colorCorrection.tintAmount = 0.87f;
    p.filters.colorCorrection.posterize = 0.39f;

    p.filters.scope.enabled = true;
    p.filters.scope.intensity = 0.81f;
    p.filters.scope.size = 0.47f;
    p.filters.scope.softness = 0.23f;
    p.filters.scope.magnification = 2.75f;
    p.filters.scope.reticle = 0.68f;
    p.filters.scope.shape = ScopeShape::QuadTube;

    p.filters.bloom.enabled = true;
    p.filters.bloom.intensity = 0.53f;
    p.filters.bloom.threshold = 0.29f;
    p.filters.bloom.radius = 0.91f;
    p.filters.bloom.tint[0] = 1.00f;
    p.filters.bloom.tint[1] = 0.62f;
    p.filters.bloom.tint[2] = 0.38f;

    p.filters.falseColour.enabled = true;
    p.filters.falseColour.intensity = 0.64f;
    p.filters.falseColour.palette = FalseColourPalette::Ironbow;
    p.filters.falseColour.levels = 0.37f;

    p.filters.edgeGlow.enabled = true;
    p.filters.edgeGlow.intensity = 0.72f;
    p.filters.edgeGlow.width = 0.26f;
    p.filters.edgeGlow.tint[0] = 0.93f;
    p.filters.edgeGlow.tint[1] = 0.11f;
    p.filters.edgeGlow.tint[2] = 0.58f;

    p.filters.lensDirt.enabled = true;
    p.filters.lensDirt.intensity = 0.44f;
    p.filters.lensDirt.density = 0.86f;
    p.filters.lensDirt.smear = 0.15f;

    p.filters.lensSoftness.enabled = true;
    p.filters.lensSoftness.intensity = 0.67f;
    p.filters.lensSoftness.center = 0.21f;

    p.effects.glitch.enabled = true;
    p.effects.glitch.intensity = 0.66f;
    p.effects.glitch.frequency = 0.21f;
    p.effects.glitch.blockSize = 0.13f;
    p.effects.glitch.jitter = 0.47f;
    p.effects.glitch.rgbShift = 0.58f;
    p.effects.glitch.style = GlitchStyle::Digital;

    p.effects.noise.enabled = true;
    p.effects.noise.intensity = 0.38f;
    p.effects.noise.grainSize = 0.74f;
    p.effects.noise.speed = 0.29f;
    p.effects.noise.colorAmount = 0.63f;

    p.effects.flicker.enabled = true;
    p.effects.flicker.intensity = 0.52f;
    p.effects.flicker.speed = 0.18f;

    p.effects.jitter.enabled = true;
    p.effects.jitter.intensity = 0.27f;
    p.effects.jitter.speed = 0.94f;

    p.effects.shimmer.enabled = true;
    p.effects.shimmer.intensity = 0.35f;
    p.effects.shimmer.speed = 0.62f;
    p.effects.shimmer.scale = 0.88f;

    p.effects.rollingShutter.enabled = true;
    p.effects.rollingShutter.intensity = 0.49f;
    p.effects.rollingShutter.speed = 0.16f;

    p.effects.scanSweep.enabled = true;
    p.effects.scanSweep.intensity = 0.71f;
    p.effects.scanSweep.speed = 0.24f;
    p.effects.scanSweep.width = 0.57f;

    return p;
}

void ExpectSame(const Preset& expected, const Preset& actual) {
    CHECK(expected.name == actual.name);
    CHECK(expected.category == actual.category);

    const FilterSettings& e = expected.filters;
    const FilterSettings& a = actual.filters;

    CHECK(e.distortion.enabled == a.distortion.enabled);
    CHECK(Near(e.distortion.intensity, a.distortion.intensity));
    CHECK(Near(e.distortion.amount, a.distortion.amount));
    CHECK(e.distortion.shape == a.distortion.shape);

    CHECK(e.vignette.enabled == a.vignette.enabled);
    CHECK(Near(e.vignette.intensity, a.vignette.intensity));
    CHECK(Near(e.vignette.size, a.vignette.size));
    CHECK(Near(e.vignette.softness, a.vignette.softness));
    CHECK(Near(e.vignette.roundness, a.vignette.roundness));

    CHECK(e.scanlines.enabled == a.scanlines.enabled);
    CHECK(Near(e.scanlines.intensity, a.scanlines.intensity));
    CHECK(Near(e.scanlines.thickness, a.scanlines.thickness));
    CHECK(Near(e.scanlines.spacing, a.scanlines.spacing));
    CHECK(e.scanlines.style == a.scanlines.style);
    CHECK(e.scanlines.orientation == a.scanlines.orientation);
    CHECK(e.scanlines.scaleMode == a.scanlines.scaleMode);
    CHECK(Near(e.scanlines.beamWidth, a.scanlines.beamWidth));
    CHECK(Near(e.scanlines.interlace, a.scanlines.interlace));

    CHECK(e.chromaticAberration.enabled == a.chromaticAberration.enabled);
    CHECK(Near(e.chromaticAberration.intensity, a.chromaticAberration.intensity));
    CHECK(e.chromaticAberration.mode == a.chromaticAberration.mode);
    CHECK(Near(e.chromaticAberration.edgeBias, a.chromaticAberration.edgeBias));
    CHECK(Near(e.chromaticAberration.redShift, a.chromaticAberration.redShift));
    CHECK(Near(e.chromaticAberration.blueShift, a.chromaticAberration.blueShift));

    CHECK(e.colorCorrection.enabled == a.colorCorrection.enabled);
    CHECK(Near(e.colorCorrection.intensity, a.colorCorrection.intensity));
    CHECK(Near(e.colorCorrection.brightness, a.colorCorrection.brightness));
    CHECK(Near(e.colorCorrection.contrast, a.colorCorrection.contrast));
    CHECK(Near(e.colorCorrection.saturation, a.colorCorrection.saturation));
    CHECK(Near(e.colorCorrection.gamma, a.colorCorrection.gamma));
    CHECK(Near(e.colorCorrection.tint[0], a.colorCorrection.tint[0]));
    CHECK(Near(e.colorCorrection.tint[1], a.colorCorrection.tint[1]));
    CHECK(Near(e.colorCorrection.tint[2], a.colorCorrection.tint[2]));
    CHECK(Near(e.colorCorrection.tintAmount, a.colorCorrection.tintAmount));
    CHECK(Near(e.colorCorrection.posterize, a.colorCorrection.posterize));

    CHECK(e.scope.enabled == a.scope.enabled);
    CHECK(Near(e.scope.intensity, a.scope.intensity));
    CHECK(Near(e.scope.size, a.scope.size));
    CHECK(Near(e.scope.softness, a.scope.softness));
    CHECK(Near(e.scope.magnification, a.scope.magnification));
    CHECK(Near(e.scope.reticle, a.scope.reticle));
    CHECK(e.scope.shape == a.scope.shape);

    CHECK(e.bloom.enabled == a.bloom.enabled);
    CHECK(Near(e.bloom.intensity, a.bloom.intensity));
    CHECK(Near(e.bloom.threshold, a.bloom.threshold));
    CHECK(Near(e.bloom.radius, a.bloom.radius));
    CHECK(Near(e.bloom.tint[0], a.bloom.tint[0]));
    CHECK(Near(e.bloom.tint[1], a.bloom.tint[1]));
    CHECK(Near(e.bloom.tint[2], a.bloom.tint[2]));

    CHECK(e.falseColour.enabled == a.falseColour.enabled);
    CHECK(Near(e.falseColour.intensity, a.falseColour.intensity));
    CHECK(e.falseColour.palette == a.falseColour.palette);
    CHECK(Near(e.falseColour.levels, a.falseColour.levels));

    CHECK(e.edgeGlow.enabled == a.edgeGlow.enabled);
    CHECK(Near(e.edgeGlow.intensity, a.edgeGlow.intensity));
    CHECK(Near(e.edgeGlow.width, a.edgeGlow.width));
    CHECK(Near(e.edgeGlow.tint[0], a.edgeGlow.tint[0]));
    CHECK(Near(e.edgeGlow.tint[1], a.edgeGlow.tint[1]));
    CHECK(Near(e.edgeGlow.tint[2], a.edgeGlow.tint[2]));

    CHECK(e.lensSoftness.enabled == a.lensSoftness.enabled);
    CHECK(Near(e.lensSoftness.intensity, a.lensSoftness.intensity));
    CHECK(Near(e.lensSoftness.center, a.lensSoftness.center));

    CHECK(e.lensDirt.enabled == a.lensDirt.enabled);
    CHECK(Near(e.lensDirt.intensity, a.lensDirt.intensity));
    CHECK(Near(e.lensDirt.density, a.lensDirt.density));
    CHECK(Near(e.lensDirt.smear, a.lensDirt.smear));

    const EffectSettings& ee = expected.effects;
    const EffectSettings& ae = actual.effects;

    CHECK(ee.glitch.enabled == ae.glitch.enabled);
    CHECK(Near(ee.glitch.intensity, ae.glitch.intensity));
    CHECK(Near(ee.glitch.frequency, ae.glitch.frequency));
    CHECK(Near(ee.glitch.blockSize, ae.glitch.blockSize));
    CHECK(Near(ee.glitch.jitter, ae.glitch.jitter));
    CHECK(Near(ee.glitch.rgbShift, ae.glitch.rgbShift));
    CHECK(ee.glitch.style == ae.glitch.style);

    CHECK(ee.noise.enabled == ae.noise.enabled);
    CHECK(Near(ee.noise.intensity, ae.noise.intensity));
    CHECK(Near(ee.noise.grainSize, ae.noise.grainSize));
    CHECK(Near(ee.noise.speed, ae.noise.speed));
    CHECK(Near(ee.noise.colorAmount, ae.noise.colorAmount));

    CHECK(ee.flicker.enabled == ae.flicker.enabled);
    CHECK(Near(ee.flicker.intensity, ae.flicker.intensity));
    CHECK(Near(ee.flicker.speed, ae.flicker.speed));

    CHECK(ee.jitter.enabled == ae.jitter.enabled);
    CHECK(Near(ee.jitter.intensity, ae.jitter.intensity));
    CHECK(Near(ee.jitter.speed, ae.jitter.speed));

    CHECK(ee.shimmer.enabled == ae.shimmer.enabled);
    CHECK(Near(ee.shimmer.intensity, ae.shimmer.intensity));
    CHECK(Near(ee.shimmer.speed, ae.shimmer.speed));
    CHECK(Near(ee.shimmer.scale, ae.shimmer.scale));

    CHECK(ee.rollingShutter.enabled == ae.rollingShutter.enabled);
    CHECK(Near(ee.rollingShutter.intensity, ae.rollingShutter.intensity));
    CHECK(Near(ee.rollingShutter.speed, ae.rollingShutter.speed));

    CHECK(ee.scanSweep.enabled == ae.scanSweep.enabled);
    CHECK(Near(ee.scanSweep.intensity, ae.scanSweep.intensity));
    CHECK(Near(ee.scanSweep.speed, ae.scanSweep.speed));
    CHECK(Near(ee.scanSweep.width, ae.scanSweep.width));
}

const Preset* FindByName(const std::vector<Preset>& presets, const std::string& name) {
    const auto it = std::find_if(presets.begin(), presets.end(),
                                 [&name](const Preset& p) { return p.name == name; });
    return it != presets.end() ? &(*it) : nullptr;
}

// --- Tests ---------------------------------------------------------------------------------

// AT-013, data half: every parameter survives the trip to disk and back.
void TestRoundTrip() {
    ResetScratch();
    const PresetRepository repository(ScratchDirectory());

    const Preset original = MakeDistinctivePreset("Round Trip");
    CHECK(repository.Save(original));

    const std::vector<Preset> loaded = repository.LoadAll();
    CHECK(loaded.size() == 1);
    if (loaded.empty()) {
        return;
    }
    ExpectSame(original, loaded.front());

    // And the loaded copy must report itself as an exact match of the values it carries,
    // which is what lets the UI tell "Soft CRT" from "Soft CRT (edited)".
    CHECK(PresetMatches(loaded.front(), original.filters, original.effects));
}

void TestMatchDetectsEdits() {
    const Preset preset = MakeDistinctivePreset("Edited");

    FilterSettings filters = preset.filters;
    EffectSettings effects = preset.effects;
    CHECK(PresetMatches(preset, filters, effects));

    filters.vignette.softness += 0.05f;
    CHECK(!PresetMatches(preset, filters, effects));

    filters = preset.filters;
    filters.scanlines.orientation = ScanlineOrientation::Vertical;
    CHECK(!PresetMatches(preset, filters, effects));

    filters = preset.filters;
    filters.colorCorrection.enabled = !filters.colorCorrection.enabled;
    CHECK(!PresetMatches(preset, filters, effects));

    filters = preset.filters;
    effects.glitch.frequency += 0.02f;
    CHECK(!PresetMatches(preset, filters, effects));

    // One per module added after the original six, so a comparison that forgot a whole module
    // cannot pass. ADR-0009 makes this the difference between "Thermal" and "Thermal (edited)"
    // for looks whose entire character lives in one of these fields.
    effects = preset.effects;
    filters.falseColour.palette = FalseColourPalette::BlackHot;
    CHECK(!PresetMatches(preset, filters, effects));

    filters = preset.filters;
    filters.scanlines.beamWidth += 0.05f;
    CHECK(!PresetMatches(preset, filters, effects));

    filters = preset.filters;
    filters.bloom.tint[0] -= 0.05f;
    CHECK(!PresetMatches(preset, filters, effects));

    filters = preset.filters;
    filters.bloom.threshold += 0.05f;
    CHECK(!PresetMatches(preset, filters, effects));

    filters = preset.filters;
    filters.edgeGlow.tint[1] += 0.05f;
    CHECK(!PresetMatches(preset, filters, effects));

    filters = preset.filters;
    filters.lensSoftness.center += 0.05f;
    CHECK(!PresetMatches(preset, filters, effects));

    filters = preset.filters;
    filters.lensDirt.smear += 0.05f;
    CHECK(!PresetMatches(preset, filters, effects));

    filters = preset.filters;
    filters.colorCorrection.posterize += 0.05f;
    CHECK(!PresetMatches(preset, filters, effects));

    filters = preset.filters;
    filters.scope.magnification += 0.25f;
    CHECK(!PresetMatches(preset, filters, effects));

    effects.glitch.style = GlitchStyle::Analog;
    CHECK(!PresetMatches(preset, filters, effects));

    effects = preset.effects;
    effects.shimmer.scale += 0.05f;
    CHECK(!PresetMatches(preset, filters, effects));

    effects = preset.effects;
    effects.rollingShutter.speed += 0.05f;
    CHECK(!PresetMatches(preset, filters, effects));

    effects = preset.effects;
    effects.scanSweep.width += 0.05f;
    CHECK(!PresetMatches(preset, filters, effects));

    effects = preset.effects;
    effects.noise.colorAmount += 0.05f;
    CHECK(!PresetMatches(preset, filters, effects));

    effects = preset.effects;
    effects.flicker.speed += 0.05f;
    CHECK(!PresetMatches(preset, filters, effects));

    effects = preset.effects;
    effects.jitter.speed += 0.05f;
    CHECK(!PresetMatches(preset, filters, effects));
}

void TestBuiltInSeeding() {
    ResetScratch();
    std::error_code ec;
    std::filesystem::remove_all(ScratchDirectory(), ec);

    const PresetRepository repository(ScratchDirectory());
    const size_t expected = BuiltInPresets().size();

    // First run: everything is new, so everything is written.
    std::vector<std::string> seeded = repository.SeedMissingBuiltIns({});
    CHECK(seeded.size() == expected);
    CHECK(repository.LoadAll().size() == expected);

    // Second run with the same seeded list writes nothing.
    CHECK(repository.SeedMissingBuiltIns(seeded).empty());
    CHECK(repository.LoadAll().size() == expected);

    // Upgrading from a build that had no seeded list: the files are all on disk but the list
    // is empty. Every name must be reported as offered, and no file may be touched - one of
    // them could be a built-in the user has edited in place.
    const std::filesystem::path sample = repository.FilePathFor(seeded.front());
    const auto stamp = std::filesystem::last_write_time(sample);
    const std::vector<std::string> rediscovered = repository.SeedMissingBuiltIns({});
    CHECK(rediscovered.size() == expected);
    CHECK(std::filesystem::last_write_time(sample) == stamp);
    CHECK(repository.LoadAll().size() == expected);

    // Deleting one and re-running must not resurrect it: a user who removed a preset meant it.
    const std::vector<Preset> presets = repository.LoadAll();
    CHECK(!presets.empty());
    if (!presets.empty()) {
        CHECK(repository.Delete(presets.front()));
    }
    CHECK(repository.SeedMissingBuiltIns(seeded).empty());
    CHECK(repository.LoadAll().size() == expected - 1);

    // A preset the installation has never offered - the case of a new version adding one -
    // is written even though the directory is long since populated.
    std::vector<std::string> partial = seeded;
    const std::string dropped = partial.back();
    partial.pop_back();
    // Its file is still on disk from the first seeding, so removing it proves the write.
    for (const Preset& candidate : repository.LoadAll()) {
        if (candidate.name == dropped) {
            CHECK(repository.Delete(candidate));
        }
    }
    const std::vector<std::string> added = repository.SeedMissingBuiltIns(partial);
    CHECK(added.size() == 1);
    if (!added.empty()) {
        CHECK(added.front() == dropped);
    }
}

void TestRenameAndDuplicateNaming() {
    ResetScratch();
    const PresetRepository repository(ScratchDirectory());

    const Preset original = MakeDistinctivePreset("My Look");
    CHECK(repository.Save(original));

    std::vector<Preset> presets = repository.LoadAll();
    CHECK(presets.size() == 1);

    // Duplicating must not collide with the original.
    const std::string copyName = PresetRepository::MakeUniqueName("My Look", presets);
    CHECK(copyName == "My Look (2)");

    Preset copy = original;
    copy.name = copyName;
    copy.file.clear();
    CHECK(repository.Save(copy));

    presets = repository.LoadAll();
    CHECK(presets.size() == 2);
    CHECK(PresetRepository::MakeUniqueName("My Look", presets) == "My Look (3)");

    // Renaming moves the file rather than leaving an orphan behind.
    const Preset* toRename = FindByName(presets, "My Look");
    CHECK(toRename != nullptr);
    if (toRename != nullptr) {
        CHECK(repository.Rename(*toRename, "Renamed Look"));
    }

    presets = repository.LoadAll();
    CHECK(presets.size() == 2);
    CHECK(FindByName(presets, "Renamed Look") != nullptr);
    CHECK(FindByName(presets, "My Look") == nullptr);

    // The renamed preset keeps every value it had.
    const Preset* renamed = FindByName(presets, "Renamed Look");
    CHECK(renamed != nullptr);
    if (renamed != nullptr) {
        Preset expected = original;
        expected.name = "Renamed Look";
        ExpectSame(expected, *renamed);
    }
}

void TestFileStemAndSanitize() {
    CHECK(PresetRepository::MakeFileStem("Soft CRT") == "soft-crt");
    CHECK(PresetRepository::MakeFileStem("Game Boy Advance") == "game-boy-advance");
    CHECK(PresetRepository::MakeFileStem("My Look (2)") == "my-look-2");
    CHECK(PresetRepository::MakeFileStem("  spaced  out  ") == "spaced-out");
    // Nothing usable left: the preset still needs a file to live in.
    CHECK(PresetRepository::MakeFileStem("***") == "preset");
    CHECK(PresetRepository::MakeFileStem("") == "preset");

    // A name that would escape the presets directory must not be able to.
    const std::string traversal = PresetRepository::MakeFileStem("../../etc/passwd");
    CHECK(traversal.find('.') == std::string::npos);
    CHECK(traversal.find('/') == std::string::npos);
    CHECK(traversal.find('\\') == std::string::npos);

    CHECK(PresetRepository::SanitizeName("  Soft   CRT  ") == "Soft CRT");
    CHECK(PresetRepository::SanitizeName("   ").empty());
}

// Every preset file written before the category field existed carries no category, and
// SeedMissingBuiltIns deliberately never rewrites a file that is already on disk. Without the
// recovery by name, an upgrade would file every shipped preset the user already had under
// "Custom" - which is the whole population on any existing installation.
void TestCategoryRecoveredForOlderFiles() {
    ResetScratch();
    const PresetRepository repository(ScratchDirectory());

    // Save() would create this on the way past; these files are written by hand precisely
    // because they have to look like something an older build left behind.
    std::error_code ec;
    std::filesystem::create_directories(repository.Directory(), ec);

    const std::vector<Preset> shipped = BuiltInPresets();
    CHECK(!shipped.empty());
    if (shipped.empty()) {
        return;
    }

    // A shipped preset whose category is something other than the Custom fallback, so a
    // failure to recover cannot pass by landing on the right answer accidentally.
    const Preset* sample = nullptr;
    for (const Preset& candidate : shipped) {
        if (candidate.category != kCategoryCustom && !candidate.category.empty()) {
            sample = &candidate;
            break;
        }
    }
    CHECK(sample != nullptr);
    if (sample == nullptr) {
        return;
    }

    // Exactly what an older build wrote: no "category" key at all.
    const std::filesystem::path path = repository.FilePathFor(sample->name);
    {
        std::ofstream file(path, std::ios::binary);
        file << "{\"schemaVersion\":1,\"name\":\"" << sample->name
             << "\",\"builtIn\":true,\"filters\":{},\"effects\":{}}";
    }

    const std::vector<Preset> loaded = repository.LoadAll();
    CHECK(loaded.size() == 1);
    if (!loaded.empty()) {
        CHECK(loaded.front().name == sample->name);
        CHECK(loaded.front().category == sample->category);
    }

    // A name that is not shipped has nothing to recover from, and lands in Custom.
    {
        std::ofstream file(repository.FilePathFor("Not A Shipped Name"), std::ios::binary);
        file << "{\"schemaVersion\":1,\"name\":\"Not A Shipped Name\",\"filters\":{},"
                "\"effects\":{}}";
    }
    // Named, not a temporary: FindByName hands back a pointer into the vector, and one into a
    // temporary would dangle the moment the expression ended.
    const std::vector<Preset> reloaded = repository.LoadAll();
    const Preset* stranger = FindByName(reloaded, "Not A Shipped Name");
    CHECK(stranger != nullptr);
    if (stranger != nullptr) {
        CHECK(stranger->category == kCategoryCustom);
    }
}

// Every shipped preset has to be filed somewhere, and the grouping is only useful if the
// headings are the ones the panel knows how to order.
void TestEveryBuiltInHasAKnownCategory() {
    const std::vector<Preset> shipped = BuiltInPresets();
    const int unknownRank = PresetCategoryRank("something nobody defined");

    for (const Preset& preset : shipped) {
        if (preset.category.empty()) {
            std::printf("FAIL: built-in '%s' has no category\n", preset.name.c_str());
            ++g_failures;
        } else if (PresetCategoryRank(preset.category) >= unknownRank) {
            std::printf("FAIL: built-in '%s' is in unranked category '%s'\n", preset.name.c_str(),
                        preset.category.c_str());
            ++g_failures;
        }
    }
}

// AT-018 in spirit: one unreadable preset must not take the rest of the list with it.
void TestCorruptPresetIsSkipped() {
    ResetScratch();
    const PresetRepository repository(ScratchDirectory());

    CHECK(repository.Save(MakeDistinctivePreset("Good One")));

    {
        std::ofstream broken(ScratchDirectory() / "broken.json", std::ios::binary);
        broken << "{ this is not json, ,, ";
    }
    {
        std::ofstream future(ScratchDirectory() / "future.json", std::ios::binary);
        future << R"({ "schemaVersion": 99, "name": "From The Future" })";
    }

    const std::vector<Preset> presets = repository.LoadAll();
    CHECK(presets.size() == 1);
    CHECK(FindByName(presets, "Good One") != nullptr);
    CHECK(FindByName(presets, "From The Future") == nullptr);
}

}  // namespace

int main() {
    TestRoundTrip();
    TestMatchDetectsEdits();
    TestBuiltInSeeding();
    TestRenameAndDuplicateNaming();
    TestFileStemAndSanitize();
    TestCategoryRecoveredForOlderFiles();
    TestEveryBuiltInHasAKnownCategory();
    TestCorruptPresetIsSkipped();

    std::error_code ec;
    std::filesystem::remove_all(ScratchDirectory(), ec);

    if (g_failures == 0) {
        std::printf("All PresetRepository tests passed.\n");
        return 0;
    }
    std::printf("%d check(s) failed.\n", g_failures);
    return 1;
}
