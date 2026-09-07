# ADR-0001 — Stack tecnológica

## Status

Accepted

## Contexto

Overlay Desk exige:

- captura de janela;
- renderização em tempo real;
- baixa latência;
- baixo consumo de RAM;
- shaders;
- controle fino de HWND;
- click-through;
- always-on-top;
- resize;
- Windows como plataforma primária.

## Decisão

Usar:

- C++20;
- Win32;
- C++/WinRT;
- Windows.Graphics.Capture;
- Direct3D 11;
- HLSL;
- Dear ImGui;
- CMake;
- JSON.

## Justificativa

### C++20

Permite controle explícito de:

- lifetime;
- memória;
- COM;
- recursos gráficos;
- integração Win32.

### Windows.Graphics.Capture

É adequada para captura moderna de janelas no Windows e integra com recursos gráficos Direct3D.

### Direct3D 11

Escolhido por:

- maturidade;
- compatibilidade;
- simplicidade relativa;
- integração natural com Windows.Graphics.Capture;
- suporte suficiente para os shaders do produto.

### HLSL

Adequado para:

- distortion;
- vignette;
- scanlines;
- RGB shift;
- glitch procedural;
- color correction.

### Dear ImGui

Adequado para painel técnico e compacto.

Permite compartilhar a infraestrutura D3D11 da aplicação.

## Alternativas rejeitadas

### Electron

Rejeitado por:

- footprint;
- runtime Chromium;
- memória desnecessária;
- arquitetura mais indireta para captura e render.

### Tauri

Poderia reduzir footprint da UI, mas o núcleo continuaria nativo.

Não traz vantagem suficiente para justificar duas stacks.

### WinUI 3

Pode ser revisitado futuramente para UI mais polida.

Não é necessário para o MVP.

### Direct3D 12

Mais complexo sem benefício proporcional para este workload.

## Consequências

Positivas:

- alto controle;
- baixo overhead;
- pipeline GPU eficiente.

Negativas:

- complexidade maior de C++;
- necessidade de cuidado com COM e recursos;
- curva de debugging gráfico.
