#pragma once

// PERFORMANCE.md sections 4 and 5: everything the render path needs is created once here
// and reused for the life of the device. Nothing in this file may be called per frame
// except UpdateConstants, which maps an already-allocated dynamic buffer.

#include <d3d11_4.h>

#include <cstdint>

#include "core/AppState.h"
#include "util/ComHelpers.h"

namespace overlaydesk {

// Feature bits, mirroring the FEATURE_* defines in shaders/Common.hlsli.
enum ShaderFeature : uint32_t {
    ShaderFeatureDistortion = 1u << 0,
    ShaderFeatureVignette = 1u << 1,
    ShaderFeatureScanlines = 1u << 2,
    ShaderFeatureChromatic = 1u << 3,
    ShaderFeatureColorCorrection = 1u << 4,
    ShaderFeatureGlitch = 1u << 5,
    ShaderFeatureEditBorder = 1u << 6,
    ShaderFeatureHasSource = 1u << 7,
    ShaderFeatureNoise = 1u << 8,
    ShaderFeatureFlicker = 1u << 9,
    ShaderFeatureJitter = 1u << 10,
    ShaderFeatureScope = 1u << 11,
    ShaderFeatureBloom = 1u << 12,
    ShaderFeatureFalseColour = 1u << 13,
    ShaderFeatureEdgeGlow = 1u << 14,
    ShaderFeatureLensDirt = 1u << 15,
    ShaderFeatureShimmer = 1u << 16,
    ShaderFeatureRollingShutter = 1u << 17,
    ShaderFeatureScanSweep = 1u << 18,
    ShaderFeatureLensSoftness = 1u << 19,
    ShaderFeatureSharpen = 1u << 20,
};

// Byte-for-byte mirror of the ShaderConstants cbuffer in shaders/Common.hlsli. HLSL packs
// constants into 16-byte registers and never lets one straddle a register boundary, so the
// grouping below (four scalars per line) is load-bearing, not cosmetic.
struct alignas(16) ShaderConstants {
    float outputResolution[2]{};
    float sourceResolution[2]{};

    float fitScaleOffset[4]{1.0f, 1.0f, 0.0f, 0.0f};

    float time = 0.0f;
    float opacity = 1.0f;
    uint32_t enabledMask = 0;
    float editBorderThickness = 0.0f;

    float distortionAmount = 0.0f;
    float distortionIntensity = 1.0f;
    float vignetteIntensity = 0.0f;
    float vignetteSize = 0.7f;

    float vignetteSoftness = 0.75f;
    float vignetteRoundness = 0.3f;
    float scanlineIntensity = 0.0f;
    float scanlineThickness = 1.0f;

    float scanlineSpacing = 2.0f;
    int32_t scanlineOrientation = 0;
    int32_t scanlineScaleMode = 1;
    float chromaticIntensity = 0.0f;

    int32_t chromaticMode = 0;
    float chromaticEdgeBias = 0.7f;
    float chromaticRedShift = 1.0f;
    float chromaticBlueShift = -1.0f;

    float glitchIntensity = 0.0f;
    float glitchFrequency = 0.05f;
    float glitchBlockSize = 0.1f;
    float glitchJitter = 0.05f;

    float glitchRgbShift = 0.1f;
    float brightness = 0.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;

    float gamma = 1.0f;
    float colorIntensity = 1.0f;
    // Visible content size divided by the frame-pool texture size. See g_sourceUvScale in
    // shaders/Common.hlsli.
    float sourceUvScale[2]{1.0f, 1.0f};

    int32_t distortionShape = 0;
    int32_t scanlineStyle = 0;
    float noiseIntensity = 0.0f;
    float noiseGrainSize = 0.5f;

    float noiseSpeed = 0.6f;
    float noiseColorAmount = 0.0f;
    float flickerIntensity = 0.0f;
    float flickerSpeed = 0.5f;

    float jitterIntensity = 0.0f;
    float jitterSpeed = 0.5f;
    float tintAmount = 0.0f;
    float padding0 = 0.0f;

    float tint[3]{1.0f, 1.0f, 1.0f};
    float scopeIntensity = 1.0f;

    float scopeSize = 0.62f;
    float scopeSoftness = 0.05f;
    float scopeMagnification = 1.0f;
    float scopeReticle = 0.0f;

    uint32_t scopeShape = 0;
    float bloomIntensity = 0.0f;
    float bloomThreshold = 0.75f;
    float bloomRadius = 0.35f;

    float falseColourIntensity = 0.0f;
    int32_t falseColourPalette = 0;
    float falseColourLevels = 0.0f;
    float edgeGlowIntensity = 0.0f;

    float edgeGlowWidth = 0.5f;
    float lensDirtIntensity = 0.0f;
    float lensDirtDensity = 0.5f;
    float lensDirtSmear = 0.4f;

    float edgeGlowTint[3]{0.35f, 0.80f, 1.00f};
    float shimmerIntensity = 0.0f;

    float shimmerSpeed = 0.5f;
    float shimmerScale = 0.5f;
    float rollingShutterIntensity = 0.0f;
    float rollingShutterSpeed = 0.5f;

    float scanSweepIntensity = 0.0f;
    float scanSweepSpeed = 0.4f;
    float scanSweepWidth = 0.15f;
    int32_t glitchStyle = 0;

    float posterize = 0.0f;
    float lensSoftnessIntensity = 0.0f;
    float lensSoftnessCenter = 0.45f;
    float scanlineBeamWidth = 0.0f;

    float bloomTint[3]{1.0f, 1.0f, 1.0f};
    float scanlineInterlace = 0.0f;

    float sharpenIntensity = 0.0f;
    float sharpenRadius = 0.35f;
    float sharpenPad0 = 0.0f;
    float sharpenPad1 = 0.0f;
};

static_assert(sizeof(ShaderConstants) == 384,
              "ShaderConstants must match the cbuffer layout in shaders/Common.hlsli");
static_assert(sizeof(ShaderConstants) % 16 == 0,
              "D3D11 constant buffers must be a multiple of 16 bytes");

struct SourceGeometry {
    uint32_t contentWidth = 0;   // visible frame size reported by the capture frame
    uint32_t contentHeight = 0;
    uint32_t textureWidth = 0;   // allocation size of the frame-pool texture
    uint32_t textureHeight = 0;
};

// Fills the constant buffer from persisted settings plus per-frame runtime values.
// Disabled modules clear their feature bit and leave their parameters untouched, which is
// what FILTERS-AND-EFFECTS.md section 5 requires: turning a filter off must not lose the
// values the user dialled in.
ShaderConstants BuildShaderConstants(const AppSettings& settings, uint32_t outputWidth,
                                     uint32_t outputHeight, const SourceGeometry& source,
                                     float timeSeconds, bool hasSource, bool editMode,
                                     float editBorderThickness);

class RenderResources {
public:
    RenderResources() = default;
    RenderResources(const RenderResources&) = delete;
    RenderResources& operator=(const RenderResources&) = delete;

    void Create(ID3D11Device* device);
    void Reset() noexcept;

    bool IsValid() const noexcept { return static_cast<bool>(m_constantBuffer); }

    ID3D11Buffer* ConstantBuffer() const noexcept { return m_constantBuffer.get(); }
    ID3D11SamplerState* Sampler() const noexcept { return m_sampler.get(); }
    ID3D11RasterizerState* RasterizerState() const noexcept { return m_rasterizerState.get(); }

    // Maps the existing dynamic buffer with WRITE_DISCARD. No allocation happens here.
    void UpdateConstants(ID3D11DeviceContext* context, const ShaderConstants& constants) const;

private:
    com_ptr<ID3D11Buffer> m_constantBuffer;
    com_ptr<ID3D11SamplerState> m_sampler;
    com_ptr<ID3D11RasterizerState> m_rasterizerState;
};

}  // namespace overlaydesk
