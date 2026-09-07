# ADR-0002 — Modelo de janela Overlay

## Status

Accepted

## Contexto

O produto é definido como um overlay independente executando por cima de outra aplicação.

O overlay precisa:

- ficar no topo;
- ser redimensionável;
- permitir click-through;
- ser reposicionável;
- ser editável;
- não depender da hierarquia de janelas do emulador.

## Decisão

Criar o overlay como HWND top-level independente.

Não usar:

- child window;
- injeção no target;
- hook gráfico no processo do emulador.

## Modos

### Play Mode

- borderless;
- topmost conforme configuração;
- click-through;
- sem resize handles visíveis.

### Edit Mode

- click-through OFF;
- border ON;
- resize handles;
- move habilitado.

### Locked Mode

- posição fixa;
- tamanho fixo.

### Fullscreen Mode

- bounds do monitor;
- borderless.

## Configurações

```text
enabled
alwaysOnTop
clickThrough
resizable
lockPosition
lockSize
lockAspectRatio
aspectRatio
opacity
fullscreen
```

## Consequências

O overlay pode ser usado sobre:

- emuladores;
- players;
- jogos;
- qualquer janela capturável.

O usuário controla a composição espacial sem depender da source.
