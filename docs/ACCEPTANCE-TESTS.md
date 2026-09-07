# Testes de Aceite

## AT-001 — Startup

Dado que a aplicação foi instalada,
quando for aberta,
então o painel de controle deve aparecer sem criar overlay ativo automaticamente caso não exista target válido.

## AT-002 — Target selection

Quando o usuário selecionar uma janela,
a captura deve iniciar sem reiniciar a aplicação.

## AT-003 — Overlay

Quando o overlay for ativado,
deve aparecer como janela independente sobre outras janelas.

## AT-004 — Always-on-top

Com always-on-top ON,
o overlay deve permanecer acima de janelas normais.

## AT-005 — Click-through

Com click-through ON,
cliques devem atingir a aplicação abaixo.

## AT-006 — Edit Mode

Ao entrar em Edit Mode:

- click-through fica temporariamente OFF;
- overlay pode ser movido;
- overlay pode ser redimensionado.

## AT-007 — Resize

Ao redimensionar overlay,
a imagem deve continuar renderizando sem crash ou artefato permanente.

## AT-008 — Distortion

Valores:

```text
-1 -> anti-fisheye
0 -> normal
+1 -> fisheye
```

Transição deve ser contínua.

## AT-009 — Vignette

OFF:
- não altera imagem.

ON:
- bordas escurecem;
- intensity funciona;
- size funciona;
- softness funciona;
- roundness funciona.

## AT-010 — Scanlines

OFF:
- custo visual zero.

ON:
- parâmetros alteram padrão sem reiniciar renderer.

## AT-011 — Chromatic Aberration

OFF:
- RGB permanece alinhado.

ON:
- canais se deslocam conforme intensidade.

## AT-012 — Glitch

OFF:
- nenhum glitch.

ON:
- eventos respeitam frequency;
- magnitude respeita intensity.

## AT-013 — Preset

Salvar preset, mudar valores e reaplicar preset deve restaurar os valores salvos.

## AT-014 — Target closed

Se target fechar:

- aplicação não fecha;
- captura encerra;
- overlay entra em estado seguro;
- usuário pode escolher novo target.

## AT-015 — Minimized

Se source minimizar:

- aplicação não deve continuar processando frames inexistentes em loop agressivo.

## AT-016 — 60-minute stability

Executar por 60 minutos em 1080p.

Passa se:

- memória não cresce continuamente;
- handles não crescem continuamente;
- app permanece responsivo.

## AT-017 — Multi-monitor

Mover overlay para segundo monitor deve manter:

- render;
- resize;
- click-through;
- DPI coerente.

## AT-018 — Config corruption

Com settings JSON inválido:

- app abre;
- usa defaults;
- registra erro;
- não crasha.
