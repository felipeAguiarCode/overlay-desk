// Common.hlsli - shared declarations for the Overlay Desk pixel pipeline.
//
// ADR-0003: every filter and effect is fused into one pixel shader. There are no
// intermediate render targets, so nothing here may sample anything other than the
// captured source texture.
//
// The constant buffer layout below is mirrored byte-for-byte by
// overlaydesk::ShaderConstants in src/graphics/RenderResources.h. Change one and you must
// change the other.

#ifndef OVERLAYDESK_COMMON_HLSLI
#define OVERLAYDESK_COMMON_HLSLI

// --- Feature bits (ShaderConstants::enabledMask) ---------------------------------------
#define FEATURE_DISTORTION        (1u << 0)
#define FEATURE_VIGNETTE          (1u << 1)
#define FEATURE_SCANLINES         (1u << 2)
#define FEATURE_CHROMATIC         (1u << 3)
#define FEATURE_COLOR_CORRECTION  (1u << 4)
#define FEATURE_GLITCH            (1u << 5)
#define FEATURE_EDIT_BORDER       (1u << 6)
#define FEATURE_HAS_SOURCE        (1u << 7)
#define FEATURE_NOISE             (1u << 8)
#define FEATURE_FLICKER           (1u << 9)
#define FEATURE_JITTER            (1u << 10)
#define FEATURE_SCOPE             (1u << 11)
#define FEATURE_BLOOM             (1u << 12)
#define FEATURE_FALSE_COLOUR      (1u << 13)
#define FEATURE_EDGE_GLOW         (1u << 14)
#define FEATURE_LENS_DIRT         (1u << 15)
#define FEATURE_SHIMMER           (1u << 16)
#define FEATURE_ROLLING_SHUTTER   (1u << 17)
#define FEATURE_SCAN_SWEEP        (1u << 18)
#define FEATURE_LENS_SOFTNESS     (1u << 19)
#define FEATURE_SHARPEN           (1u << 20)

#define SCOPE_CIRCLE      0u
#define SCOPE_BINOCULAR   1u
#define SCOPE_QUAD_TUBE   2u
#define SCOPE_TUBE        3u

// --- False colour palette (matches overlaydesk::FalseColourPalette) ---------------------
#define PALETTE_WHITE_HOT      0
#define PALETTE_BLACK_HOT      1
#define PALETTE_IRONBOW        2
#define PALETTE_PHOSPHOR       3
#define PALETTE_WHITE_PHOSPHOR 4
#define PALETTE_CROSS_COM      5

// --- Glitch style (matches overlaydesk::GlitchStyle) ------------------------------------
#define GLITCH_ANALOG  0
#define GLITCH_DIGITAL 1

// --- Distortion shape (matches overlaydesk::DistortionShape) ----------------------------
#define DISTORTION_RADIAL      0
#define DISTORTION_CRT         1
#define DISTORTION_CYLINDRICAL 2
#define DISTORTION_VERTICAL    3
#define DISTORTION_CORNER      4

// --- Scanline style (matches overlaydesk::ScanlineStyle) --------------------------------
#define SCANLINE_HARD            0
#define SCANLINE_SOFT            1
#define SCANLINE_SHARP           2
#define SCANLINE_APERTURE_GRILLE 3
#define SCANLINE_SLOT_MASK       4

// --- Scanline orientation (matches overlaydesk::ScanlineOrientation) --------------------
#define SCANLINE_HORIZONTAL 0
#define SCANLINE_VERTICAL   1
#define SCANLINE_GRID       2

// --- Scanline scale mode (matches overlaydesk::ScanlineScaleMode) -----------------------
#define SCANLINE_RELATIVE      0
#define SCANLINE_PIXEL_PERFECT 1

// --- Chromatic aberration mode (matches overlaydesk::ChromaticAberrationMode) -----------
#define CHROMATIC_RADIAL     0
#define CHROMATIC_HORIZONTAL 1
#define CHROMATIC_VERTICAL   2
#define CHROMATIC_EDGE       3
#define CHROMATIC_PRISM      4
#define CHROMATIC_BARREL     5

cbuffer ShaderConstants : register(b0)
{
    float2 g_outputResolution;   // overlay client area, pixels
    float2 g_sourceResolution;   // captured texture, pixels

    float4 g_fitScaleOffset;     // xy = scale, zw = offset (letterbox mapping)

    float  g_time;               // seconds since startup, for procedural effects
    float  g_opacity;            // RF-008, global window opacity
    uint   g_enabledMask;
    float  g_editBorderThickness; // pixels; 0 disables the border

    float  g_distortionAmount;    // RF-011, bipolar -1..+1
    float  g_distortionIntensity;
    float  g_vignetteIntensity;
    float  g_vignetteSize;

    float  g_vignetteSoftness;
    float  g_vignetteRoundness;
    float  g_scanlineIntensity;
    float  g_scanlineThickness;

    float  g_scanlineSpacing;
    int    g_scanlineOrientation;
    int    g_scanlineScaleMode;
    float  g_chromaticIntensity;

    int    g_chromaticMode;
    float  g_chromaticEdgeBias;
    float  g_chromaticRedShift;
    float  g_chromaticBlueShift;

    float  g_glitchIntensity;
    float  g_glitchFrequency;
    float  g_glitchBlockSize;
    float  g_glitchJitter;

    float  g_glitchRgbShift;
    float  g_brightness;
    float  g_contrast;
    float  g_saturation;

    float  g_gamma;
    float  g_colorIntensity;
    // Windows.Graphics.Capture allocates frame-pool textures at the size the pool was
    // created with, and the live content occupies only the top-left corner of that texture
    // until the pool is recreated. This scales content UV into texture UV so a source
    // resize never stretches the frame for the frame or two before Recreate lands.
    float2 g_sourceUvScale;

    int    g_distortionShape;
    int    g_scanlineStyle;
    float  g_noiseIntensity;
    float  g_noiseGrainSize;

    float  g_noiseSpeed;
    float  g_noiseColorAmount;
    float  g_flickerIntensity;
    float  g_flickerSpeed;

    float  g_jitterIntensity;
    float  g_jitterSpeed;
    float  g_tintAmount;
    float  g_padding0;

    float3 g_tint;
    float  g_scopeIntensity;

    float  g_scopeSize;
    float  g_scopeSoftness;
    float  g_scopeMagnification;
    float  g_scopeReticle;

    uint   g_scopeShape;
    float  g_bloomIntensity;
    float  g_bloomThreshold;
    float  g_bloomRadius;

    float  g_falseColourIntensity;
    int    g_falseColourPalette;
    float  g_falseColourLevels;
    float  g_edgeGlowIntensity;

    float  g_edgeGlowWidth;
    float  g_lensDirtIntensity;
    float  g_lensDirtDensity;
    float  g_lensDirtSmear;

    float3 g_edgeGlowTint;
    float  g_shimmerIntensity;

    float  g_shimmerSpeed;
    float  g_shimmerScale;
    float  g_rollingShutterIntensity;
    float  g_rollingShutterSpeed;

    float  g_scanSweepIntensity;
    float  g_scanSweepSpeed;
    float  g_scanSweepWidth;
    int    g_glitchStyle;

    float  g_posterize;
    float  g_lensSoftnessIntensity;
    float  g_lensSoftnessCenter;
    float  g_scanlineBeamWidth;

    float3 g_bloomTint;
    float  g_scanlineInterlace;

    float  g_sharpenIntensity;
    float  g_sharpenRadius;
    float  g_sharpenPad0;
    float  g_sharpenPad1;
};

Texture2D<float4> g_source : register(t0);
SamplerState      g_sampler : register(s0);

struct PixelInput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

bool FeatureEnabled(uint bit)
{
    return (g_enabledMask & bit) != 0u;
}

// Maps overlay UV to source UV. OVERLAY-WINDOW.md: source and overlay dimensions are
// independent, so this letterbox mapping is the only place the two meet.
float2 OverlayUvToSourceUv(float2 uv)
{
    return (uv - g_fitScaleOffset.zw) / max(g_fitScaleOffset.xy, 1e-6);
}

bool InsideUnitSquare(float2 uv)
{
    return all(uv >= 0.0) && all(uv <= 1.0);
}

// Content-space UV (0..1 over the visible frame) to the texture UV actually sampled.
float2 ContentUvToTextureUv(float2 uv)
{
    return uv * g_sourceUvScale;
}

#endif  // OVERLAYDESK_COMMON_HLSLI
