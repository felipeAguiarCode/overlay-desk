# Arquitetura — Overlay Desk

## 1. Visão geral

```text
┌──────────────────────────────┐
│          Emulator            │
│          Target HWND         │
└──────────────┬───────────────┘
               │
               │ Windows.Graphics.Capture
               ▼
┌──────────────────────────────┐
│      Capture Session         │
│ D3D11 capture frame texture  │
└──────────────┬───────────────┘
               │
               ▼
┌──────────────────────────────┐
│       Render Pipeline        │
│                              │
│ Glitch UV                    │
│ Distortion                   │
│ Chromatic Aberration         │
│ Color Correction             │
│ Scanlines                    │
│ Vignette                     │
└──────────────┬───────────────┘
               │
               ▼
┌──────────────────────────────┐
│        Overlay HWND          │
│ Borderless / Topmost         │
│ Click-through optional       │
└──────────────────────────────┘
```

## 2. Componentes

### Application

Responsável por:

- lifecycle;
- inicialização;
- shutdown;
- loop principal;
- coordenação dos subsistemas.

### AppState

Estado runtime:

- target selecionado;
- overlay geometry;
- overlay flags;
- filter settings;
- effect settings;
- FPS mode;
- active preset.

### TargetWindow

Armazena:

- HWND;
- process id;
- title;
- bounds;
- visibility;
- minimized;
- monitor.

### WindowTracker

Monitora:

- target destroyed;
- target resized;
- target moved;
- target minimized;
- monitor changes.

No MVP, o overlay não precisa seguir automaticamente o target o tempo todo.

O usuário pode:

- mover manualmente;
- usar Match Target Window.

Auto-follow pode ser uma configuração futura.

### OverlayWindow

Responsável por:

- HWND;
- borderless mode;
- topmost;
- click-through;
- resize;
- move;
- edit mode;
- DPI;
- fullscreen.

### ControlWindow

Dear ImGui.

Nunca deve ser requisito para o renderer continuar ativo.

### CaptureSession

Responsável por:

- criar GraphicsCaptureItem;
- Direct3D11CaptureFramePool;
- GraphicsCaptureSession;
- receber frames;
- detectar resize da source;
- recriar frame pool quando necessário.

### D3D11Device

Centraliza:

- ID3D11Device;
- ID3D11DeviceContext;
- adapter;
- DXGI factory.

Deve haver um único device principal sempre que possível.

### Renderer

Responsável por:

- obter texture atual;
- bind resources;
- constant buffers;
- shader invocation;
- swap chain;
- present.

### ShaderManager

Responsável por:

- carregar/compilar shaders;
- variants futuras;
- hot reload opcional em Debug.

### SettingsRepository

Persistência JSON.

### PresetRepository

CRUD de presets.

## 3. Threads

MVP deve evitar threading excessivo.

Modelo recomendado:

- UI / Win32 message loop;
- callbacks de captura;
- GPU work no mesmo fluxo coordenado;
- I/O de configuração fora do hot path.

Não criar worker threads por filtro.

## 4. Recursos de GPU

Evitar:

- criar texture por frame;
- criar sampler por frame;
- criar buffer por frame;
- compilar shader por frame.

Reutilizar:

- constant buffer;
- sampler;
- swap chain;
- RTV;
- shader resource views.

## 5. Resize

Existem dois resizes independentes.

### Source resize

O target muda de resolução.

A captura deve recriar recursos necessários.

### Overlay resize

O usuário muda o tamanho da janela overlay.

A swap chain deve ser redimensionada.

Filtros baseados em UV normalizado continuam proporcionais.

## 6. Coordenadas

Shaders devem usar UV normalizado.

```text
(0,0) ---------------- (1,0)
  |                      |
  |                      |
(0,1) ---------------- (1,1)
```

Scanlines em pixel-perfect devem utilizar resolução do output.

## 7. Configuração runtime

Estruturas sugeridas:

```cpp
struct OverlaySettings {
    bool enabled;
    bool alwaysOnTop;
    bool clickThrough;
    bool resizable;
    bool lockPosition;
    bool lockSize;
    bool lockAspectRatio;
    float opacity;
    int x;
    int y;
    int width;
    int height;
};

struct FilterBase {
    bool enabled;
    float intensity;
};

struct DistortionSettings : FilterBase {
    float amount;
};

struct VignetteSettings : FilterBase {
    float size;
    float softness;
    float roundness;
};

struct ScanlineSettings : FilterBase {
    float thickness;
    float spacing;
    int orientation;
    int scaleMode;
};

struct ChromaticAberrationSettings : FilterBase {
    int mode;
    float edgeBias;
    float redShift;
    float blueShift;
};

struct ColorCorrectionSettings : FilterBase {
    float brightness;
    float contrast;
    float saturation;
    float gamma;
};

struct GlitchSettings : FilterBase {
    float frequency;
    float blockSize;
    float jitter;
    float rgbShift;
};
```

## 8. Error handling

Falhas esperadas:

- target fechado;
- target protegido;
- captura indisponível;
- D3D device lost;
- monitor desconectado;
- swap chain resize failure;
- JSON corrompido.

A aplicação deve:

- registrar erro;
- liberar recursos afetados;
- voltar para estado seguro;
- permitir nova seleção.

## 9. Logging

Logs mínimos:

- startup;
- selected target;
- capture start/stop;
- device creation;
- overlay create/destroy;
- shader compile failure;
- preset load;
- unexpected exception.

Não logar cada frame.
