# CLAUDE.md — Overlay Desk

## Papel

Você está implementando o projeto **Overlay Desk**.

Trate os documentos em `/docs` como requisitos normativos.

Não altere decisões arquiteturais sem registrar um novo ADR.

## Objetivo

Construir uma aplicação Windows nativa que:

1. capture uma janela escolhida pelo usuário;
2. processe essa imagem na GPU;
3. renderize o resultado em uma janela overlay independente;
4. mantenha o overlay sempre no topo quando configurado;
5. permita mover e redimensionar o overlay;
6. permita click-through;
7. aplique filtros e efeitos em tempo real;
8. tenha baixo consumo de RAM e GPU;
9. persista configurações e presets.

## Stack obrigatória

- C++20
- Win32
- C++/WinRT
- Windows.Graphics.Capture
- Direct3D 11
- HLSL
- Dear ImGui
- CMake
- JSON

## Restrições

Não usar:

- Electron
- Chromium Embedded Framework
- WebView como UI principal
- Node.js no runtime
- captura baseada em screenshots repetidos da CPU
- processamento de imagem por CPU no loop principal
- histórico de frames sem necessidade explícita
- múltiplas cópias desnecessárias da textura capturada

## Arquitetura esperada

```text
OverlayDesk.exe
├── Application
├── Core
│   ├── AppState
│   ├── Settings
│   └── Presets
├── Windowing
│   ├── ControlWindow
│   ├── OverlayWindow
│   ├── TargetWindow
│   └── WindowTracker
├── Capture
│   ├── CaptureSession
│   └── FrameSource
├── Graphics
│   ├── D3D11Device
│   ├── SwapChain
│   ├── Renderer
│   ├── ShaderManager
│   └── RenderResources
├── Filters
├── Effects
├── UI
└── Shaders
```

## Ordem de implementação

Nunca tente implementar tudo de uma vez.

### Fase 1
- bootstrap CMake;
- janela de controle;
- inicialização D3D11;
- janela overlay simples;
- always-on-top;
- click-through;
- resize;
- modo de edição.

### Fase 2
- enumeração de janelas;
- seleção de target;
- Windows.Graphics.Capture;
- textura capturada;
- render da captura no overlay.

### Fase 3
- shader base;
- distortion;
- vignette;
- scanlines;
- chromatic aberration;
- color correction.

### Fase 4
- glitch;
- presets;
- persistência;
- performance tuning.

## Regras de implementação

### Overlay

O overlay é uma janela independente.

Não deve ser child window do emulador.

Precisa funcionar mesmo quando o target muda de posição.

Estados:

- normal;
- edit mode;
- click-through;
- locked;
- fullscreen;
- resizable;
- fixed size.

### Efeitos e filtros

Todo módulo deve possuir:

```cpp
bool enabled;
float intensity;
```

Parâmetros específicos são adicionais.

### Distortion

Usar um parâmetro bipolar:

```text
-1.0 = anti-fisheye máximo
 0.0 = sem distorção
+1.0 = fisheye máximo
```

### Render pipeline inicial

```text
Captured Texture
↓
Glitch UV
↓
Distortion
↓
Chromatic Aberration
↓
Color Correction
↓
Scanlines
↓
Vignette
↓
Output
```

A ordem só pode ser alterada com justificativa técnica.

### Performance

Prioridade:

1. estabilidade;
2. baixa latência;
3. baixo uso de RAM;
4. baixo uso de GPU;
5. fidelidade visual.

O frame deve permanecer na GPU.

Evite:
- readback;
- staging texture;
- cópias CPU;
- alocação por frame.

Recursos de GPU devem ser reutilizados.

## Processo de trabalho

Para cada feature:

1. leia os requisitos;
2. identifique interfaces afetadas;
3. implemente a menor unidade funcional;
4. compile;
5. corrija warnings;
6. execute testes;
7. valide performance;
8. atualize checklist/backlog quando aplicável.

## Definition of Done

Uma feature só é considerada concluída quando:

- compila em Release;
- não introduz warning relevante;
- funciona com resize;
- funciona com DPI scaling;
- não quebra click-through;
- não cria vazamento de recurso;
- mantém configuração persistível;
- possui comportamento definido quando desativada;
- possui teste manual documentado;
- não aumenta memória sem justificativa.
