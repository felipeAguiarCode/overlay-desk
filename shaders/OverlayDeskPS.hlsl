#include "Common.hlsli"

// Overlay Desk main pixel shader.
//
// ADR-0003 forbids intermediate render targets, so the whole chain runs here in one pass over
// the captured texture. ADR-0007, ADR-0008, ADR-0009 and ADR-0011 extend the original six
// stages; the relative order of those six is unchanged throughout:
//
//     Captured Texture -> Scope Zoom -> Jitter UV -> Shimmer UV -> Rolling Shutter UV
//                      -> Glitch UV -> Distortion -> Chromatic Aberration -> Lens Softness
//                      -> Bloom -> Color Correction -> False Colour -> Edge Glow -> Noise
//                      -> Scanlines -> Vignette -> Lens Dirt -> Scan Sweep -> Flicker
//                      -> Scope Mask -> Output
//
// ADR-0008 adds the two scope stages. They bracket the chain rather than sitting inside it,
// which is what an optic physically is: the objective magnifies before anything else sees the
// image, and the aperture is the last thing between the picture and the eye.
//
// The section numbers below are definition order, not pipeline position - scope is section 10
// but runs first and last. main() is the authority on what runs when.
//
// Output is premultiplied alpha, because the swap chain is created with
// DXGI_ALPHA_MODE_PREMULTIPLIED and composed by DirectComposition (ADR-0006).
//
// Every module is gated on a branch from g_enabledMask, so a disabled filter costs a branch
// the whole wave agrees on and nothing else - FILTERS-AND-EFFECTS.md section 5 and AT-010
// ("OFF: custo visual zero").
//
// House rule for every function in this file: assign to a single result variable and return
// once at the end. fxc's unoptimised path (/Od, used for the Debug build) reports early
// returns inside branches as X4000 "potentially uninitialized variable", and the build treats
// warnings as errors. The optimised path folds these away, so the two forms cost the same in
// Release.

static const float3 kEditBorderColor = float3(0.15, 0.75, 1.00);
static const float3 kIdleBackdropColor = float3(0.05, 0.05, 0.07);
static const float3 kLumaWeights = float3(0.2126, 0.7152, 0.0722);
static const float kTau = 6.28318530718;

// --- Hashes -------------------------------------------------------------------------------
//
// Everything procedural in this file draws from these. They are pure functions of their
// input, which is what makes every effect deterministic for a given g_time - the same instant
// always produces the same frame, with no state carried between them (ADR-0003).

float Hash11(float value)
{
    return frac(sin(value * 127.1) * 43758.5453123);
}

float Hash21(float2 value)
{
    return frac(sin(dot(value, float2(127.1, 311.7))) * 43758.5453123);
}

// --- Geometry helpers -----------------------------------------------------------------------

// Overlay UV to a centred, aspect-corrected coordinate, plus the radius of the corners in that
// space. Working here is what keeps radial effects circular on screen instead of stretched
// into ellipses on a wide overlay, and it is expressed entirely in normalised units, so
// resizing the overlay never changes the shape of an effect (AT-007).
float2 ToCentred(float2 uv, out float cornerRadius)
{
    const float aspect = g_outputResolution.x / max(g_outputResolution.y, 1.0);
    const float2 scale = float2(aspect, 1.0);
    cornerRadius = length(scale * 0.5);
    return (uv - 0.5) * scale;
}

float2 FromCentred(float2 centred)
{
    const float aspect = g_outputResolution.x / max(g_outputResolution.y, 1.0);
    return centred / float2(aspect, 1.0) + 0.5;
}

float2 Rotate(float2 value, float radians)
{
    const float s = sin(radians);
    const float c = cos(radians);
    return float2(value.x * c - value.y * s, value.x * s + value.y * c);
}

// --- 1. Jitter (image shake) ------------------------------------------------------------------
//
// The whole frame wanders, the way an unstable signal or an unsteady camera does. Distinct
// from the per-line jitter inside the glitch, which only exists during a burst.

float2 ApplyJitterUv(float2 uv)
{
    float2 result = uv;

    if (FeatureEnabled(FEATURE_JITTER))
    {
        const float speed = lerp(3.0, 40.0, saturate(g_jitterSpeed));
        const float t = g_time * speed;

        // Two sines per axis at unrelated rates: a single sine would trace a clean diagonal,
        // which reads as animation rather than instability.
        const float2 offset = float2(sin(t * 1.7) + sin(t * 0.9) * 0.5,
                                     cos(t * 1.3) + cos(t * 2.1) * 0.5);

        result = uv + offset * saturate(g_jitterIntensity) * 0.01;
    }

    return result;
}

// --- 2. Glitch (RF-016) -------------------------------------------------------------------
//
// Time is quantised into short slots. Each slot draws one pseudo-random number; the slot
// glitches when that number falls under g_glitchFrequency. That is what keeps frequency
// independent of intensity, as FILTERS-AND-EFFECTS.md 3.1 requires: frequency decides how
// often a burst happens, intensity only decides how violent it is when it does.

static const float kGlitchSlotsPerSecond = 14.0;

float GlitchSlot()
{
    return floor(g_time * kGlitchSlotsPerSecond);
}

// 0 when no burst is happening, otherwise how hard this one hits. Cheap enough (two hashes)
// to recompute wherever it is needed rather than threading a struct through the pipeline.
float GlitchBurst()
{
    float burst = 0.0;

    if (FeatureEnabled(FEATURE_GLITCH))
    {
        const float slot = GlitchSlot();
        if (Hash11(slot) < saturate(g_glitchFrequency))
        {
            // Bursts vary in severity so a run of them does not look like a loop.
            burst = saturate(g_glitchIntensity) * (0.35 + 0.65 * Hash11(slot + 17.0));
        }
    }

    return burst;
}

float2 ApplyGlitchUv(float2 uv)
{
    float2 result = uv;

    const float burst = GlitchBurst();
    if (burst > 0.0)
    {
        const float slot = GlitchSlot();

        if (g_glitchStyle == GLITCH_DIGITAL)
        {
            // A compressed downlink does not tear, it loses blocks. The grid is
            // two-dimensional, each macroblock either survives intact or is replaced wholesale
            // by a neighbour, and nothing shears in between - inside a block the image is
            // undisturbed, which is exactly what distinguishes this from the analog style.
            const float cell = max(g_glitchBlockSize, 0.02) * 0.5;
            const float2 block = floor(uv / cell);
            const float h = Hash21(block + slot * 7.0);

            if (h < burst)
            {
                const float2 offset = float2(frac(h * 61.0), frac(h * 173.0)) * 2.0 - 1.0;
                result = uv + offset * cell * burst * 2.0;
            }
        }
        else
        {
            // Horizontal bands, each displaced by its own amount. Roughly half the bands stay
            // put, which is what makes it read as torn signal rather than a uniform wobble.
            const float bandHeight = max(g_glitchBlockSize, 0.01);
            const float band = floor(uv.y / bandHeight);
            const float bandActive = step(0.5, Hash21(float2(band, slot + 3.0)));
            const float bandOffset = (Hash21(float2(band, slot)) * 2.0 - 1.0) * bandActive;

            // Fine per-line noise on top, so even the bands that did not jump are unsettled.
            //
            // The row height is derived from the band grid rather than being a fixed count:
            // blockSize then governs the texture of the whole effect, and the rows stay coarse
            // enough that neighbouring lines displacing in opposite directions reads as an
            // unstable signal instead of shredding the frame into an unreadable comb.
            const float jitterRows = max(1.0 / bandHeight, 1.0) * 8.0;
            const float jitterNoise = Hash21(float2(floor(uv.y * jitterRows), slot)) * 2.0 - 1.0;

            // Capped at 12% of the width: past that the frame reads as broken rather than
            // glitched, and content displaced off the edge is simply not there to sample.
            result.x +=
                bandOffset * burst * 0.12 + jitterNoise * saturate(g_glitchJitter) * burst * 0.02;
        }
    }

    return result;
}

// --- 3. Distortion (RF-011, AT-008) ---------------------------------------------------------
//
// g_distortionAmount runs -1 (anti-fisheye) .. 0 (neutral) .. +1 (fisheye) in every shape, so
// the negative half of each one is its own anti-fisheye counterpart.
//
// Each shape scales the centred coordinate by DistortionScale, differing only in the term they
// bend along. At k = 0 the scale is exactly 1 in both halves, so continuity through zero falls
// out of the algebra instead of needing a special case - which is what AT-008 asks for.

// The scale applied at `t`: the squared normalised term the shape bends along, 0 at the centre
// and 1 at the extreme it anchors on.
//
// The two halves are not one expression with a sign flipped, and carrying that difference is
// why this function exists (ADR-0010).
//
// The fisheye half divides by (1 + k). That keeps the scale at or below 1, so the map only ever
// reads inward and cannot leave the frame, and it pins the extremes to themselves.
//
// Feeding the same expression a negative k does not invert it. It still magnifies the centre,
// merely less; and the scale rises above 1 across the whole interior, so the map reads from
// beyond the captured frame and leaves a transparent border where there is nothing to read.
// Measured at amount -1 it reached 1.089 of the corner radius, and every sample past 1.0 was a
// hole.
//
// The anti-fisheye half is 1 / (1 - k*t) instead: 1 at the centre, 1/(1 + |k|) at the extreme,
// so the edges are magnified relative to the middle - which is the pinch - while the scale
// never exceeds 1 and the map is therefore incapable of leaving the frame. The price is that
// the extremes stop being anchored to themselves: a pinch has to crop, because the content it
// would otherwise need is off the edge of what was captured. Cropping a little is the honest
// trade against showing a hole.
float DistortionScale(float k, float t)
{
    const float scale = (k >= 0.0) ? ((1.0 + k * t) / (1.0 + k)) : (1.0 / (1.0 - k * t));
    return scale;
}

float2 ApplyDistortion(float2 uv)
{
    float2 result = uv;

    if (FeatureEnabled(FEATURE_DISTORTION))
    {
        const float k = g_distortionAmount * saturate(g_distortionIntensity) * 0.5;

        float cornerRadius;
        const float2 centred = ToCentred(uv, cornerRadius);

        // Normalised so its length is 1 at the corners, whatever the overlay's aspect.
        const float2 n = centred / max(cornerRadius, 1e-6);
        const float r2 = dot(n, n);

        float2 factor = DistortionScale(k, r2).xx;

        if (g_distortionShape == DISTORTION_CRT)
        {
            // Separable: each axis bends by the square of the OTHER one. That is how tube
            // geometry actually behaves - the top edge bows because it sits far from centre
            // vertically, not because of where it is horizontally - and it keeps straight
            // lines near the axes straight, which pure radial does not.
            factor.x = DistortionScale(k, n.y * n.y);
            factor.y = DistortionScale(k, n.x * n.x);
        }
        else if (g_distortionShape == DISTORTION_CYLINDRICAL)
        {
            factor.x = DistortionScale(k, n.x * n.x);
            factor.y = 1.0;
        }
        else if (g_distortionShape == DISTORTION_VERTICAL)
        {
            factor.x = 1.0;
            factor.y = DistortionScale(k, n.y * n.y);
        }
        else if (g_distortionShape == DISTORTION_CORNER)
        {
            // Quartic instead of quadratic: the middle of the frame stays essentially flat
            // and the pull concentrates in the corners.
            factor = DistortionScale(k, r2 * r2).xx;
        }

        result = FromCentred(centred * factor);
    }

    return result;
}

// --- 4. Chromatic aberration (RF-014) --------------------------------------------------------
//
// Runs at sample time rather than on an already-sampled colour, because separating the
// channels means sampling the source three times at three offsets.

float2 ChromaticAxis(float2 centred, float radius)
{
    // Radial, edge, prism and barrel all displace along the direction from the centre; they
    // differ in magnitude (ChromaticWeight) or angle (the prism rotation below).
    float2 axis = radius > 1e-5 ? centred / radius : float2(0.0, 0.0);

    if (g_chromaticMode == CHROMATIC_HORIZONTAL)
    {
        axis = float2(1.0, 0.0);
    }
    else if (g_chromaticMode == CHROMATIC_VERTICAL)
    {
        axis = float2(0.0, 1.0);
    }

    return axis;
}

float ChromaticWeight(float normalisedRadius)
{
    const float bias = saturate(g_chromaticEdgeBias);

    float weight = lerp(normalisedRadius, normalisedRadius * normalisedRadius, bias);

    if (g_chromaticMode == CHROMATIC_EDGE)
    {
        // Edge mode keeps the centre clean and concentrates everything at the rim.
        weight = pow(normalisedRadius, 1.0 + 3.0 * bias);
    }
    else if (g_chromaticMode == CHROMATIC_BARREL)
    {
        // How a lens actually misfocuses: separation grows with the square of the radius,
        // regardless of the edge bias.
        weight = normalisedRadius * normalisedRadius;
    }
    else if (g_chromaticMode == CHROMATIC_HORIZONTAL || g_chromaticMode == CHROMATIC_VERTICAL)
    {
        // An axial shift is uniform by default; edgeBias fades it toward the rim.
        weight = lerp(1.0, normalisedRadius, bias);
    }

    return weight;
}

// A plain read of the source at one point, under the same letterbox rule SampleSource applies:
// outside the frame it contributes nothing.
//
// Bloom and edge glow need taps of their own, and this is what keeps that rule in one place.
// A tap that returned the clamped edge pixel instead of zero would bleed a halo out into the
// letterbox bars, where there is no picture to bloom.
// Returns the colour in .rgb and 1 in .w when the tap landed on the picture, 0 in both when it
// did not. Callers that only add light (bloom, edge glow) can ignore .w and let a tap outside
// contribute nothing; callers that average (lens softness) have to weight by it, or the taps
// that fell off the edge would drag the average towards black.
float4 SampleSourceTap(float2 uv)
{
    float4 result = 0.0;

    const float2 contentUv = OverlayUvToSourceUv(uv);
    if (InsideUnitSquare(contentUv))
    {
        result.rgb = g_source.SampleLevel(g_sampler, ContentUvToTextureUv(contentUv), 0.0).rgb;
        result.w = 1.0;
    }

    return result;
}

// Samples the source, splitting the channels when chromatic aberration is on. `coverage` comes
// from the green channel: it is the unshifted one, so it defines where the frame actually is,
// while red and blue are free to fringe past that edge on their own.
float3 SampleSource(float2 uv, out float coverage)
{
    float2 redUv = uv;
    float2 blueUv = uv;

    if (FeatureEnabled(FEATURE_CHROMATIC))
    {
        float cornerRadius;
        const float2 centred = ToCentred(uv, cornerRadius);
        const float radius = length(centred);
        const float normalised = saturate(radius / max(cornerRadius, 1e-6));

        // 2% of the frame at full intensity: enough to read as a lens artefact, not enough to
        // look like a broken shader.
        const float magnitude =
            saturate(g_chromaticIntensity) * 0.02 * ChromaticWeight(normalised) * cornerRadius;
        const float2 axis = ChromaticAxis(centred, radius);

        float2 redAxis = axis;
        float2 blueAxis = axis;
        if (g_chromaticMode == CHROMATIC_PRISM)
        {
            // Glass splits light by wavelength into different angles, not just different
            // distances. Fanning the two channels apart is what separates this from radial.
            redAxis = Rotate(axis, 0.6);
            blueAxis = Rotate(axis, -0.6);
        }

        redUv = FromCentred(centred + redAxis * magnitude * g_chromaticRedShift);
        blueUv = FromCentred(centred + blueAxis * magnitude * g_chromaticBlueShift);
    }

    // RF-016: the glitch carries its own channel separation, stacking on whatever chromatic
    // aberration is already doing. Strictly horizontal - this one is a signal artefact, not a
    // lens one, so it does not follow the radial geometry above.
    const float glitchBurst = GlitchBurst();
    if (glitchBurst > 0.0)
    {
        const float shift = glitchBurst * saturate(g_glitchRgbShift) * 0.03;
        redUv.x += shift;
        blueUv.x -= shift;
    }

    // Overlay UV becomes content UV here and nowhere else. Every stage above works in the
    // overlay's own space, which is what keeps a radial effect circular on screen instead of
    // following the source's aspect; the aspect-fit mapping belongs at the sampling boundary,
    // where the two spaces actually meet. Components outside [0,1] after the mapping are the
    // letterbox bars, and that is where the coverage that drives the transparent bars comes
    // from - without this the source would simply be stretched to fill the overlay.
    const float2 contentUv = OverlayUvToSourceUv(uv);
    const float2 contentRedUv = OverlayUvToSourceUv(redUv);
    const float2 contentBlueUv = OverlayUvToSourceUv(blueUv);

    coverage = InsideUnitSquare(contentUv) ? 1.0 : 0.0;

    // SampleLevel, not Sample: this runs inside divergent control flow, so screen-space
    // derivatives are unavailable. The sampler clamps, and the per-channel coverage test is
    // what stops a shifted channel from smearing the edge pixel out across the letterbox.
    const float red =
        g_source.SampleLevel(g_sampler, ContentUvToTextureUv(saturate(contentRedUv)), 0.0).r;
    const float green =
        g_source.SampleLevel(g_sampler, ContentUvToTextureUv(saturate(contentUv)), 0.0).g;
    const float blue =
        g_source.SampleLevel(g_sampler, ContentUvToTextureUv(saturate(contentBlueUv)), 0.0).b;

    return float3(red * (InsideUnitSquare(contentRedUv) ? 1.0 : 0.0),
                  green * coverage,
                  blue * (InsideUnitSquare(contentBlueUv) ? 1.0 : 0.0));
}

// --- 5. Colour correction (RF-015) ------------------------------------------------------------
//
// Neutral values - brightness 0, contrast 1, saturation 1, gamma 1 - reproduce the input
// exactly, which is the Milestone 5 gate.

float3 ApplyColorCorrection(float3 color)
{
    float3 result = color;

    if (FeatureEnabled(FEATURE_COLOR_CORRECTION))
    {
        float3 corrected = max(color, 0.0);

        corrected = pow(corrected, 1.0 / max(g_gamma, 1e-3));
        corrected = (corrected - 0.5) * g_contrast + 0.5;
        corrected += g_brightness;

        const float luma = dot(max(corrected, 0.0), kLumaWeights);
        corrected = lerp(luma.xxx, corrected, g_saturation);

        // Tint last, so it colours the finished result rather than something the contrast and
        // saturation steps then fight over. Multiplying keeps blacks black, which is what a
        // phosphor or an LCD dye actually does - adding would wash the image into the colour.
        corrected = lerp(corrected, corrected * g_tint, saturate(g_tintAmount));

        // Tonal quantisation, the way a display with too few bits steps instead of ramping.
        // Zero is identity, so the neutral state still reproduces the source exactly.
        if (g_posterize > 0.0)
        {
            const float steps = lerp(64.0, 3.0, saturate(g_posterize));
            corrected = floor(saturate(corrected) * steps + 0.5) / steps;
        }

        result = lerp(color, saturate(corrected), saturate(g_colorIntensity));
    }

    return result;
}

// --- 6. Noise --------------------------------------------------------------------------------
//
// Sensor and film grain. Quantising both the cell and the time step is what makes it grain
// rather than a crawling pattern: the speckles hold still for a moment and then re-roll,
// which is what film actually does frame to frame.

float3 ApplyNoise(float3 color, float2 pixel)
{
    float3 result = color;

    if (FeatureEnabled(FEATURE_NOISE))
    {
        const float grain = lerp(1.0, 6.0, saturate(g_noiseGrainSize));
        const float2 cell = floor(pixel / grain);
        const float tick = floor(g_time * lerp(4.0, 40.0, saturate(g_noiseSpeed)));

        const float mono = Hash21(cell + tick * 17.0) - 0.5;
        const float3 perChannel = float3(Hash21(cell + tick * 17.0),
                                         Hash21(cell + tick * 29.0),
                                         Hash21(cell + tick * 41.0)) - 0.5;

        const float3 grainValue = lerp(mono.xxx, perChannel, saturate(g_noiseColorAmount));
        result = saturate(color + grainValue * saturate(g_noiseIntensity));
    }

    return result;
}

// --- 7. Scanlines (RF-013) --------------------------------------------------------------------
//
// scaleMode decides what the spacing is measured against:
//
//   pixelPerfect - output pixels, so the pattern stays crisp whatever the source is;
//   relative     - source pixels, so a 240p source keeps 240 scanlines however far the overlay
//                  is scaled up.
//
// style decides the shape of the darkening. Hard, Soft and Sharp dim every channel equally;
// Aperture Grille and Slot Mask dim the channels separately, which is what produces the colour
// fringing a real shadow mask has.

float ScanlineProfile(float phase, float width)
{
    // A half-period feather keeps the pattern from aliasing into moire when the spacing does
    // not divide the output resolution evenly.
    const float feather = max(width * 0.5, 1e-4);

    float mask = 1.0 - smoothstep(width - feather, width + feather, phase);

    if (g_scanlineStyle == SCANLINE_SOFT)
    {
        // A full cosine cycle: the darkening eases in and out instead of switching.
        mask = 0.5 + 0.5 * cos(kTau * phase);
    }
    else if (g_scanlineStyle == SCANLINE_SHARP)
    {
        // A narrow, steep well - thin dark lines with bright gaps between them.
        mask = pow(saturate(1.0 - phase / max(width, 1e-4)), 2.0);
    }

    return mask;
}

// Vertical RGB triads. Each column leaves one channel bright and dims the other two, which is
// how an aperture grille splits the beam.
float3 TriadMask(float coordinate, float period, float stagger)
{
    const float phase = frac(coordinate / period + stagger);
    const float cell = floor(phase * 3.0);

    // 0 for the channel this column belongs to, 1 for the others - no branching needed.
    return saturate(abs(cell.xxx - float3(0.0, 1.0, 2.0)));
}

float3 ApplyScanlines(float3 color, float2 uv, float2 pixel)
{
    float3 result = color;

    if (FeatureEnabled(FEATURE_SCANLINES))
    {
        float2 coordinate = (g_scanlineScaleMode == SCANLINE_PIXEL_PERFECT)
                                ? pixel
                                : uv * max(g_sourceResolution, 1.0);

        const float period = max(g_scanlineSpacing, 1.0);
        const float strength = saturate(g_scanlineIntensity);

        // 480i: the two fields are drawn half a line apart, alternating at the field rate. A
        // function of g_time alone - nothing is remembered between frames (ADR-0003), and at 0
        // the offset vanishes and the pattern is progressive again.
        const float field = fmod(floor(g_time * 60.0), 2.0);
        coordinate.y += field * period * 0.5 * saturate(g_scanlineInterlace);

        // The beam spreads as it is driven harder, so a bright line is a fat line. Without this
        // the pattern is a fixed set of stripes laid over the picture and reads as an overlay;
        // with it, highlights close up and shadows keep their black gaps, which is what the eye
        // actually recognises as a tube. At 0 the width is the uniform one.
        const float luma = dot(saturate(color), kLumaWeights);
        const float spread = lerp(1.0, 1.0 + luma * 1.6, saturate(g_scanlineBeamWidth));
        const float width = saturate(g_scanlineThickness * spread / period);

        float3 mask = 0.0;

        if (g_scanlineStyle == SCANLINE_APERTURE_GRILLE)
        {
            mask = TriadMask(coordinate.x, period, 0.0);
        }
        else if (g_scanlineStyle == SCANLINE_SLOT_MASK)
        {
            // Alternate row groups are offset by half a triad. That stagger is the whole
            // difference between a grille and a shadow mask.
            const float rowGroup = floor(coordinate.y / (period * 2.0));
            const float stagger = fmod(rowGroup, 2.0) * 0.5;

            const float3 triad = TriadMask(coordinate.x, period, stagger);
            const float rows = ScanlineProfile(frac(coordinate.y / (period * 2.0)), 0.5);
            mask = max(triad * 0.7, rows.xxx * 0.6);
        }
        else if (g_scanlineOrientation == SCANLINE_VERTICAL)
        {
            mask = ScanlineProfile(frac(coordinate.x / period), width).xxx;
        }
        else if (g_scanlineOrientation == SCANLINE_GRID)
        {
            const float horizontal = ScanlineProfile(frac(coordinate.y / period), width);
            const float vertical = ScanlineProfile(frac(coordinate.x / period), width);
            mask = max(horizontal, vertical).xxx;
        }
        else
        {
            mask = ScanlineProfile(frac(coordinate.y / period), width).xxx;
        }

        result = color * (1.0 - saturate(mask) * strength);
    }

    return result;
}

// --- 8. Vignette (RF-012) ---------------------------------------------------------------------
//
// Deliberately fed the undistorted screen UV: FILTERS-AND-EFFECTS.md 2.2 requires the vignette
// to stay anchored to the overlay's own edges rather than riding along with the distortion.

float3 ApplyVignette(float3 color, float2 screenUv)
{
    float3 result = color;

    if (FeatureEnabled(FEATURE_VIGNETTE))
    {
        const float2 centred = abs(screenUv - 0.5) * 2.0;

        // roundness morphs the falloff between the two natural metrics - Chebyshev distance,
        // which traces a rectangle following the overlay border, and Euclidean distance, which
        // traces an ellipse inscribed in it.
        const float rectangular = max(centred.x, centred.y);
        const float elliptical = length(centred);
        const float distance = lerp(rectangular, elliptical, saturate(g_vignetteRoundness));

        const float inner = saturate(g_vignetteSize);
        const float outer = inner + max(saturate(g_vignetteSoftness), 1e-3);

        const float falloff = smoothstep(inner, outer, distance);
        result = color * (1.0 - falloff * saturate(g_vignetteIntensity));
    }

    return result;
}

// --- 9. Flicker -------------------------------------------------------------------------------
//
// The whole image breathes, the way a tube or a projector lamp does. Applied last of the colour
// stages so it modulates the finished picture rather than one layer of it.

float3 ApplyFlicker(float3 color)
{
    float3 result = color;

    if (FeatureEnabled(FEATURE_FLICKER))
    {
        const float speed = lerp(2.0, 30.0, saturate(g_flickerSpeed));

        // Two incommensurate rates so the pulse never settles into a recognisable loop.
        const float wave = sin(g_time * speed) * 0.6 + sin(g_time * speed * 2.37) * 0.4;

        // Half the slider at most: a full-range brightness swing is a strobe, not a flicker.
        result = saturate(color * (1.0 + wave * saturate(g_flickerIntensity) * 0.5));
    }

    return result;
}

// --- 11. Shimmer (refractive warp) ------------------------------------------------------------
//
// Hot air rising, or the field of an optical camouflage bending the light behind it. A
// continuous ripple, which is what separates it from the jitter (rigid displacement of the
// whole frame) and from the glitch (whole bands jumping at once).
//
// Applied before the distortion, because the lens sees an already-rippling scene.

float2 ApplyShimmerUv(float2 uv)
{
    float2 result = uv;

    if (FeatureEnabled(FEATURE_SHIMMER))
    {
        const float t = g_time * lerp(0.4, 4.0, saturate(g_shimmerSpeed));

        // Low scale is the fine boil right above hot metal; high scale is a slow swell.
        const float freq = lerp(34.0, 4.0, saturate(g_shimmerScale));

        // Two travelling waves per axis at unrelated wavelengths. One would resolve into a
        // visible grid the moment the eye caught the period.
        const float2 wave = float2(sin(uv.y * freq + t * 1.7) +
                                       sin(uv.x * freq * 0.63 - t * 1.1) * 0.5,
                                   cos(uv.x * freq * 1.13 + t * 1.3) +
                                       cos(uv.y * freq * 0.47 - t * 0.8) * 0.5);

        result = uv + wave * saturate(g_shimmerIntensity) * 0.012;
    }

    return result;
}

// --- 12. Rolling shutter ------------------------------------------------------------------------
//
// A CMOS sensor exposes one row at a time. When the camera moves during that readout the rows
// no longer line up and the image shears. Two unrelated rates again, for the same reason the
// jitter uses them: a single sine reads as animation rather than as an unsteady hand.

float2 ApplyRollingShutterUv(float2 uv)
{
    float2 result = uv;

    if (FeatureEnabled(FEATURE_ROLLING_SHUTTER))
    {
        const float t = g_time * lerp(0.8, 9.0, saturate(g_rollingShutterSpeed));
        const float sway = sin(t * 1.9) * 0.65 + sin(t * 0.7) * 0.35;

        // Measured from the middle row, so the frame pivots about its centre. Displacing every
        // row by the same amount would be a jitter, not a shear.
        result.x += (uv.y - 0.5) * sway * saturate(g_rollingShutterIntensity) * 0.10;
    }

    return result;
}

// --- 13. Bloom ----------------------------------------------------------------------------------
//
// A saturated sensor or intensifier tube spills light into its neighbours. ADR-0009 section 2
// settles this as a spiral of source taps rather than a real blur: a separable gaussian needs a
// full-size render target and the ADR-0005 budget has no room for one. It is glow, not a
// gaussian - at maximum radius the spiral has visible structure, and that is the accepted trade
// rather than a defect.
//
// Runs before the colour correction, because the blooming happens with the light that reached
// the sensor, not with the graded picture. That is what makes a night-vision halo come out
// monochrome and only then take the phosphor.

static const uint kBloomTaps = 12u;

float3 ApplyBloom(float3 color, float2 uv)
{
    float3 result = color;

    if (FeatureEnabled(FEATURE_BLOOM))
    {
        const float radius = lerp(0.004, 0.05, saturate(g_bloomRadius));

        // The taps are in overlay UV, which is not square. Scaling x by the overlay aspect is
        // what keeps the halo round on screen instead of stretched along the wider axis.
        const float2 aspect = float2(g_outputResolution.y / max(g_outputResolution.x, 1.0), 1.0);
        const float threshold = saturate(g_bloomThreshold);

        float3 accumulated = 0.0;
        float weightSum = 0.0;

        // A golden-angle spiral: consecutive taps land as far apart as they can, so twelve of
        // them cover the disc far more evenly than twelve spaced around a single ring.
        [unroll]
        for (uint i = 0u; i < kBloomTaps; ++i)
        {
            const float index = (float)i + 0.5;
            const float angle = index * 2.39996323;
            const float distance = sqrt(index / (float)kBloomTaps);

            const float2 offset = float2(cos(angle), sin(angle)) * distance * radius * aspect;
            const float3 tap = SampleSourceTap(uv + offset).rgb;

            // Only the part above the threshold blooms, and it keeps the colour of the light
            // that produced it - a red muzzle flash does not halo white.
            const float excess = max(dot(tap, kLumaWeights) - threshold, 0.0);
            const float weight = 1.0 - distance * 0.75;

            accumulated += tap * excess * weight;
            weightSum += weight;
        }

        // The tint is what turns a bloom into a halation: light scattering inside the faceplate
        // of a tube comes back red, so a warm tint puts the warm edge on a white highlight that
        // a CRT actually has. White is a no-op and leaves the halo the colour of its source.
        const float3 glow = (accumulated / max(weightSum, 1e-6)) * g_bloomTint;
        result = saturate(color + glow * saturate(g_bloomIntensity) * 4.0);
    }

    return result;
}

// --- 18. Lens softness (ADR-0011) -----------------------------------------------------------------
//
// A lens does not resolve equally across its field. It is sharp in the middle and falls apart
// towards the corners, and the wider it is the earlier that starts - which is why an ultra-wide
// shot reads as glass and a geometric warp of a sharp digital image does not.
//
// Runs before the bloom, because the defocus happens in the lens and the blooming happens at
// the sensor behind it. The blur of a bright spot is what blooms, not the other way round.
//
// Six taps on one ring rather than a spiral: this is a small, low-frequency softening and not a
// bokeh, so the extra coverage a spiral buys would not survive being displayed.

static const uint kSoftnessTaps = 6u;

float3 ApplyLensSoftness(float3 color, float2 uv)
{
    float3 result = color;

    if (FeatureEnabled(FEATURE_LENS_SOFTNESS))
    {
        float cornerRadius;
        const float2 centred = ToCentred(uv, cornerRadius);
        const float radius = saturate(length(centred) / max(cornerRadius, 1e-6));

        // Sharp inside `center`, degrading quadratically outside it. Squaring rather than
        // ramping linearly is what keeps the middle of the frame genuinely untouched instead of
        // slightly soft everywhere.
        const float sharp = saturate(g_lensSoftnessCenter);
        const float falloff = saturate((radius - sharp) / max(1.0 - sharp, 1e-3));
        const float blur = falloff * falloff * saturate(g_lensSoftnessIntensity);

        if (blur > 0.0)
        {
            const float2 aspect =
                float2(g_outputResolution.y / max(g_outputResolution.x, 1.0), 1.0);
            const float spread = blur * 0.014;

            // The centre tap starts the average at full weight, so at the smallest spreads the
            // result converges on the untouched colour rather than on a six-tap ring.
            float3 accumulated = color;
            float weight = 1.0;

            [unroll]
            for (uint i = 0u; i < kSoftnessTaps; ++i)
            {
                const float angle = ((float)i + 0.5) * (kTau / (float)kSoftnessTaps);
                const float2 offset = float2(cos(angle), sin(angle)) * spread * aspect;

                const float4 tap = SampleSourceTap(uv + offset);
                accumulated += tap.rgb * tap.w;
                weight += tap.w;
            }

            result = accumulated / max(weight, 1e-6);
        }
    }

    return result;
}

// --- 14. False colour ---------------------------------------------------------------------------
//
// Luminance mapped onto a palette. ADR-0009 section 4: this is the operation a tint cannot
// express, because ironbow is not monotonic in hue and black hot inverts before mapping.
//
// The ramps are evaluated in ALU from a handful of control points rather than sampled from a
// lookup texture - no permanent GPU resource, nothing to load, nothing that can be missing.

float3 PaletteRamp(float t)
{
    float3 result = t.xxx;

    if (g_falseColourPalette == PALETTE_BLACK_HOT)
    {
        result = (1.0 - t).xxx;
    }
    else if (g_falseColourPalette == PALETTE_IRONBOW)
    {
        const float3 a = float3(0.00, 0.00, 0.05);
        const float3 b = float3(0.28, 0.04, 0.45);
        const float3 c = float3(0.76, 0.12, 0.22);
        const float3 d = float3(0.98, 0.62, 0.05);
        const float3 e = float3(1.00, 1.00, 0.92);

        const float s = saturate(t) * 4.0;
        float3 ramp = lerp(a, b, saturate(s));
        ramp = lerp(ramp, c, saturate(s - 1.0));
        ramp = lerp(ramp, d, saturate(s - 2.0));
        ramp = lerp(ramp, e, saturate(s - 3.0));
        result = ramp;
    }
    else if (g_falseColourPalette == PALETTE_PHOSPHOR)
    {
        // P43, the green of an image intensifier. Multiplying rather than adding is what keeps
        // the dark parts of the scene dark instead of washing the whole frame green.
        result = t * float3(0.18, 1.00, 0.32);
    }
    else if (g_falseColourPalette == PALETTE_WHITE_PHOSPHOR)
    {
        result = t * float3(0.86, 0.93, 1.00);
    }
    else if (g_falseColourPalette == PALETTE_CROSS_COM)
    {
        // A sensor HUD reads cyan long before it reads white: blue saturates first, green
        // follows, and red only arrives at the very top of the range.
        result = float3(saturate(t * 1.6 - 0.75), saturate(t * 1.25), saturate(t * 1.5 + 0.12));
    }

    return result;
}

float3 ApplyFalseColour(float3 color)
{
    float3 result = color;

    if (FeatureEnabled(FEATURE_FALSE_COLOUR))
    {
        float luma = dot(saturate(color), kLumaWeights);

        // Quantising before the ramp rather than after is what makes the steps read as a
        // display with too few bits, instead of as banding in the colour itself.
        if (g_falseColourLevels > 0.0)
        {
            const float steps = lerp(48.0, 4.0, saturate(g_falseColourLevels));
            luma = floor(luma * steps + 0.5) / steps;
        }

        result = lerp(color, saturate(PaletteRamp(luma)), saturate(g_falseColourIntensity));
    }

    return result;
}

// --- 15. Edge glow ------------------------------------------------------------------------------
//
// The bright outline of a sensor view. The derivative is taken from the SOURCE texture, never
// from the processed result: by this point the result carries scanlines, grain and a vignette,
// and a detector would happily find the edges of those patterns instead of the content's.
//
// Drawn after the false colour so the palette does not recolour it - the outline belongs to the
// instrument, not to the scene it is looking at.

float3 ApplyEdgeGlow(float3 color, float2 uv)
{
    float3 result = color;

    if (FeatureEnabled(FEATURE_EDGE_GLOW))
    {
        const float2 spacing = lerp(0.0015, 0.006, saturate(g_edgeGlowWidth)) *
                               float2(g_outputResolution.y / max(g_outputResolution.x, 1.0), 1.0);

        const float left = dot(SampleSourceTap(uv - float2(spacing.x, 0.0)).rgb, kLumaWeights);
        const float right = dot(SampleSourceTap(uv + float2(spacing.x, 0.0)).rgb, kLumaWeights);
        const float above = dot(SampleSourceTap(uv - float2(0.0, spacing.y)).rgb, kLumaWeights);
        const float below = dot(SampleSourceTap(uv + float2(0.0, spacing.y)).rgb, kLumaWeights);

        // A central-difference gradient, four taps rather than a full Sobel's eight: the
        // diagonals buy a slightly rounder outline for a third more sampling, and at the
        // thickness this effect is used at the difference does not survive the display.
        const float gradient = length(float2(right - left, below - above));
        const float outline = saturate(gradient * 4.0);

        result = saturate(color + g_edgeGlowTint * outline * saturate(g_edgeGlowIntensity));
    }

    return result;
}

// --- 16. Lens dirt --------------------------------------------------------------------------------
//
// Grease and spatter on the front element. Procedural like the grain (ADR-0007): no texture, no
// permanent GPU resource, no tile that repeats.
//
// The one procedural stage here that does not read g_time, and deliberately so - dirt does not
// move. Animating it would read as rain on a windscreen, which is a different effect.

float3 ApplyLensDirt(float3 color, float2 screenUv)
{
    float3 result = color;

    if (FeatureEnabled(FEATURE_LENS_DIRT))
    {
        const float aspect = g_outputResolution.x / max(g_outputResolution.y, 1.0);
        const float2 point2d = float2(screenUv.x * aspect, screenUv.y) *
                               lerp(3.0, 9.0, saturate(g_lensDirtDensity));

        const float2 cell = floor(point2d);
        const float smear = saturate(g_lensDirtSmear);
        float smudge = 0.0;

        // Three by three, because a blob whose centre sits near a cell edge still has to be
        // found by the neighbours it spills into. One hash per neighbour, with the rest of the
        // numbers derived from it by frac: nine sines instead of thirty-six.
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            [unroll]
            for (int x = -1; x <= 1; ++x)
            {
                const float2 neighbour = cell + float2(x, y);
                const float h = Hash21(neighbour);

                const float present = step(0.45, frac(h * 13.0));
                const float2 centre = neighbour + float2(frac(h * 37.0), frac(h * 91.0));
                const float radius = lerp(0.10, 0.34, frac(h * 57.0)) * lerp(1.0, 2.2, smear);

                // Streaked rather than round: grease wipes in one direction.
                float2 delta = point2d - centre;
                delta.y *= lerp(1.0, 3.0, smear);

                smudge += present * (1.0 - smoothstep(0.0, radius, length(delta)));
            }
        }

        // Dirt does two things at once: it scatters light into a local haze and it blocks some
        // of what is behind it. Doing only one of the two reads as a decal stuck on the glass.
        const float amount = saturate(g_lensDirtIntensity) * saturate(smudge);
        const float haze = dot(color, kLumaWeights) * 0.6 + 0.15;

        result = saturate(lerp(color, lerp(color * 0.85, haze.xxx, 0.5), amount));
    }

    return result;
}

// --- 17. Scan sweep -------------------------------------------------------------------------------
//
// The light bar of a sensor that scans. Applied just before the flicker because it is light
// from the display itself, so it breathes with the lamp.

float3 ApplyScanSweep(float3 color, float2 screenUv)
{
    float3 result = color;

    if (FeatureEnabled(FEATURE_SCAN_SWEEP))
    {
        const float period = lerp(6.0, 0.8, saturate(g_scanSweepSpeed));
        const float head = frac(g_time / max(period, 1e-3));

        // Distance behind the head, wrapped, so the bar leaves the bottom and re-enters at the
        // top with no seam.
        const float trail = frac(head - screenUv.y);
        const float width = max(saturate(g_scanSweepWidth), 0.01);

        // A hard leading edge with a squared decay behind it: a scanning beam brightens the
        // instant it arrives and then fades. A symmetric bar reads as a moving stripe instead.
        const float bar = saturate(1.0 - trail / width);

        result = saturate(color + bar * bar * saturate(g_scanSweepIntensity) * 0.55);
    }

    return result;
}

// --- Edit-mode chrome ---------------------------------------------------------------------

float DistanceToNearestEdge(float2 pixel)
{
    const float2 fromEdges = min(pixel, g_outputResolution - pixel);
    return min(fromEdges.x, fromEdges.y);
}

// Square grab handles at the four corners and the four edge midpoints, matching the resize
// regions OverlayWindow reports from WM_NCHITTEST.
//
// Branchless by construction: the anchors form a 3x3 grid at {0, centre, extent} on each axis,
// so "near any anchor" is just the per-axis distance to the nearest of those three. The centre
// cell is the one anchor that is not a handle, so it is subtracted back out.
bool InsideResizeHandle(float2 pixel, float thickness)
{
    const float size = max(thickness * 3.0, 6.0);
    const float2 center = g_outputResolution * 0.5;

    const float2 toCenter = abs(pixel - center);
    const float2 toNearestAnchor = min(min(pixel, g_outputResolution - pixel), toCenter);

    const bool nearAnchor = all(toNearestAnchor <= size);
    const bool nearCenter = all(toCenter <= size);

    return nearAnchor && !nearCenter;
}

// --- Entry point ------------------------------------------------------------------------------

// --- 10. Scope (optic aperture) ------------------------------------------------------------
//
// Deliberately not the vignette with the sliders pushed. A vignette darkens the picture
// towards transparency; the body of a scope is solid, so this writes opaque black and forces
// alpha to 1 outside the glass. Without that the desktop would show through the part of the
// overlay that is supposed to be a steel tube.
//
// Magnification lives in ApplyScopeZoom, up with the other geometry, and runs before the
// jitter for a reason: displacing after the zoom means the shake is magnified along with the
// image, which is exactly the thing that makes a high-power optic hard to hold steady.

float2 ApplyScopeZoom(float2 uv)
{
    float2 result = uv;

    if (FeatureEnabled(FEATURE_SCOPE))
    {
        // Above 1 the frame edges leave the view, which is correct - a scope shows less of the
        // world, larger - and the aperture covers the boundary in any case.
        result = (uv - 0.5) / max(g_scopeMagnification, 0.05) + 0.5;
    }

    return result;
}

// 1 inside the glass, 0 in the body, feathered across the edge.
float ScopeAperture(float2 centred, float cornerRadius)
{
    float coverage;

    const float radius = max(saturate(g_scopeSize), 1e-3) * cornerRadius;
    const float feather = max(saturate(g_scopeSoftness), 1e-4) * cornerRadius;

    if (g_scopeShape == SCOPE_BINOCULAR)
    {
        // Two circles that overlap in the middle, which is what binoculars actually produce -
        // the two separate discs are a film convention, not an optical result.
        const float separation = radius * 0.5;
        const float left = length(centred - float2(-separation, 0.0));
        const float right = length(centred - float2(separation, 0.0));
        coverage = 1.0 - smoothstep(radius - feather, radius, min(left, right));
    }
    else if (g_scopeShape == SCOPE_QUAD_TUBE)
    {
        // Four tubes: the inner pair sit where a dual-tube goggle would, and the outer pair are
        // canted outwards to widen the field. The overlap is the point - a panoramic NVG shows
        // one continuous scene through four circles, not four separate portholes, so the tubes
        // are placed closer together than their own radius.
        const float tube = radius * 0.62;
        const float inner = tube * 0.78;
        const float outer = tube * 2.10;

        const float a = length(centred - float2(-outer, 0.0));
        const float b = length(centred - float2(-inner, 0.0));
        const float c = length(centred - float2(inner, 0.0));
        const float d = length(centred - float2(outer, 0.0));

        const float nearest = min(min(a, b), min(c, d));
        coverage = 1.0 - smoothstep(tube - feather, tube, nearest);
    }
    else if (g_scopeShape == SCOPE_TUBE)
    {
        // A CRT faceplate: a rectangle with corners rounded off, not a circle. The signed
        // distance to a rounded box - push the point out by the corner radius, clamp at zero,
        // and measure - which is the standard way to get one and costs a max and a length.
        //
        // The glass fills most of the overlay rather than sitting in the middle of it, so the
        // half-extents come from the frame itself and not from the corner radius the circular
        // shapes use. Deriving them from the corner radius makes the box larger than the frame
        // at any usable size, and then nothing is cut at all - a tube that leaves a wide black
        // margin would be a porthole, not a screen.
        const float aspect = g_outputResolution.x / max(g_outputResolution.y, 1.0);
        const float2 half = float2(aspect, 1.0) * 0.5 * max(saturate(g_scopeSize), 1e-3);
        const float corner = min(half.x, half.y) * 0.35;

        const float2 outside = abs(centred) - (half - corner);
        const float distance = length(max(outside, 0.0)) + min(max(outside.x, outside.y), 0.0);

        coverage = 1.0 - smoothstep(-feather, feather, distance - corner);
    }
    else
    {
        coverage = 1.0 - smoothstep(radius - feather, radius, length(centred));
    }

    return coverage;
}

// A duplex crosshair: thin at the centre so it does not cover the aiming point, thickening
// towards the edge so it stays readable on a small overlay. That is the shape most optics
// actually use, and it survives scaling better than a uniform hairline.
float ScopeReticle(float2 centred, float cornerRadius)
{
    float ink = 0.0;

    const float amount = saturate(g_scopeReticle);
    if (amount > 0.0)
    {
        const float radius = max(saturate(g_scopeSize), 1e-3) * cornerRadius;
        const float r = length(centred);
        const float2 d = abs(centred);

        const float thin = 0.0016 * cornerRadius;
        const float width = lerp(thin, thin * 3.5, smoothstep(0.5, 0.95, r / radius));

        // A gap at the centre, and both hairs stop at the edge of the glass.
        const float gap = 0.03 * cornerRadius;
        const float inside = step(gap, r) * step(r, radius);

        const float vertical = 1.0 - smoothstep(width, width * 2.0, d.x);
        const float horizontal = 1.0 - smoothstep(width, width * 2.0, d.y);

        // Holdover ticks below the centre, at even intervals down the vertical hair.
        const float spacing = 0.11 * cornerRadius;
        const float tickPhase = abs(frac(centred.y / spacing + 0.5) - 0.5) * spacing;
        const float tick = (1.0 - smoothstep(thin, thin * 2.0, tickPhase)) *
                           (1.0 - smoothstep(0.020 * cornerRadius, 0.026 * cornerRadius, d.x)) *
                           step(gap, centred.y);

        ink = saturate(max(vertical + horizontal, tick)) * inside * amount;
    }

    return ink;
}

// Returns rgb in .xyz and alpha in .w: the mask is the one stage that has to touch both.
float4 ApplyScope(float3 color, float alpha, float2 screenUv)
{
    float4 result = float4(color, alpha);

    if (FeatureEnabled(FEATURE_SCOPE))
    {
        float cornerRadius;
        const float2 centred = ToCentred(screenUv, cornerRadius);

        const float glass = ScopeAperture(centred, cornerRadius);
        const float strength = saturate(g_scopeIntensity);

        // At intensity 1 the body is fully opaque; below that the mask is only partly applied,
        // so the slider fades the whole optic in rather than making the tube translucent.
        const float body = (1.0 - glass) * strength;

        const float3 masked = lerp(color, 0.0.xxx, body);
        const float ink = ScopeReticle(centred, cornerRadius) * strength;

        result.xyz = lerp(masked, 0.0.xxx, ink);
        result.w = max(alpha, body);
    }

    return result;
}

float4 main(PixelInput input) : SV_Target
{
    const float2 pixel = input.position.xy;

    float2 uv = input.uv;
    uv = ApplyScopeZoom(uv);
    uv = ApplyJitterUv(uv);
    uv = ApplyShimmerUv(uv);
    uv = ApplyRollingShutterUv(uv);
    uv = ApplyGlitchUv(uv);
    uv = ApplyDistortion(uv);

    float3 color = 0.0;
    float alpha = 0.0;

    if (FeatureEnabled(FEATURE_HAS_SOURCE))
    {
        float coverage;
        color = SampleSource(uv, coverage);
        alpha = coverage;

        // Both need taps of the source, so they can only run where the source exists. The
        // softness is the lens and the bloom is the sensor behind it, in that order.
        color = ApplyLensSoftness(color, uv);
        color = ApplyBloom(color, uv);

        // Outside the frame we are in a letterbox bar. Leaving it fully transparent lets
        // whatever is underneath show through, which reads better over an emulator than black
        // pillarboxing.
    }
    else
    {
        // No capture yet. A dim panel makes the overlay findable while the user positions it;
        // it disappears the moment real frames arrive.
        color = kIdleBackdropColor;
        alpha = 0.55;
    }

    color = ApplyColorCorrection(color);
    color = ApplyFalseColour(color);
    color = ApplyEdgeGlow(color, uv);
    color = ApplyNoise(color, pixel);
    // Scanlines in "relative to source" mode measure spacing in source pixels, so they need
    // the content coordinate rather than the overlay one - otherwise a 240p source would be
    // given as many lines as the letterbox bars are wide.
    color = ApplyScanlines(color, OverlayUvToSourceUv(uv), pixel);
    color = ApplyVignette(color, input.uv);
    color = ApplyLensDirt(color, input.uv);
    color = ApplyScanSweep(color, input.uv);
    color = ApplyFlicker(color);

    // Last, and after the flicker: the body of the optic is opaque metal, so it neither
    // breathes with the lamp nor lets the edit border show through the part it covers.
    const float4 scoped = ApplyScope(color, alpha, input.uv);
    color = scoped.xyz;
    alpha = scoped.w;

    if (FeatureEnabled(FEATURE_EDIT_BORDER) && g_editBorderThickness > 0.0)
    {
        const bool onBorder = DistanceToNearestEdge(pixel) <= g_editBorderThickness;
        const bool onHandle = InsideResizeHandle(pixel, g_editBorderThickness);

        if (onBorder || onHandle)
        {
            // A slow pulse distinguishes edit mode from a static frame at a glance.
            const float pulse = 0.5 + 0.5 * sin(g_time * 3.0);
            const float3 chrome = lerp(kEditBorderColor, 1.0.xxx, onHandle ? 0.45 : pulse * 0.35);
            color = lerp(color, chrome, onHandle ? 1.0 : 0.9);
            alpha = max(alpha, onHandle ? 1.0 : 0.92);
        }
    }

    alpha *= saturate(g_opacity);
    return float4(color * alpha, alpha);
}
