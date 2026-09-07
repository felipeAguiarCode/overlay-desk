# Roadmap de Implementação

## Milestone 0 — Bootstrap

### Entregas

- CMake;
- estrutura de diretórios;
- app Win32;
- logging;
- configuração Debug/Release;
- D3D11 device.

### Gate

Aplicação abre e fecha sem leak evidente.

---

## Milestone 1 — Overlay Window

### Entregas

- overlay HWND;
- always-on-top;
- borderless;
- click-through;
- opacity;
- resize;
- move;
- lock size;
- lock position;
- aspect ratio;
- fullscreen;
- edit mode.

### Gate

Overlay funciona sobre aplicações comuns sem capturar input quando click-through está ON.

---

## Milestone 2 — Capture

### Entregas

- listar windows;
- selecionar target;
- criar GraphicsCaptureItem;
- frame pool;
- session;
- receber texture;
- detectar source resize;
- detectar target close.

### Gate

Frame do target aparece sem filtros no overlay.

---

## Milestone 3 — Render Base

### Entregas

- swap chain;
- fullscreen quad;
- sampler;
- texture binding;
- constant buffer;
- base HLSL shader.

### Gate

Resize da overlay não quebra rendering.

---

## Milestone 4 — Core Filters

### Distortion
- fisheye;
- anti-fisheye.

### Vignette
- intensity;
- size;
- softness;
- roundness.

### Scanlines
- intensity;
- thickness;
- spacing;
- orientation;
- relative/pixel perfect.

### Gate

Filtros podem ser ligados/desligados individualmente.

---

## Milestone 5 — Advanced Filters

### Chromatic Aberration
- radial;
- horizontal;
- vertical;
- edge.

### Color Correction
- brightness;
- contrast;
- saturation;
- gamma.

### Gate

Estado neutro reproduz imagem original sem alteração perceptível.

---

## Milestone 6 — Effects

### Glitch
- intensity;
- frequency;
- block size;
- jitter;
- RGB shift.

### Gate

Glitch não utiliza frame history.

---

## Milestone 7 — UI

### Entregas

Tabs:

- Overlay
- Filters
- Effects
- Presets
- Settings

Cards padronizados.

### Gate

Usuário configura todas as funções sem editar arquivos manualmente.

---

## Milestone 8 — Persistence

### Entregas

- settings.json;
- presets;
- schema version;
- default presets;
- CRUD.

### Gate

Fechar/reabrir preserva estado.

---

## Milestone 9 — Performance

### Entregas

- FPS cap;
- pause minimized;
- resource reuse;
- profiling;
- leak audit.

### Gate

Teste de 60 minutos sem crescimento contínuo de memória.

---

## Milestone 10 — Release

### Entregas

- build Release;
- portable package;
- versioning;
- license;
- README usuário;
- crash logging mínimo.
