# Overlay Desk

[![Release](https://img.shields.io/github/v/release/felipeAguiarCode/overlay-desk?style=flat-square&label=release&color=2ea44f)](https://github.com/felipeAguiarCode/overlay-desk/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/felipeAguiarCode/overlay-desk/total?style=flat-square&label=downloads&color=555)](https://github.com/felipeAguiarCode/overlay-desk/releases)
[![Plataforma](https://img.shields.io/badge/plataforma-Windows%2010%201903%2B-0078D6?style=flat-square)](#download)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square)](CMakeLists.txt)
[![Direct3D 11](https://img.shields.io/badge/Direct3D-11-5C2D91?style=flat-square)](docs/adr/ADR-0001-technology-stack.md)
[![Licença MIT](https://img.shields.io/badge/licen%C3%A7a-MIT-blue?style=flat-square)](LICENSE)

Uma camada visual que fica por cima de um emulador (ou de qualquer janela) e aplica filtros de
CRT, distorção, aberração cromática e glitch em tempo real, na GPU.

Não grava vídeo, não injeta nada no emulador e não modifica o jogo: ele captura a janela,
processa na GPU e desenha o resultado numa janela própria e independente.

Windows 10 1903+ · GPU com Direct3D 11 · zip de ~800 KB · sem instalador e sem Visual C++
Redistributable.

Versão 1.1.0: entraram o filtro Sharpen, o preset `GoPro Bodycam` e um aviso que diz quando a
janela de origem não está mandando frame. Todas as features do MVP estão implementadas e os
testes automatizados passam. A corrida de 60 minutos do teste de estabilidade ainda não foi
fechada — acompanhe em `docs/planning/BACKLOG.md`.

## Download

**[Baixe a versão mais recente](https://github.com/felipeAguiarCode/overlay-desk/releases/latest)**
— descompacte em qualquer pasta e execute `OverlayDesk.exe`. Não há instalação, nada é escrito
no registro, e todas as versões ficam em
[Releases](https://github.com/felipeAguiarCode/overlay-desk/releases).

## Uso

1. Abra o emulador, depois o `OverlayDesk.exe`.
2. Aba **Target** → `Refresh list` → escolha a janela do emulador.
3. Aba **Overlay** → `Match Target Window` para o overlay assumir o tamanho e a posição dela.
4. Aba **Presets** → duplo clique em `Soft CRT` ou `Arcade CRT`.

O overlay nasce **click-through**: os cliques atravessam para o jogo, então não há o que
agarrar para movê-lo. Para mexer nele, entre no **Edit Mode** (`Ctrl`+`Shift`+`O`) — borda azul
com 8 alças, arraste o meio para mover e as alças para redimensionar — ou simplesmente desligue
o `Click-through` na aba Overlay.

**`Ctrl`+`Shift`+`Q`** abre um menu rápido no meio do overlay, com `Control panel`,
`Stop overlay` e `Quit`. Com o overlay em foco, `Esc` faz o mesmo — durante o jogo o `Esc` é do
jogo, e o Overlay Desk não o intercepta.

## Atalhos

Globais: funcionam com o jogo em foco. Reconfiguráveis na aba **Hotkeys**.

| Atalho | Ação |
|---|---|
| `Ctrl`+`Shift`+`O` | Edit Mode (mover e redimensionar) |
| `Ctrl`+`Shift`+`0` | Edit Mode — o zero, já que O e 0 se confundem |
| `Ctrl`+`Shift`+`H` | Mostra / esconde o overlay, sem perder o alvo |
| `Ctrl`+`Shift`+`C` | Click-through |
| `Ctrl`+`Shift`+`T` | Always-on-top |
| `Ctrl`+`Shift`+`F` | Fullscreen |
| `Ctrl`+`Shift`+`M` | Encaixa o overlay na janela capturada |
| `Ctrl`+`Shift`+`→` / `←` | Próximo / anterior preset |
| `Ctrl`+`Shift`+`P` | Traz o painel de controle para a frente |
| `Ctrl`+`Shift`+`Q` | Abre o menu rápido do overlay |
| `Esc` | O mesmo menu, quando o overlay tem foco (Edit Mode ou click-through desligado) |

Se a aba Overlay disser `shortcut unavailable`, outro programa registrou a combinação antes —
troque-a na aba Hotkeys; os botões do painel continuam funcionando.

## Filtros e efeitos

**Filters** — transformam a imagem de forma constante: Distortion (bipolar, de anti-fisheye a
fisheye), Chromatic Aberration, Color Correction, Scanlines, Vignette, Lens Softness, Bloom,
Sharpen, False Colour, Edge Glow, Lens Dirt, Scope.

**Effects** — mudam com o tempo, todos procedurais e sem guardar frame anterior: Jitter,
Shimmer, Rolling Shutter, Glitch, Scan Sweep, Noise, Flicker.

Cada módulo tem `ON`/`OFF`, `Intensity` e um `Advanced` recolhível. Desligar não perde os
valores ajustados, e duplo clique em qualquer slider volta ao default. Um preset guarda **só**
filtros e efeitos — nunca a geometria da janela nem o alvo — então o mesmo arquivo funciona em
qualquer máquina.

Configurações, presets e logs ficam em `%APPDATA%\OverlayDesk\`. Apagar essa pasta devolve tudo
ao estado de fábrica.

O **`USAGE.md`** documenta cada parâmetro de cada módulo, as travas do overlay, o ajuste de
desempenho e o que fazer quando algo dá errado.

## Build

Requer Visual Studio Build Tools com o workload C++ e um Windows SDK com a projeção C++/WinRT
(10.0.17763+). CMake e Ninja acompanham o Build Tools; `vcpkg` não é usado.

```powershell
.\scripts\build.ps1 -Config Release -Test
```

Sai em `build\Release\OverlayDesk.exe`. Dear ImGui e nlohmann/json são baixados pelo CMake via
`FetchContent`, com versão e hash fixados. `scripts\package.ps1` produz o zip portátil.

## Documentação

`CLAUDE.md` (requisitos e restrições) · `docs/PRD.md` · `docs/ARCHITECTURE.md` · `docs/adr/`
(decisões arquiteturais) · `docs/specs/` · `docs/planning/BACKLOG.md` (progresso) ·
`docs/ACCEPTANCE-TESTS.md` · `docs/MANUAL-TESTS.md`.

MIT — veja `LICENSE` e `THIRD-PARTY-NOTICES.md`.
