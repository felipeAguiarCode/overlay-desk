# Overlay Desk

Uma camada visual que fica por cima de um emulador (ou de qualquer janela) e aplica filtros de
CRT, distorção, aberração cromática e glitch em tempo real, na GPU.

Não grava vídeo, não injeta nada no emulador e não modifica o jogo: ele captura a janela,
processa na GPU e desenha o resultado numa janela própria e independente.

Windows 10 1903+ · GPU com Direct3D 11 · zip de ~800 KB · sem instalador e sem Visual C++
Redistributable.

Versão 0.1.0: todas as features do MVP estão implementadas e os testes passam. A única
pendência é fechar a corrida de 60 minutos do teste de estabilidade (`docs/planning/BACKLOG.md`).

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
| `Esc` `Esc` | Fecha o overlay e devolve o painel — duas vezes em menos de meio segundo, e este é fixo |

Se a aba Overlay disser `shortcut unavailable`, outro programa registrou a combinação antes —
troque-a na aba Hotkeys; os botões do painel continuam funcionando.

## Filtros e efeitos

**Filters** — transformam a imagem de forma constante: Distortion (bipolar, de anti-fisheye a
fisheye), Chromatic Aberration, Color Correction, Scanlines, Vignette, Lens Softness, Bloom,
False Colour, Edge Glow, Lens Dirt, Scope.

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
