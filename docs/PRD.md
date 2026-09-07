# PRD — Overlay Desk

## 1. Visão do produto

Overlay Desk é uma aplicação Windows desktop nativa que permite ao usuário criar uma janela visual sobreposta a um emulador ou outro aplicativo e aplicar filtros e efeitos de pós-processamento em tempo real.

O produto é inspirado na categoria de aplicações como ShaderGlass, mas possui foco específico em:

- distorção fisheye;
- anti-fisheye;
- vinheta;
- scanlines;
- aberração cromática;
- glitch;
- controle manual do tamanho e posição do overlay;
- baixo consumo de RAM;
- arquitetura GPU-first.

## 2. Problema

Emuladores e jogos antigos possuem capacidades distintas de shaders. Alguns não possuem:

- scanlines customizáveis;
- fisheye;
- anti-fisheye;
- vinheta;
- aberração cromática;
- glitch;
- pipeline consistente de pós-processamento.

O usuário precisa de uma camada visual externa que funcione independentemente do emulador.

## 3. Objetivo

Permitir que qualquer usuário Windows:

1. selecione uma janela de origem;
2. visualize a captura processada em um overlay;
3. posicione e redimensione o overlay;
4. ligue ou desligue filtros;
5. ligue ou desligue efeitos;
6. controle a intensidade global de cada módulo;
7. salve presets;
8. reutilize configurações;
9. mantenha consumo de recursos previsível.

## 4. Não objetivos

Não faz parte do MVP:

- gravar vídeo;
- transmitir via streaming;
- editar vídeos offline;
- substituir OBS;
- injetar DLL no emulador;
- modificar memória do emulador;
- criar cheats;
- capturar áudio;
- multiplayer;
- cloud sync;
- marketplace de shaders.

## 5. Público-alvo

### Primário

- jogadores de emuladores;
- usuários de retro gaming;
- pessoas que desejam simular CRT/LCD;
- usuários que gostam de customização visual.

### Secundário

- streamers;
- criadores de conteúdo;
- jogadores de PC que desejam pós-processamento externo.

## 6. Fluxo principal

```text
Abrir Overlay Desk
↓
Selecionar janela de origem
↓
Criar/ativar Overlay
↓
Ajustar posição e tamanho
↓
Ativar filtros
↓
Ativar efeitos
↓
Ajustar intensidades
↓
Salvar preset
↓
Jogar
```

## 7. Requisitos funcionais

### RF-001 — Seleção de janela

O usuário deve poder listar janelas disponíveis e selecionar uma como fonte.

### RF-002 — Overlay independente

O overlay deve ser uma HWND independente da janela capturada.

### RF-003 — Always on top

O overlay deve possuir configuração de always-on-top.

Default: ligado.

### RF-004 — Click-through

O usuário deve poder ativar click-through.

Default durante gameplay: ligado.

### RF-005 — Resize

O overlay deve ser redimensionável quando permitido.

Configurações:

- resizable;
- lock size;
- lock position;
- lock aspect ratio;
- aspect ratio;
- fullscreen.

### RF-006 — Modo de edição

O overlay deve possuir modo de edição que:

- desliga temporariamente click-through;
- mostra borda;
- mostra resize handles;
- permite mover;
- permite redimensionar.

### RF-007 — Match target window

Deve existir um comando que ajuste o overlay às dimensões da janela de origem.

### RF-008 — Opacidade

O overlay deve ter controle global de opacidade.

### RF-009 — Sistema de filtros

Filtros iniciais:

- distortion;
- vignette;
- scanlines;
- chromatic aberration;
- color correction.

Todos devem possuir:

- enabled;
- intensity.

### RF-010 — Sistema de efeitos

Efeitos iniciais:

- glitch.

Todo efeito deve possuir:

- enabled;
- intensity.

### RF-011 — Distortion

O módulo deve usar escala bipolar:

- negativo = anti-fisheye;
- zero = normal;
- positivo = fisheye.

### RF-012 — Vignette

Parâmetros:

- enabled;
- intensity;
- size;
- softness;
- roundness.

### RF-013 — Scanlines

Parâmetros:

- enabled;
- intensity;
- thickness;
- spacing;
- orientation;
- scale mode.

Orientações:

- horizontal;
- vertical;
- grid.

Scale mode:

- relative;
- pixel perfect.

### RF-014 — Chromatic Aberration

Parâmetros:

- enabled;
- intensity;
- mode;
- edge bias;
- red shift;
- blue shift.

Modes:

- radial;
- horizontal;
- vertical;
- edge.

### RF-015 — Color Correction

Parâmetros:

- enabled;
- intensity;
- brightness;
- contrast;
- saturation;
- gamma.

### RF-016 — Glitch

Parâmetros:

- enabled;
- intensity;
- frequency;
- block size;
- jitter;
- RGB shift.

O efeito deve ser procedural no MVP.

### RF-017 — Presets

O usuário deve poder:

- salvar preset;
- renomear preset;
- aplicar preset;
- deletar preset;
- duplicar preset.

### RF-018 — Persistência

A aplicação deve persistir:

- último target opcional;
- geometria da janela;
- configurações do overlay;
- filtros;
- efeitos;
- presets;
- preferências da UI.

## 8. Requisitos não funcionais

### RNF-001 — Performance

O processamento visual deve ocorrer na GPU.

### RNF-002 — RAM

Não manter histórico de frames no MVP.

Não realizar readback de GPU por frame.

Meta de memória do processo em cenário típico 1080p:

- ideal: abaixo de 150 MB;
- aceitável inicial: abaixo de 250 MB;
- investigar regressão acima de 250 MB.

Esses valores são metas de engenharia, não garantias rígidas.

### RNF-003 — FPS

Modos:

- match source;
- 60 FPS cap;
- 30 FPS cap;
- unlimited.

Default: match source com cap operacional de 60 FPS quando aplicável.

### RNF-004 — Pausa

Quando a fonte estiver minimizada ou indisponível:

- reduzir/parar render;
- não executar filtros desnecessariamente.

### RNF-005 — GPU

Evitar passes intermediários quando os efeitos puderem ser fundidos.

### RNF-006 — Alocações

Não alocar recursos gráficos por frame.

### RNF-007 — DPI

Compatível com DPI scaling do Windows.

### RNF-008 — Multi-monitor

Overlay deve suportar mover entre monitores.

### RNF-009 — Crash safety

Falha de captura não deve encerrar a aplicação de forma abrupta.

## 9. UX

Navegação:

- Overlay
- Filters
- Effects
- Presets
- Settings

Cada módulo visual deve usar um card consistente.

Exemplo:

```text
VIGNETTE                  [ON]

Intensity
0 ─────────────●──── 48%

Size
0 ───────────────●── 64%

Softness
0 ─────────────────● 78%
```

Quando OFF, parâmetros avançados podem permanecer visíveis mas desabilitados.

## 10. Estados do overlay

### Editing

- click-through OFF;
- borda ON;
- resize ON quando permitido;
- movimento habilitado.

### Playing

- borderless;
- click-through ON;
- topmost conforme configuração.

### Locked

- posição travada;
- tamanho travado;
- click-through ON.

### Fullscreen

- ocupa monitor selecionado;
- borderless;
- topmost configurável.

## 11. Presets iniciais sugeridos

O produto pode iniciar com:

- Neutral
- Soft CRT
- Arcade CRT
- Curved CRT
- Game Boy
- Game Boy Advance
- SNES
- Mega Drive
- PlayStation
- Custom

Presets não devem impedir customização posterior.

## 12. Critérios de sucesso do MVP

MVP concluído quando:

- captura uma janela;
- cria overlay independente;
- resize funciona;
- click-through funciona;
- modo de edição funciona;
- distortion funciona;
- vignette funciona;
- scanlines funcionam;
- chromatic aberration funciona;
- color correction funciona;
- glitch funciona;
- presets persistem;
- memória permanece estável em sessão prolongada;
- não há crescimento contínuo de recursos.
