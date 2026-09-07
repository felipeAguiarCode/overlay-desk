# UI Spec

## Estrutura

```text
Overlay Desk

[ Overlay ]
[ Filters ]
[ Effects ]
[ Presets ]
[ Settings ]
```

## Overlay

Controles:

- enabled;
- Edit Overlay;
- Match Target Window;
- position;
- size;
- resizable;
- lock position;
- lock size;
- lock aspect ratio;
- aspect ratio;
- always on top;
- click-through;
- opacity;
- fullscreen.

## Filters

Cards:

- Distortion
- Vignette
- Scanlines
- Chromatic Aberration
- Color Correction

Padrão:

```text
┌─────────────────────────────┐
│ VIGNETTE              [ON]  │
│                             │
│ Intensity          48%      │
│ ─────────────●────────      │
│                             │
│ Advanced ▾                  │
└─────────────────────────────┘
```

## Effects

Cards:

- Glitch

## Presets

Ações:

- New;
- Save;
- Save As;
- Duplicate;
- Rename;
- Delete;
- Apply.

## Settings

- FPS mode;
- log level;
- launch behavior;
- hotkeys;
- config folder;
- reset defaults.

## UX principles

- controles simples primeiro;
- parâmetros avançados recolhíveis;
- valores mostrados numericamente;
- double click no slider pode restaurar default;
- reset por módulo;
- estado ON/OFF sempre evidente.
