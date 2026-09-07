# Backlog

Legenda: `[x]` concluido, `[~]` parcial, `[ ]` pendente.

Estado em 2026-09-06: Milestones 0-8 e 10 concluidos; Milestone 9 parcial.
Todas as features do MVP estao implementadas: captura, overlay, os filtros, os efeitos, a UI
completa e o sistema de presets. O pacote portatil, a licenca MIT, o resource de versao e o
crash logging estao prontos.
Depois do MVP entraram: tint de cor, o filtro Scope, tres efeitos novos (noise, flicker,
jitter), cinco formas de distortion, cinco estilos de scanline, dois modos novos de aberracao
cromatica e 41 presets de fabrica (ADR-0007 e ADR-0008).
Em seguida, a direcao tatica (ADR-0009): quatro filtros novos (bloom, false colour, edge glow,
lens dirt), tres efeitos novos (shimmer, rolling shutter, scan sweep), o estilo digital do
glitch, a forma quadTube do scope, o posterize da correcao de cor, hotkeys globais
configuraveis e 30 presets novos - 71 de fabrica no total.
Corrigido no mesmo passo um defeito antigo: o mapeamento de aspect-fit era calculado e enviado
ao shader mas nunca aplicado, entao a fonte era esticada para preencher o overlay em vez de ser
encaixada, e as barras de letterbox transparentes nunca existiam. O teste automatizado
`shader.pipeline` renderiza o pixel shader compilado fora da tela e mede isso, junto com o
AT-010 modulo a modulo e a distorcao bipolar.
O mesmo harness expos um segundo defeito: a metade negativa da distorcao abria uma borda
transparente porque o mapa lia de fora do quadro capturado. Corrigido no ADR-0010 - o
anti-fisheye passa a cortar, que e a unica alternativa honesta a mostrar um buraco.
Pendencia unica para fechar o MVP: rodar os 60 minutos do AT-016 ate o fim
(`scripts\stability-test.ps1`).

## P0 — MVP

- [x] Bootstrap C++20/CMake
- [x] Win32 message loop
- [x] D3D11 device
- [x] Overlay HWND
- [x] Borderless
- [x] Always-on-top
- [x] Click-through
- [x] Resize
- [x] Move
- [x] Edit Mode
- [x] Lock Size
- [x] Lock Position
- [x] Aspect Ratio Lock
- [x] Fullscreen
- [x] Window enumeration
- [x] Target selection
- [x] Windows.Graphics.Capture
- [x] Source resize handling
- [x] Target close handling
- [x] Base renderer
- [x] Shader constants
- [x] Distortion (5 formas: radial, crt, cylindrical, vertical, corner)
- [x] Vignette
- [x] Scanlines (5 estilos, incluindo aperture grille e slot mask)
- [x] Chromatic Aberration (6 modos, incluindo prism e barrel)
- [x] Color Correction (com tint multiplicativa)
- [x] Scope (abertura optica, magnificacao, reticulo, binoculo)
- [x] Glitch
- [x] Dear ImGui control panel
- [x] Settings persistence
- [x] Presets (101 de fabrica, agrupados em secoes; presets novos chegam sozinhos, apagados ficam apagados)
- [x] Start/Stop capture na aba Target (o stop deixou de ser um caminho sem volta)
- [x] Icone da aplicacao (assets/OverlayDesk.ico, gerado por scripts/make-icon.ps1)
- [x] Anti-fisheye sem furo transparente (ADR-0010)
- [x] Lens softness - queda de nitidez para os cantos (ADR-0011)
- [x] 12 presets novos de fisheye e anti-fisheye
- [x] Secoes na aba Presets (categoria gravada no arquivo, recuperada pelo nome nos antigos)
- [x] 8 presets novos de entrada tatica
- [x] Beam width (linha larga onde a imagem e clara) e interlace nas scanlines
- [x] Halation (tint do halo do bloom) e ScopeShape tube (vidro de CRT)
- [x] 10 presets novos de CRT (101 de fabrica no total)
- [x] FPS limiter
- [x] Source minimized behavior
- [x] Release profiling (scripts/stability-test.ps1 + crash handler + version resource)
- [~] Leak test (harness pronto; a corrida completa de 60 min do AT-016 ainda nao foi concluida)

## P1 — Pós-MVP

- [ ] Auto-follow target window
- [x] Hotkeys configuráveis (10 acoes, rebind por captura de tecla, ADR-0009)
- [ ] Tray icon
- [ ] Startup minimized
- [ ] Multi-overlay
- [ ] Multi-monitor preset
- [x] Noise
- [x] Flicker
- [x] Jitter
- [x] VHS (preset, sobre os modulos existentes)
- [x] Bloom (aproximacao single-pass; ver ADR-0009 secao 2)
- [x] Halation (tint do halo do bloom)
- [x] CRT masks (aperture grille e slot mask, como estilos de scanline)
- [~] LCD subpixel grid (a grade existe via `orientation=grid`; subpixel de verdade nao)
- [ ] Ghosting
- [ ] Import/export presets
- [x] Thermal / false colour (white hot, black hot, ironbow, fosforo, Cross-Com)
- [x] Edge glow (vista de sensor)
- [x] Lens dirt
- [x] Shimmer / refracao (camuflagem optica)
- [x] Rolling shutter
- [x] Scan sweep
- [x] Compressao digital (estilo `digital` do glitch)

## P2 — Futuro

- [ ] Shader plugin architecture
- [ ] Preset sharing
- [x] shader hot reload em dev (existe em Debug; ver MANUAL-TESTS.md secao 17)
- [ ] overlay profiles por executable
- [ ] auto-load preset por emulator
