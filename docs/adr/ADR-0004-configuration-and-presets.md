# ADR-0004 — Configuração e presets

## Status

Accepted

## Decisão

Persistência local em JSON.

Config global e presets são separados.

## Local

```text
%APPDATA%\OverlayDesk\
```

## Motivo

- simples;
- legível;
- fácil de versionar;
- fácil de depurar;
- suficiente para o volume de dados.

SQLite não é necessário para o MVP.

## Evolução

Se presets, biblioteca ou metadata crescerem significativamente, SQLite pode ser revisitado.
