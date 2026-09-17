#include "graphics/RenderResources.h"

#include <cstring>

#include "core/RenderMath.h"
#include "util/Log.h"

namespace overlaydesk {

ShaderConstants BuildShaderConstants(const AppSettings& settings, uint32_t outputWidth,
                                     uint32_t outputHeight, const SourceGeometry& source,
                                     float timeSeconds, bool hasSource, bool editMode,
                                     float editBorderThickness) {
    ShaderConstants c;

    c.outputResolution[0] = static_cast<float>(outputWidth);
    c.outputResolution[1] = static_cast<float>(outputHeight);
    c.sourceResolution[0] = static_cast<float>(source.contentWidth);
    c.sourceResolution[1] = static_cast<float>(source.contentHeight);

    // The frame-pool texture can be larger than the live content between a source resize
    // and the pool being recreated.
    c.sourceUvScale[0] = source.textureWidth > 0 ? static_cast<float>(source.contentWidth) /
                                                       static_cast<float>(source.textureWidth)
                                                 : 1.0f;
    c.sourceUvScale[1] = source.textureHeight > 0 ? static_cast<float>(source.contentHeight) /
                                                        static_cast<float>(source.textureHeight)
                                                  : 1.0f;

    const AspectFit fit = ComputeAspectFit(
        static_cast<float>(source.contentWidth), static_cast<float>(source.contentHeight),
        static_cast<float>(outputWidth), static_cast<float>(outputHeight));
    c.fitScaleOffset[0] = fit.scaleX;
    c.fitScaleOffset[1] = fit.scaleY;
    c.fitScaleOffset[2] = fit.offsetX;
    c.fitScaleOffset[3] = fit.offsetY;

    c.time = timeSeconds;
    c.opacity = ClampIntensity(settings.overlay.opacity);
    c.editBorderThickness = editMode ? editBorderThickness : 0.0f;

    uint32_t mask = 0;
    if (hasSource) {
        mask |= ShaderFeatureHasSource;
    }
    if (editMode) {
        mask |= ShaderFeatureEditBorder;
    }

    const FilterSettings& f = settings.filters;

    if (f.distortion.enabled) {
        mask |= ShaderFeatureDistortion;
    }
    c.distortionAmount = ClampBipolar(f.distortion.amount);
    c.distortionIntensity = ClampIntensity(f.distortion.intensity);
    c.distortionShape = static_cast<int32_t>(f.distortion.shape);

    if (f.vignette.enabled) {
        mask |= ShaderFeatureVignette;
    }
    c.vignetteIntensity = ClampIntensity(f.vignette.intensity);
    c.vignetteSize = f.vignette.size;
    c.vignetteSoftness = f.vignette.softness;
    c.vignetteRoundness = f.vignette.roundness;

    if (f.scanlines.enabled) {
        mask |= ShaderFeatureScanlines;
    }
    c.scanlineIntensity = ClampIntensity(f.scanlines.intensity);
    c.scanlineThickness = f.scanlines.thickness;
    c.scanlineSpacing = f.scanlines.spacing;
    c.scanlineOrientation = static_cast<int32_t>(f.scanlines.orientation);
    c.scanlineScaleMode = static_cast<int32_t>(f.scanlines.scaleMode);
    c.scanlineStyle = static_cast<int32_t>(f.scanlines.style);
    c.scanlineBeamWidth = ClampIntensity(f.scanlines.beamWidth);
    c.scanlineInterlace = ClampIntensity(f.scanlines.interlace);

    if (f.chromaticAberration.enabled) {
        mask |= ShaderFeatureChromatic;
    }
    c.chromaticIntensity = ClampIntensity(f.chromaticAberration.intensity);
    c.chromaticMode = static_cast<int32_t>(f.chromaticAberration.mode);
    c.chromaticEdgeBias = f.chromaticAberration.edgeBias;
    c.chromaticRedShift = f.chromaticAberration.redShift;
    c.chromaticBlueShift = f.chromaticAberration.blueShift;

    if (f.colorCorrection.enabled) {
        mask |= ShaderFeatureColorCorrection;
    }
    c.colorIntensity = ClampIntensity(f.colorCorrection.intensity);
    c.brightness = f.colorCorrection.brightness;
    c.contrast = f.colorCorrection.contrast;
    c.saturation = f.colorCorrection.saturation;
    c.gamma = f.colorCorrection.gamma;
    c.tint[0] = f.colorCorrection.tint[0];
    c.tint[1] = f.colorCorrection.tint[1];
    c.tint[2] = f.colorCorrection.tint[2];
    c.tintAmount = ClampIntensity(f.colorCorrection.tintAmount);
    c.posterize = ClampIntensity(f.colorCorrection.posterize);

    if (f.bloom.enabled) {
        mask |= ShaderFeatureBloom;
    }
    c.bloomIntensity = ClampIntensity(f.bloom.intensity);
    c.bloomThreshold = ClampIntensity(f.bloom.threshold);
    c.bloomRadius = ClampIntensity(f.bloom.radius);
    c.bloomTint[0] = f.bloom.tint[0];
    c.bloomTint[1] = f.bloom.tint[1];
    c.bloomTint[2] = f.bloom.tint[2];

    if (f.falseColour.enabled) {
        mask |= ShaderFeatureFalseColour;
    }
    c.falseColourIntensity = ClampIntensity(f.falseColour.intensity);
    c.falseColourPalette = static_cast<int32_t>(f.falseColour.palette);
    c.falseColourLevels = ClampIntensity(f.falseColour.levels);

    if (f.edgeGlow.enabled) {
        mask |= ShaderFeatureEdgeGlow;
    }
    c.edgeGlowIntensity = ClampIntensity(f.edgeGlow.intensity);
    c.edgeGlowWidth = ClampIntensity(f.edgeGlow.width);
    c.edgeGlowTint[0] = f.edgeGlow.tint[0];
    c.edgeGlowTint[1] = f.edgeGlow.tint[1];
    c.edgeGlowTint[2] = f.edgeGlow.tint[2];

    if (f.lensSoftness.enabled) {
        mask |= ShaderFeatureLensSoftness;
    }
    c.lensSoftnessIntensity = ClampIntensity(f.lensSoftness.intensity);
    c.lensSoftnessCenter = ClampIntensity(f.lensSoftness.center);

    if (f.sharpen.enabled) {
        mask |= ShaderFeatureSharpen;
    }
    c.sharpenIntensity = ClampIntensity(f.sharpen.intensity);
    c.sharpenRadius = ClampIntensity(f.sharpen.radius);

    if (f.lensDirt.enabled) {
        mask |= ShaderFeatureLensDirt;
    }
    c.lensDirtIntensity = ClampIntensity(f.lensDirt.intensity);
    c.lensDirtDensity = ClampIntensity(f.lensDirt.density);
    c.lensDirtSmear = ClampIntensity(f.lensDirt.smear);

    if (f.scope.enabled) {
        mask |= ShaderFeatureScope;
    }
    c.scopeIntensity = ClampIntensity(f.scope.intensity);
    c.scopeSize = f.scope.size;
    c.scopeSoftness = f.scope.softness;
    c.scopeMagnification = f.scope.magnification;
    c.scopeReticle = ClampIntensity(f.scope.reticle);
    c.scopeShape = static_cast<uint32_t>(f.scope.shape);

    const GlitchSettings& g = settings.effects.glitch;
    if (g.enabled) {
        mask |= ShaderFeatureGlitch;
    }
    c.glitchIntensity = ClampIntensity(g.intensity);
    c.glitchFrequency = g.frequency;
    c.glitchBlockSize = g.blockSize;
    c.glitchJitter = g.jitter;
    c.glitchRgbShift = g.rgbShift;
    c.glitchStyle = static_cast<int32_t>(g.style);

    const NoiseSettings& noise = settings.effects.noise;
    if (noise.enabled) {
        mask |= ShaderFeatureNoise;
    }
    c.noiseIntensity = ClampIntensity(noise.intensity);
    c.noiseGrainSize = noise.grainSize;
    c.noiseSpeed = noise.speed;
    c.noiseColorAmount = ClampIntensity(noise.colorAmount);

    const FlickerSettings& flicker = settings.effects.flicker;
    if (flicker.enabled) {
        mask |= ShaderFeatureFlicker;
    }
    c.flickerIntensity = ClampIntensity(flicker.intensity);
    c.flickerSpeed = flicker.speed;

    const JitterSettings& jitter = settings.effects.jitter;
    if (jitter.enabled) {
        mask |= ShaderFeatureJitter;
    }
    c.jitterIntensity = ClampIntensity(jitter.intensity);
    c.jitterSpeed = jitter.speed;

    const ShimmerSettings& shimmer = settings.effects.shimmer;
    if (shimmer.enabled) {
        mask |= ShaderFeatureShimmer;
    }
    c.shimmerIntensity = ClampIntensity(shimmer.intensity);
    c.shimmerSpeed = shimmer.speed;
    c.shimmerScale = ClampIntensity(shimmer.scale);

    const RollingShutterSettings& rolling = settings.effects.rollingShutter;
    if (rolling.enabled) {
        mask |= ShaderFeatureRollingShutter;
    }
    c.rollingShutterIntensity = ClampIntensity(rolling.intensity);
    c.rollingShutterSpeed = rolling.speed;

    const ScanSweepSettings& sweep = settings.effects.scanSweep;
    if (sweep.enabled) {
        mask |= ShaderFeatureScanSweep;
    }
    c.scanSweepIntensity = ClampIntensity(sweep.intensity);
    c.scanSweepSpeed = sweep.speed;
    c.scanSweepWidth = ClampIntensity(sweep.width);

    c.enabledMask = mask;
    return c;
}

void RenderResources::Create(ID3D11Device* device) {
    Reset();

    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = sizeof(ShaderConstants);
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    OS_CHECK_HR(device->CreateBuffer(&bufferDesc, nullptr, m_constantBuffer.put()));

    // Linear + clamp. Clamping matters once distortion starts pushing UVs past the edge:
    // wrapping would smear the opposite side of the frame into the corners.
    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    OS_CHECK_HR(device->CreateSamplerState(&samplerDesc, m_sampler.put()));

    // The fullscreen triangle is generated from SV_VertexID with no regard for winding, so
    // culling has to be off or half the draws would vanish depending on the sign
    // conventions in play.
    D3D11_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.DepthClipEnable = TRUE;
    OS_CHECK_HR(device->CreateRasterizerState(&rasterizerDesc, m_rasterizerState.put()));

    LogInfo("Renderer: shared GPU resources created ({} byte constant buffer).",
            sizeof(ShaderConstants));
}

void RenderResources::Reset() noexcept {
    m_rasterizerState = nullptr;
    m_sampler = nullptr;
    m_constantBuffer = nullptr;
}

void RenderResources::UpdateConstants(ID3D11DeviceContext* context,
                                      const ShaderConstants& constants) const {
    if (!m_constantBuffer) {
        return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(m_constantBuffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        // Deliberately silent: this sits in the per-frame path and a Map failure means the
        // device is going away, which the caller detects through the swap chain instead.
        return;
    }
    std::memcpy(mapped.pData, &constants, sizeof(constants));
    context->Unmap(m_constantBuffer.get(), 0);
}

}  // namespace overlaydesk
