# Data Model

## AppSettings

```cpp
struct AppSettings {
    uint32_t schemaVersion;
    OverlaySettings overlay;
    FilterSettings filters;
    EffectSettings effects;
    RenderSettings render;
    UiSettings ui;
};
```

## FilterSettings

```cpp
struct FilterSettings {
    DistortionSettings distortion;
    VignetteSettings vignette;
    ScanlineSettings scanlines;
    ChromaticAberrationSettings chromaticAberration;
    ColorCorrectionSettings colorCorrection;
};
```

## EffectSettings

```cpp
struct EffectSettings {
    GlitchSettings glitch;
};
```

## ShaderConstants

Recomenda-se empacotar parâmetros usados por frame em um constant buffer alinhado corretamente para HLSL.

Exemplo conceitual:

```cpp
struct alignas(16) ShaderConstants {
    float2 outputResolution;
    float time;
    float padding0;

    float distortionAmount;
    float distortionIntensity;
    float vignetteIntensity;
    float vignetteSize;

    float vignetteSoftness;
    float vignetteRoundness;
    float scanlineIntensity;
    float scanlineThickness;

    float scanlineSpacing;
    float chromaticIntensity;
    float chromaticEdgeBias;
    float glitchIntensity;

    float glitchFrequency;
    float glitchBlockSize;
    float glitchJitter;
    float glitchRgbShift;

    float brightness;
    float contrast;
    float saturation;
    float gamma;
};
```

Flags enabled podem ser:

- bitmask;
- integers alinhados;
- variantes de shader.

Para MVP, um bitmask é suficiente.

## Runtime-only

Não persistir:

```text
HWND
ID3D11Texture2D*
IDXGISwapChain*
GraphicsCaptureSession
frame handles
```
