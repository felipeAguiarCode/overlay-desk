#pragma once

// Header-only, side-effect free geometry helpers shared by the renderer and the unit
// tests. Nothing here may pull in Windows or D3D headers - tests/math links against this
// file alone.

#include <algorithm>
#include <cmath>

namespace overlaydesk {

// ARCHITECTURE.md section 6 / OVERLAY-WINDOW.md: source and overlay have independent
// dimensions, so the renderer must scale one into the other. We letterbox rather than
// stretch, because a stretched emulator frame breaks every aspect-sensitive filter
// downstream (scanline spacing, vignette roundness, radial chromatic aberration).
//
// The result maps normalized output UV to normalized source UV:
//
//     sourceUv = (outputUv - offset) / scale
//
// Components of sourceUv outside [0, 1] fall in the letterbox bars.
struct AspectFit {
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
};

// Degenerate inputs (a zero or negative extent, which happens while a window is minimized
// or before the first capture frame lands) fall back to identity instead of producing
// NaNs that would poison the constant buffer.
constexpr AspectFit ComputeAspectFit(float sourceWidth, float sourceHeight, float outputWidth,
                                     float outputHeight) noexcept {
    AspectFit fit;
    if (sourceWidth <= 0.0f || sourceHeight <= 0.0f || outputWidth <= 0.0f || outputHeight <= 0.0f) {
        return fit;
    }

    const float sourceAspect = sourceWidth / sourceHeight;
    const float outputAspect = outputWidth / outputHeight;

    if (sourceAspect > outputAspect) {
        // Source is relatively wider: fill the width, bar the top and bottom.
        fit.scaleY = outputAspect / sourceAspect;
    } else if (sourceAspect < outputAspect) {
        // Source is relatively taller: fill the height, bar the left and right.
        fit.scaleX = sourceAspect / outputAspect;
    }

    fit.offsetX = (1.0f - fit.scaleX) * 0.5f;
    fit.offsetY = (1.0f - fit.scaleY) * 0.5f;
    return fit;
}

// FILTERS-AND-EFFECTS.md section 1: intensity is normalized to [0, 1] internally even
// though the UI presents 0-100%.
constexpr float ClampIntensity(float value) noexcept {
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

// RF-011 / AT-008: distortion is bipolar, -1 anti-fisheye .. +1 fisheye, continuous
// through zero.
constexpr float ClampBipolar(float value) noexcept {
    return value < -1.0f ? -1.0f : (value > 1.0f ? 1.0f : value);
}

// Parses "16:9" / "4:3" / "1.777" into a width/height ratio. Returns 0 when the text does
// not describe a usable ratio, which callers treat as "no aspect constraint".
inline float ParseAspectRatio(const char* text) noexcept {
    if (text == nullptr) {
        return 0.0f;
    }

    float width = 0.0f;
    float fraction = 0.0f;
    int fractionDigits = 0;
    bool inFraction = false;
    bool sawDigit = false;

    for (const char* p = text; *p != '\0'; ++p) {
        const char c = *p;
        if (c >= '0' && c <= '9') {
            sawDigit = true;
            if (inFraction) {
                fraction = fraction * 10.0f + static_cast<float>(c - '0');
                ++fractionDigits;
            } else {
                width = width * 10.0f + static_cast<float>(c - '0');
            }
        } else if (c == '.' && !inFraction) {
            inFraction = true;
        } else if (c == ':' || c == '/') {
            // "W:H" form - the part after the separator is the height.
            float height = 0.0f;
            bool sawHeightDigit = false;
            for (const char* q = p + 1; *q != '\0'; ++q) {
                if (*q >= '0' && *q <= '9') {
                    height = height * 10.0f + static_cast<float>(*q - '0');
                    sawHeightDigit = true;
                } else if (*q != ' ') {
                    break;
                }
            }
            if (!sawDigit || !sawHeightDigit || height <= 0.0f || width <= 0.0f) {
                return 0.0f;
            }
            return width / height;
        } else if (c != ' ') {
            return 0.0f;
        }
    }

    if (!sawDigit) {
        return 0.0f;
    }

    // Decimal form - "1.7777".
    float scale = 1.0f;
    for (int i = 0; i < fractionDigits; ++i) {
        scale *= 10.0f;
    }
    const float value = width + fraction / scale;
    return value > 0.0f ? value : 0.0f;
}

// Given a proposed window size and a required ratio, returns the size that honours the
// ratio. `adjustHeight` decides which axis gives way: dragging a vertical edge should keep
// the width the user chose and recompute the height, and vice versa.
struct SizeI {
    int width = 0;
    int height = 0;
};

constexpr SizeI ApplyAspectRatio(SizeI proposed, float ratio, bool adjustHeight) noexcept {
    if (ratio <= 0.0f || proposed.width <= 0 || proposed.height <= 0) {
        return proposed;
    }

    SizeI result = proposed;
    if (adjustHeight) {
        result.height = static_cast<int>(static_cast<float>(proposed.width) / ratio + 0.5f);
    } else {
        result.width = static_cast<int>(static_cast<float>(proposed.height) * ratio + 0.5f);
    }
    result.width = result.width > 1 ? result.width : 1;
    result.height = result.height > 1 ? result.height : 1;
    return result;
}

}  // namespace overlaydesk
