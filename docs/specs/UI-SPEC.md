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

## Aviso de fonte

Sempre que **não** houver frame chegando, um aviso aparece acima das abas — logo, portanto, em
todas as páginas, e não só na de Target.

Dispara em qualquer um destes casos:

```text
nenhum alvo selecionado
alvo fechado
captura pausada (fonte minimizada, ou overlay oculto)
captura falhou ou não é suportada
captura rodando e nenhum frame chegou ainda
```

O último é o que mais importa e o menos óbvio: uma janela que já estava minimizada quando foi
escolhida inicia a captura normalmente e depois não manda nada. O estado fica `Running`, e sem
o aviso a tela é indistinguível de um overlay funcionando.

O texto explica a consequência prática: os filtros continuam sendo aplicados, mas ao fundo de
"sem sinal" e não a uma imagem. Distortion, Jitter, Shimmer, Rolling Shutter e Glitch deformam
esse fundo; Chromatic Aberration, Lens Softness, Bloom, Sharpen e Edge Glow leem a textura
capturada e não têm o que fazer sem ela.

Quando a fonte está minimizada, o aviso traz **`Restore source window`**, que restaura a janela
de origem. A barra de status continua existindo e não muda — o aviso é adicional, não a
substitui.

## Fundo de "sem sinal"

Sem frame, o overlay desenha um hachurado diagonal escuro e de baixo contraste, e **não** um
preenchimento chapado.

Chapado era ambíguo em duas direções ao mesmo tempo: parecia um overlay funcionando, e não
tinha como mostrar mudança geométrica nenhuma — deformar as coordenadas de uma cor constante é
uma operação nula. O resultado é que, sem fonte, os filtros de cor pareciam funcionar e os
geométricos pareciam quebrados.

O padrão é calculado a partir do UV **já distorcido**, então os estágios de UV agem sobre ele
visivelmente. É estático de propósito: o overlay ocioso só é redesenhado quando um repaint é
pedido, e um padrão animado ou travaria ou obrigaria a redesenhar continuamente, o que o
RNF-004 e o AT-015 proíbem.

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
