# Especificação — Performance e Memória

## Objetivo

Overlay Desk deve poder permanecer executando durante gameplay sem se tornar mais pesado que o emulador.

## Princípios

### 1. GPU-first

Processamento de frame ocorre na GPU.

### 2. Zero-copy quando possível

Fluxo desejado:

```text
Capture D3D Texture
↓
Shader
↓
SwapChain
```

Evitar:

```text
GPU
↓
CPU RAM
↓
GPU
```

### 3. Sem frame history

MVP não mantém frames anteriores.

### 4. Sem allocation per frame

Proibido no hot path quando evitável:

- new/delete;
- CreateTexture2D;
- CreateBuffer;
- shader compilation;
- JSON parse;
- filesystem I/O.

### 5. Recursos reutilizados

Manter:

- sampler;
- constant buffer;
- shader;
- RTV;
- SRV;
- swap chain.

### 6. Fusão de filtros

Preferir um shader principal.

## Metas

### RAM — 1080p

Meta:

- ideal: <150 MB;
- aceitável inicial: <250 MB.

### Estabilidade

Teste de 60 minutos:

- memória não pode crescer continuamente;
- handles não podem crescer continuamente;
- recursos D3D não podem crescer continuamente.

### Frame pacing

Suportar:

- match source;
- 60 FPS;
- 30 FPS;
- unlimited.

Default:

- match source;
- limitar a 60 quando apropriado.

## Comportamento quando source não está ativa

### Minimized

- pausar ou reduzir renderização;
- não processar frames inexistentes.

### Closed

- encerrar capture session;
- liberar resources da source;
- manter aplicação aberta.

## Textures

1080p BGRA:

```text
1920 × 1080 × 4 bytes ≈ 8 MB
```

4K BGRA:

```text
3840 × 2160 × 4 bytes ≈ 33 MB
```

Por isso:

- evitar múltiplas render targets;
- evitar cópias;
- evitar double buffering extra além do necessário.

## Profiling

Medir:

- Working Set;
- Private Bytes;
- GPU utilization;
- GPU memory;
- FPS;
- frame time;
- dropped frames;
- handle count.

## Ferramentas sugeridas

- Visual Studio Diagnostics;
- PIX for Windows;
- Windows Performance Recorder;
- Windows Performance Analyzer.

## Release build

Performance deve ser validada em Release.

Debug não é referência de consumo final.
