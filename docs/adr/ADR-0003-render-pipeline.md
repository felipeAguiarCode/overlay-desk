# ADR-0003 — Pipeline de renderização

## Status

Accepted

## Contexto

O produto possui vários filtros e efeitos, porém deve permanecer econômico em RAM e GPU.

Uma arquitetura multi-pass ingênua poderia criar várias render targets intermediárias.

## Decisão

No MVP, fundir o máximo possível de operações em um pixel shader principal.

Pipeline lógico:

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

A ordem lógica pode existir dentro de um shader único.

## Motivação

Reduz:

- render targets intermediários;
- memory bandwidth;
- GPU allocations;
- VRAM;
- complexidade.

## Exceções

Um efeito poderá virar pass dedicado se:

- depender de frame anterior;
- exigir blur;
- exigir feedback temporal;
- exigir múltiplas amostragens incompatíveis.

Essa decisão exige novo ADR ou atualização deste ADR.

## Desativação

Filtros/effects OFF devem:

- evitar trabalho desnecessário;
- usar branch uniforme ou shader variant quando fizer sentido.

## Temporal effects

Glitch do MVP é procedural.

Não manter histórico de frames.
