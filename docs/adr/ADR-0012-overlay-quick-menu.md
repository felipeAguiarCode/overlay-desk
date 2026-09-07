# ADR-0012 — Menu rápido do overlay: menu nativo, e o Escape não vira hook global

## Status

Accepted

## Contexto

O overlay não tinha nenhuma saída própria. Para trocar de preset, voltar ao painel ou fechar o
programa, o usuário precisava encontrar a janela de controle — que em jogo fullscreen está
atrás de tudo. O único gesto no overlay era o duplo Escape, que fechava o overlay inteiro: caro
demais para ser a única ação, e invisível para quem não leu o `USAGE.md`.

O pedido era um menu rápido no overlay, no formato do ShaderGlass, com três ações: abrir o
painel de configurações, parar o overlay e sair.

Duas restrições moldaram a decisão.

**A primeira é normativa.** `docs/specs/OVERLAY-WINDOW.md` linha 95: *"Teclado não deve ser
capturado pelo overlay durante Play Mode."* É por isso que o overlay carrega `WS_EX_NOACTIVATE`
fora do Edit Mode. A consequência é direta: **em Play Mode o overlay nunca recebe `WM_KEYDOWN`**.
Enquanto o jogo está em foco, o Escape é do jogo, e nenhuma tecla chega ao overlay.

**A segunda é que o overlay não tem ImGui.** O ImGui vive só no `ControlWindow`. O overlay é um
swap chain com um pixel shader e nada mais — o frame entra pela GPU e sai pela GPU, que é o que
o ADR-0003 e o ADR-0006 estabelecem.

## Decisão

### 1. O menu é nativo, via `TrackPopupMenu`

`CreatePopupMenu` + `TrackPopupMenu` com `TPM_RETURNCMD`, centrado no overlay.

Custo por frame: zero. Nada entra no pipeline de render, nada é alocado enquanto o menu está
fechado, e a prioridade 4 do `CLAUDE.md` (baixo uso de GPU) fica intacta. O Windows já resolve
DPI, navegação por teclado, tema e o descarte ao clicar fora — tudo que uma barra desenhada em
ImGui teria de reimplementar.

`TPM_RETURNCMD` devolve o id escolhido em vez de postar `WM_COMMAND`, então o `WindowProc` do
overlay não ganha nenhum caso novo e a chamada é uma função síncrona comum.

### 2. Escape abre o menu, e continua sem hook global

O Escape abre o menu **quando o overlay tem foco** — Edit Mode, ou click-through desligado.
Isso não é uma limitação contornável: é a linha 95 da spec sendo respeitada.

Para o Play Mode, uma ação nova no sistema de atalhos que já existe: `showQuickMenu`, com
`Ctrl`+`Shift`+`Q` de fábrica, reconfigurável como qualquer outra.

### 3. Click-through e no-activate são suspensos enquanto o menu está aberto

Um menu que não se pode clicar não é um menu. `WS_EX_TRANSPARENT` e `WS_EX_NOACTIVATE` saem
antes do `TrackPopupMenu` e o `UpdateStyles()` os repõe ao fim, a partir do que as settings
dizem — não de um valor guardado à parte, que é como esse tipo de suspensão costuma vazar.

O `SetForegroundWindow` antes do menu é o que faz ele fechar ao clicar fora. Um processo que
está tratando um hotkey tem direito ao foreground, que é exatamente o caso do Play Mode.

### 4. O duplo Escape deixa de existir

O menu o substitui inteiro: `Stop overlay` faz o que ele fazia, e agora é visível em vez de
folclore. Manter os dois seria impossível de qualquer forma — o primeiro Escape abre o menu, e
o segundo é consumido pelo próprio menu.

## Consequências

- O overlay ganha um estado transitório novo, `quick menu open`, em que ele aceita mouse e
  foco mesmo em Play Mode. É reentrante-safe por uma flag: Escape e o atalho podem chegar os
  dois com o menu já aberto.
- `TrackPopupMenu` é modal. O loop principal fica parado enquanto o menu está na tela, então o
  overlay não repinta nesse intervalo. A captura roda em thread própria (ADR-0006) e continua
  entregando frames normalmente; o que se perde é a repintura de alguns frames durante uma
  interação de segundos.
- O menu é o primeiro elemento de UI fora do `ControlWindow`. Se um dia ele crescer para além
  de três itens, a decisão de não trazer ImGui para o overlay merece ser revisitada.
- `HotkeyAction` ganhou um valor. O enum é indexado por posição, então ele foi **acrescentado
  ao fim**, antes de `Count`, como o próprio header exige. Bindings em disco são casados pela
  string `showQuickMenu`, e uma entrada ausente cai no default — um `settings.json` da 0.1.0
  ganha o atalho novo sem migração.

## Alternativas descartadas

**Hook `WH_KEYBOARD_LL` para capturar Escape globalmente.** Faria o Escape funcionar durante o
jogo, que é o cenário principal. Descartado por três razões: viola a linha 95 da spec; o Escape
é a tecla que mais se usa em jogo, então ou o menu abriria junto com o menu de pausa ou
engoliria o Escape e quebraria o jogo; e hooks globais de teclado são rotineiramente sinalizados
por anti-cheat, o que transformaria um overlay visual em algo que parece um cheat.

**Barra em ImGui desenhada no swap chain do overlay.** Visual coerente com o painel, mas traz o
ImGui para o renderer do overlay e custa por frame enquanto aberta. Para três itens, paga-se
uma dependência nova no caminho crítico de render para reimplementar o que o `TrackPopupMenu`
já faz.

**Menu bar permanente no topo do overlay**, como o ShaderGlass tem. O ShaderGlass mostra uma
janela com barra de título; este overlay é sem moldura por decisão do ADR-0002, e uma barra
permanente comeria área da imagem capturada e apareceria em qualquer captura de tela ou
gravação do usuário.
