// Unit tests for the pure geometry in src/core/RenderMath.h.
//
// These are the calculations the renderer cannot easily be eyeballed for: a letterbox that
// is one pixel off looks fine on screen but drifts every UV-anchored filter downstream.

#include "core/RenderMath.h"

#include <cassert>
#include <cmath>
#include <cstdio>

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

void TestAspectFitIdentity() {
    // Same aspect ratio: the source fills the overlay exactly, whatever the scale.
    const AspectFit fit = ComputeAspectFit(1920.0f, 1080.0f, 1280.0f, 720.0f);
    CHECK(Near(fit.scaleX, 1.0f));
    CHECK(Near(fit.scaleY, 1.0f));
    CHECK(Near(fit.offsetX, 0.0f));
    CHECK(Near(fit.offsetY, 0.0f));
}

void TestAspectFitPillarbox() {
    // A 4:3 source in a 16:9 overlay: full height, bars on the left and right.
    const AspectFit fit = ComputeAspectFit(640.0f, 480.0f, 1600.0f, 900.0f);
    CHECK(Near(fit.scaleY, 1.0f));
    CHECK(Near(fit.scaleX, (4.0f / 3.0f) / (16.0f / 9.0f)));
    CHECK(Near(fit.offsetY, 0.0f));
    CHECK(Near(fit.offsetX, (1.0f - fit.scaleX) * 0.5f));
    // Bars are symmetric.
    CHECK(Near(fit.offsetX + fit.scaleX + fit.offsetX, 1.0f));
}

void TestAspectFitLetterbox() {
    // A 21:9 source in a 4:3 overlay: full width, bars top and bottom.
    const AspectFit fit = ComputeAspectFit(2560.0f, 1080.0f, 800.0f, 600.0f);
    CHECK(Near(fit.scaleX, 1.0f));
    CHECK(fit.scaleY < 1.0f);
    CHECK(Near(fit.offsetX, 0.0f));
    CHECK(Near(fit.offsetY + fit.scaleY + fit.offsetY, 1.0f));
}

void TestAspectFitCentreMapsToCentre() {
    // The centre of the overlay must sample the centre of the source in every case, which
    // is what keeps distortion anchored (FILTERS-AND-EFFECTS.md: "centro estavel").
    const AspectFit cases[] = {
        ComputeAspectFit(640.0f, 480.0f, 1600.0f, 900.0f),
        ComputeAspectFit(2560.0f, 1080.0f, 800.0f, 600.0f),
        ComputeAspectFit(160.0f, 144.0f, 1024.0f, 1024.0f),
    };
    for (const AspectFit& fit : cases) {
        CHECK(Near((0.5f - fit.offsetX) / fit.scaleX, 0.5f));
        CHECK(Near((0.5f - fit.offsetY) / fit.scaleY, 0.5f));
    }
}

void TestAspectFitDegenerateInputs() {
    // Minimized windows and the moment before the first capture frame both produce zeroes;
    // identity is the only answer that will not put NaNs into the constant buffer.
    for (const AspectFit& fit : {ComputeAspectFit(0.0f, 1080.0f, 1280.0f, 720.0f),
                                 ComputeAspectFit(1920.0f, 0.0f, 1280.0f, 720.0f),
                                 ComputeAspectFit(1920.0f, 1080.0f, 0.0f, 720.0f),
                                 ComputeAspectFit(1920.0f, 1080.0f, 1280.0f, 0.0f),
                                 ComputeAspectFit(-1.0f, -1.0f, -1.0f, -1.0f)}) {
        CHECK(Near(fit.scaleX, 1.0f));
        CHECK(Near(fit.scaleY, 1.0f));
        CHECK(Near(fit.offsetX, 0.0f));
        CHECK(Near(fit.offsetY, 0.0f));
    }
}

void TestIntensityClamping() {
    CHECK(Near(ClampIntensity(-0.5f), 0.0f));
    CHECK(Near(ClampIntensity(0.0f), 0.0f));
    CHECK(Near(ClampIntensity(0.42f), 0.42f));
    CHECK(Near(ClampIntensity(1.0f), 1.0f));
    CHECK(Near(ClampIntensity(3.0f), 1.0f));
}

void TestBipolarClamping() {
    // RF-011 / AT-008: -1 anti-fisheye, 0 neutral, +1 fisheye, continuous through zero.
    CHECK(Near(ClampBipolar(-2.0f), -1.0f));
    CHECK(Near(ClampBipolar(-1.0f), -1.0f));
    CHECK(Near(ClampBipolar(-0.001f), -0.001f));
    CHECK(Near(ClampBipolar(0.0f), 0.0f));
    CHECK(Near(ClampBipolar(0.001f), 0.001f));
    CHECK(Near(ClampBipolar(1.0f), 1.0f));
    CHECK(Near(ClampBipolar(9.0f), 1.0f));
}

void TestAspectRatioParsing() {
    CHECK(Near(ParseAspectRatio("16:9"), 16.0f / 9.0f));
    CHECK(Near(ParseAspectRatio("4:3"), 4.0f / 3.0f));
    CHECK(Near(ParseAspectRatio("21:9"), 21.0f / 9.0f));
    CHECK(Near(ParseAspectRatio("1:1"), 1.0f));
    CHECK(Near(ParseAspectRatio("16/9"), 16.0f / 9.0f));
    CHECK(Near(ParseAspectRatio("1.5"), 1.5f, 1e-4f));

    // Anything unusable reports 0, which callers read as "no aspect constraint" rather
    // than locking the window to a nonsense shape.
    CHECK(Near(ParseAspectRatio(""), 0.0f));
    CHECK(Near(ParseAspectRatio("abc"), 0.0f));
    CHECK(Near(ParseAspectRatio("16:"), 0.0f));
    CHECK(Near(ParseAspectRatio(":9"), 0.0f));
    CHECK(Near(ParseAspectRatio("16:0"), 0.0f));
    CHECK(Near(ParseAspectRatio(nullptr), 0.0f));
}

void TestApplyAspectRatio() {
    // Dragging a vertical edge fixes the width and recomputes the height.
    const SizeI byWidth = ApplyAspectRatio(SizeI{1600, 400}, 16.0f / 9.0f, /*adjustHeight=*/true);
    CHECK(byWidth.width == 1600);
    CHECK(byWidth.height == 900);

    // Dragging a horizontal edge does the opposite.
    const SizeI byHeight = ApplyAspectRatio(SizeI{100, 900}, 16.0f / 9.0f, /*adjustHeight=*/false);
    CHECK(byHeight.height == 900);
    CHECK(byHeight.width == 1600);

    // No ratio means no correction.
    const SizeI untouched = ApplyAspectRatio(SizeI{1234, 567}, 0.0f, true);
    CHECK(untouched.width == 1234);
    CHECK(untouched.height == 567);

    // The result never collapses to zero, which would make an unpresentable swap chain.
    const SizeI tiny = ApplyAspectRatio(SizeI{1, 1}, 100.0f, true);
    CHECK(tiny.width >= 1);
    CHECK(tiny.height >= 1);
}

}  // namespace

int main() {
    TestAspectFitIdentity();
    TestAspectFitPillarbox();
    TestAspectFitLetterbox();
    TestAspectFitCentreMapsToCentre();
    TestAspectFitDegenerateInputs();
    TestIntensityClamping();
    TestBipolarClamping();
    TestAspectRatioParsing();
    TestApplyAspectRatio();

    if (g_failures == 0) {
        std::printf("All RenderMath tests passed.\n");
        return 0;
    }
    std::printf("%d check(s) failed.\n", g_failures);
    return 1;
}
