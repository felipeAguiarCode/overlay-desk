# Overlay Desk — Guia de Uso

Uma camada visual que fica por cima de um emulador (ou de qualquer janela) e aplica filtros
de CRT, distorção, aberração cromática e glitch em tempo real.

Não é um gravador, não injeta nada no emulador e não modifica o jogo. Ele captura a janela,
processa na GPU e desenha o resultado numa janela própria.

## Instalação

Não tem instalação. Descompacte e execute `OverlayDesk.exe`.

Requer Windows 10 versão 1903 ou mais recente (a captura de janela depende disso) e uma GPU
com Direct3D 11. Não precisa do Visual C++ Redistributable.

Suas configurações, presets e logs ficam em `%APPDATA%\OverlayDesk\`. Apagar essa pasta
devolve tudo ao estado de fábrica.

## Primeiro uso

1. Abra o emulador.
2. Abra o Overlay Desk.
3. Na aba **Target**, clique em `Refresh list` e escolha a janela do emulador.
   `Stop capture` interrompe e `Start capture` retoma na mesma janela, sem precisar procurá-la
   na lista de novo.
4. O overlay aparece mostrando a captura. Na aba **Overlay**, use `Match Target Window` para
   ele assumir exatamente o tamanho e a posição do emulador.
5. Na aba **Presets**, experimente `Soft CRT` ou `Arcade CRT` (duplo clique aplica).

## Posicionando o overlay

O overlay é uma janela independente — ele não segue o emulador automaticamente.

Por padrão ele é **click-through**: seus cliques atravessam e chegam no jogo, e por isso ele
não pode ser arrastado — não há o que agarrar. Há duas formas de mexer nele:

**1. Desligue o `Click-through`** na aba Overlay. A partir daí basta arrastar: clique no meio
para mover, nas bordas ou cantos para redimensionar. Não precisa entrar em modo nenhum.

**2. Entre no Edit Mode** — **`Ctrl` + `Shift` + `O`** (a letra O) ou **`Ctrl` + `Shift` + `0`**
(o zero); as duas funcionam. Também há o botão `Edit Overlay` na aba Overlay.

O atalho é um interruptor: aperta uma vez e o overlay fica arrastável e redimensionável, com
a borda e as alças à vista; aperta de novo e ele volta ao click-through. Diferente da opção 1,
isso não mexe na sua configuração — o `Click-through` continua como estava.

A aba Overlay mostra qual atalho está ativo. Se outro programa já tiver registrado os dois, ela
avisa `shortcut unavailable` e aí só o botão funciona — nesse caso vale trocar a combinação na
aba **Hotkeys**, onde todos os atalhos são reconfiguráveis.

No Edit Mode aparece uma borda azul pulsante com 8 alças. Arraste o meio para mover, as alças
para redimensionar. Saia com o mesmo atalho e o click-through volta.

Se o atalho não funcionar, outro programa já o registrou — o botão continua funcionando.

### O menu rápido

**`Esc`** abre um menu no meio do overlay com três saídas:

| Item | O que faz |
|---|---|
| `Control panel` | Traz o painel de configurações para a frente, sem mexer no overlay |
| `Stop overlay` | Desliga o overlay e devolve o painel, já na aba Overlay |
| `Quit` | Fecha o Overlay Desk |

`Esc` de novo, ou um clique fora, fecha o menu sem fazer nada.

**Quando o `Esc` funciona:** ele só chega ao overlay quando o overlay tem foco — em Edit Mode,
ou com o `Click-through` desligado. Enquanto você joga, o foco é do jogo, e o `Esc` que você
aperta é do jogo: o Overlay Desk nunca vê essa tecla, de propósito. Um programa que
interceptasse o `Esc` do sistema inteiro roubaria o menu de pausa de todo jogo que você abrisse.

**Por isso existe `Ctrl` + `Shift` + `Q`**, que abre o mesmo menu de qualquer lugar, com o jogo
em foco. É esse o caminho durante o Play Mode, e ele é reconfigurável na aba Hotkeys como
qualquer outro atalho.

Enquanto o menu está aberto o overlay aceita cliques mesmo em modo click-through — senão não
haveria como escolher um item. Ao fechar, tudo volta exatamente como estava.

### Travas

| Opção | Efeito |
|---|---|
| `Resizable` | Permite redimensionar |
| `Lock position` | Trava a posição, mesmo no Edit Mode |
| `Lock size` | Trava o tamanho |
| `Lock aspect ratio` | Mantém a proporção escolhida ao redimensionar |
| `Always on top` | Mantém o overlay acima das outras janelas |
| `Fullscreen` | Ocupa um monitor inteiro (você escolhe qual) |

## Os filtros

Aba **Filters**. Cada módulo tem um botão `ON`/`OFF`, um `Intensity` e um `Advanced`
recolhível com os parâmetros específicos. Duplo clique em qualquer slider volta ao default.

Desligar um filtro **não** perde os valores que você ajustou.

- **Distortion** — deformação óptica. O controle `Amount` é bipolar: negativo é anti-fisheye
  (a imagem beliscada para dentro), zero é neutro, positivo é fisheye (o barrigudo de TV de
  tubo).
  No lado **positivo** os cantos ficam ancorados e nada é cortado. No lado **negativo** a imagem
  é levemente aproximada e os cantos saem de vista: um pinch puxa as bordas para dentro, e o que
  ocuparia o lugar delas está fora do que foi capturado. Em `-1.00` você vê os dois terços
  centrais. Nas duas direções o quadro fica sempre cheio — nenhuma quantidade abre buraco.
  O dropdown `Shape` escolhe **como** ela deforma: `Radial` é uma lente esférica, `CRT` dobra
  cada eixo separadamente como um tubo de verdade, `Cylindrical` e `Vertical` deformam só um
  eixo, e `Corner only` deixa o centro plano e puxa apenas os cantos. O `Amount` bipolar vale
  para todas, então cada forma traz junto o seu próprio anti-fisheye.
- **Chromatic Aberration** — separa os canais de cor, como uma lente barata. O modo `Edge`
  deixa o centro limpo e concentra a franja na borda, que é o que mais parece um CRT real.
  `Prism` manda cada canal para um ângulo diferente, e `Barrel` faz a separação crescer com o
  quadrado da distância, que é como uma lente desfoca de verdade.
- **Color Correction** — brilho, contraste, saturação e gamma. Nos valores neutros a imagem
  passa intacta.
  Em `Advanced` há também **`Tint`**: a imagem é multiplicada pela cor escolhida, então o preto
  continua preto e só o que estava aceso recebe a cor — é assim que um fósforo ou o corante de
  um LCD se comportam. Para um monocromático de verdade, baixe `Saturation` a 0 **antes** de
  subir o `Tint amount`. É o que faz o verde do Game Boy e o vermelho do Virtual Boy.
- **Scanlines** — as linhas horizontais. O dropdown `Style` vai de `Hard` (onda quadrada) a
  `Soft` (queda senoidal), `Sharp` (linhas finas com brilho entre elas), `Aperture grille`
  (tríades RGB verticais, estilo Trinitron) e `Slot mask` (as tríades escalonadas da máscara de
  sombra clássica). As duas últimas colorem por canal em vez de escurecer, então escurecem
  menos na mesma intensidade.
  `Pixel perfect` mede o espaçamento em pixels do overlay; `Relative to source` mede em pixels
  da fonte, então um jogo 240p mantém 240 linhas por mais que você amplie o overlay.
  Em `Advanced`, **`Beam width`** é o controle que mais muda o resultado: num tubo o feixe se
  espalha quando é mais excitado, então o que está claro tem linhas gordas que se fecham e o
  que está escuro mantém as finas com preto entre elas. Em 0% o padrão é uniforme e lê como
  listras coladas por cima; suba um pouco e ele passa a ler como tela.
  **`Interlace`** alterna o padrão meia linha a cada campo, como o 480i é desenhado — só
  aparece em movimento, uma imagem parada não mostra.
- **Vignette** — escurece as bordas. `Roundness` vai do retângulo do overlay a uma elipse.
- **Lens Softness** — uma lente é nítida no meio e se desfaz nos cantos, e quanto mais aberta,
  mais cedo isso começa. `Sharp zone` é até onde a parte nítida chega; além dela o amolecimento
  cresce rápido. É o que faz um fisheye parecer vidro em vez de um screenshot deformado — vale
  ligar junto com o Distortion sempre que a intenção for uma lente e não uma tela.
- **Bloom** — o florescimento de um sensor ou de um tubo estourado: a luz forte espalha e come
  o detalhe em volta. Só o que passa do `Threshold` floresce, e o halo mantém a cor da luz que
  o produziu — um clarão vermelho não vira um halo branco. É um *glow* montado com doze
  amostras, não um desfoque de verdade: um desfoque exigiria uma segunda passada de render que
  o orçamento de memória não comporta, então no `Radius` máximo o padrão da amostragem aparece.
  Em `Advanced`, **`Halo colour`** multiplica o halo. Branco deixa a cor da luz que o produziu;
  esquentar é como se chega à **halation** — a luz que espalha dentro do vidro de um tubo volta
  avermelhada, e é por isso que um realce branco num CRT tem borda quente.
- **Sharpen** — o crunch que uma câmera de ação aplica na própria imagem. Um sensor pequeno
  atrás de uma lente muito aberta resolve mal, e o processador responde com um unsharp mask
  agressivo, forte o bastante para o halo em volta de uma borda de alto contraste aparecer.
  `Radius` é até onde esse halo chega. Ligue junto com o Lens Softness: cantos moles e um meio
  afiado é exatamente o que esse tipo de câmera produz. As bordas saem da imagem capturada, não
  do resultado já processado, então o grão e as scanlines nunca são afiados junto.
- **False Colour** — mapeia o brilho para uma paleta, que é o que uma mira térmica realmente
  faz. `White hot`, `Black hot` (quente lê escuro, mais fácil de achar uma silhueta),
  `Ironbow` (preto → roxo → vermelho → laranja → branco), `Phosphor green` e `White phosphor`
  dos tubos de visão noturna, e `Cross-Com cyan` de um HUD de sensor. Baixe a `Saturation` da
  Color Correction a 0 **antes**, para a paleta ler brilho e não um resto de cor. `Levels` no
  `Advanced` quantiza a rampa, como o display de poucos bits de um equipamento de campo.
- **Edge Glow** — contorna o que está na imagem com uma linha brilhante, a vista de sensor de
  jogo tático. As bordas saem da imagem capturada, então scanlines e grão nunca são
  confundidos com contorno.
- **Lens Dirt** — gordura e respingo no vidro da frente. **Não se move**, de propósito: sujeira
  em lente fica onde está, e animá-la viraria chuva no para-brisa.
- **Scope** — o recorte de uma óptica. `Aperture` é o tamanho do vidro; tudo fora dele é o
  corpo do óptico, desenhado como preto **sólido** e não como um desvanecimento — é essa a
  diferença para o vignette, que deixaria o desktop aparecer onde deveria haver um tubo de aço.
  `Magnification` amplia antes da trepidação, então o `Jitter` é ampliado junto, como acontece
  num óptico de alta potência. `Shape` escolhe entre um círculo, o par sobreposto de um
  binóculo, `Tube (CRT faceplate)` — o retângulo de cantos arredondados de uma tela de tubo, que
  corta os cantos da imagem em preto sólido — e `Quad tube (NVG)`, os quatro círculos de um
  goggle panorâmico, que mostram uma
  cena contínua através de quatro aberturas em vez de quatro vigias separadas. `Reticle`
  desenha uma cruz duplex com marcas de retenção.
  A curvatura da luneta vem do **Distortion** — os dois são controles separados de propósito,
  para dar para abrir a abertura sem achatar a imagem.

A ordem em que são aplicados é fixa e aparece na própria aba, de cima para baixo.

## Os efeitos

Aba **Effects**. Ao contrário dos filtros, todos têm uma velocidade ou uma frequência: eles
mudam com o tempo em vez de transformar a imagem de forma constante. Nenhum guarda frame
anterior — são todos procedurais, e o mesmo instante sempre produz o mesmo resultado.

- **Jitter** — o quadro inteiro vagueia, como um sinal instável ou uma câmera na mão. Em
  intensidade alta as bordas do frame entram em vista conforme a imagem sai delas; isso é a
  trepidação sendo honesta, não um defeito.
- **Shimmer** — a imagem ondula, como através de ar quente ou atrás de um campo de camuflagem
  óptica. Diferente do `Jitter`, que move o quadro inteiro rígido, este **dobra** a imagem.
  `Scale` baixo é a fervura fina logo acima de metal quente; alto é uma ondulação lenta e
  larga.
- **Rolling Shutter** — um sensor barato lê a imagem linha por linha. Mova a câmera durante
  essa leitura e as linhas param de se alinhar: a imagem inclina. Ela pivota em torno da linha
  do meio — deslocar todas as linhas por igual seria o `Jitter`.
- **Glitch** — as rajadas de sinal quebrado. O `Style` escolhe **como** ele quebra: `Analog`
  rasga a imagem em bandas deslocadas e separa os canais, do jeito que fita e sinal de RF
  falham; `Digital` perde macroblocos inteiros — dentro do bloco a imagem fica intacta, que é
  como um downlink comprimido de drone se despedaça.
  Dois controles independentes definem o resto: **`Frequency`** é com que frequência uma
  rajada acontece, **`Intensity`** é quanto ela distorce quando acontece. Frequência em 0%
  nunca dispara, por mais alta que esteja a intensidade. Em 15% de intensidade a imagem fica
  instável mas legível; em 55% fica claramente danificada. `Block size` define a altura das
  bandas em `Analog` e a aresta do macrobloco em `Digital`.
- **Scan Sweep** — uma barra de luz atravessando a imagem, como um sensor varrendo. Ela
  acende no instante em que chega e desvanece atrás de si, que é o que um feixe faz — uma
  barra simétrica leria como uma listra andando.
- **Noise** — grão de sensor. `Speed` é a frequência com que o padrão é **re-sorteado**, não a
  velocidade de um movimento: ele fica parado entre os sorteios, como grão de filme, em vez de
  rastejar. `Colour` em 0% é grão monocromático; em 100% cada canal ganha o seu salpico.
- **Flicker** — a imagem inteira respira, como um tubo ou a lâmpada de um projetor. Duas taxas
  sem relação são misturadas para a pulsação nunca virar um loop reconhecível, e a oscilação é
  limitada a metade do slider: a faixa inteira seria um estrobo.

## Presets

Aba **Presets**. Um preset guarda **apenas** filtros e efeitos — nunca a geometria da janela,
o alvo escolhido ou as configurações de FPS. Por isso o mesmo arquivo funciona em qualquer
máquina.

A linha **Current look** mostra o que está na tela agora: o nome do preset, `Nome (edited)`
se você mexeu em algo depois de aplicá-lo, ou `Custom` se não bate com nenhum.

| Botão | O que faz |
|---|---|
| `Apply` | Aplica o selecionado (duplo clique na lista faz o mesmo) |
| `Save` | Sobrescreve o selecionado com o look atual |
| `Save As` | Cria um novo com o nome digitado |
| `Duplicate` | Copia o selecionado |
| `Rename` | Renomeia |
| `Delete` | Apaga, com confirmação |
| `New` | Zera tudo para os defaults, sem gravar nada |

Os presets que vêm de fábrica são marcados como `built-in`, mas você pode editá-los e
apagá-los como qualquer outro. **Apagar é definitivo** — eles não voltam na próxima abertura.
Presets novos que uma versão futura trouxer aparecem sozinhos; os que você apagou continuam
apagados.

### Seções

A lista é agrupada em seções recolhíveis, na mesma ordem em que aparecem abaixo. Clique no
cabeçalho para dobrar uma seção inteira — com 101 presets de fábrica, é o que torna a lista
navegável. O número ao lado do nome é quantos presets há ali.

A seção fica gravada no arquivo do preset, então renomear um preset não o tira do grupo dele.
Presets que você criar entram em **`Custom`**, que aparece por último.

| Seção | Presets |
|---|---|
| Base | `Neutral` |
| CRT | `Soft CRT`, `Arcade CRT`, `Curved CRT`, `Soft Scanlines`, `Trinitron`, `Shadow Mask`, `PVM`, `Consumer TV`, `Halation`, `Interlaced`, `Tube Bezel`, `Vector Monitor`, `Green Terminal`, `Amber Terminal`, `Portable TV`, `Projection TV` |
| Consoles | `Game Boy`, `Game Boy Advance`, `SNES`, `Mega Drive`, `PlayStation`, `NES`, `Nintendo 64`, `Neo Geo`, `PC Engine`, `Virtual Boy` |
| Fisheye | `Fisheye Wide`, `Fisheye Extreme`, `Fisheye CRT`, `Peephole`, `Security Cam`, `Circular Fisheye`, `Fisheye Soft`, `Dashcam`, `Drone FPV`, `Skate Cam`, `Bubble Lens`, `Bug Eye`, `Cockpit Glass`, `Panoramic`, `Doorbell Cam` |
| Anti-fisheye | `Anti-Fisheye Light`, `Anti-Fisheye Strong`, `Lens Correction`, `Anti-Fisheye Horizontal`, `Anti-Fisheye Corners`, `Ultrawide Correct`, `Pincushion Tube` |
| Optics | `Sniper Scope`, `Scope Cam`, `Spotter Scope`, `Binoculars`, `Red Dot` |
| Tactical | `Bodycam`, `Helmet Cam`, `Night Vision`, `Thermal`, `Breach`, `Chest Cam`, `Entry Team`, `Night Ops`, `White Phosphor`, `Flashbang`, `Evidence Cam`, `Shield Cam`, `Tac Light`, `Gas Mask`, `CS Gas`, `Suspect Cam`, `Under Door Cam`, `Stack Up`, `Concussion`, `IR Illuminator`, `Dispatch Feed`, `Interview Room`, `Taser Arc` |
| Body-worn | `Action Cam`, `Duty Cam`, `Low Light Sensor`, `Cheap Sensor`, `Dirty Lens`, `Night Patrol` |
| Recon and drone | `Drone Feed`, `UAV Thermal`, `Black Hot`, `Ironbow`, `Recon Optic`, `Azure Recon`, `Sync Shot` |
| Sensor | `Cross-Com`, `Magnetic View`, `Optical Camo`, `Warhound Feed`, `EMP Burst`, `Ghost Mode` |
| Effect | `VHS`, `Projector`, `Film Grain`, `Prism Lens`, `Broken Signal` |

### De onde vieram

Os nomes descrevem o *look*, não o jogo, porque o mesmo preset serve para qualquer coisa que o
overlay esteja capturando. Se você chegou aqui procurando um jogo específico:

| Se você quer o clima de | Comece por |
|---|---|
| *Ready or Not* e afins | `Chest Cam`, `Entry Team`, `Stack Up`, `Night Ops`, `White Phosphor`, `IR Illuminator`, `Tac Light`, `Flashbang`, `Concussion`, `Gas Mask`, `CS Gas`, `Shield Cam`, `Under Door Cam`, `Taser Arc`, `Red Dot`, `Dispatch Feed`, `Interview Room` |
| *Bodycam* e o gênero de filmagem corporal | `Action Cam`, `Duty Cam`, `Low Light Sensor`, `Cheap Sensor`, `Night Patrol` |
| *Ghost Recon* (Wildlands / Breakpoint) | `Drone Feed`, `UAV Thermal`, `Black Hot`, `Recon Optic`, `Azure Recon`, `Sync Shot`, `Ghost Mode` |
| *Ghost Recon: Future Soldier* | `Cross-Com`, `Magnetic View`, `Optical Camo`, `Warhound Feed`, `EMP Burst` |

Nenhum deles lê o jogo. São looks aplicados a qualquer janela capturada — o app não sabe nem
se o que está na tela é um jogo.

## Atalhos

Aba **Hotkeys**. São atalhos **globais**: funcionam com o jogo em foco, que é justamente para
o que servem.

O que vem configurado:

| Atalho | O que faz |
|---|---|
| `Ctrl` + `Shift` + `O` | Entra e sai do Edit Mode |
| `Ctrl` + `Shift` + `0` | O mesmo, com o zero — a letra O e o dígito 0 se confundem, então os dois valem |
| `Ctrl` + `Shift` + `H` | Liga e desliga o overlay, sem perder o alvo |
| `Ctrl` + `Shift` + `C` | Liga e desliga o click-through |
| `Ctrl` + `Shift` + `T` | Liga e desliga o always-on-top |
| `Ctrl` + `Shift` + `F` | Fullscreen |
| `Ctrl` + `Shift` + `M` | Encaixa o overlay na janela capturada |
| `Ctrl` + `Shift` + `→` | Próximo preset |
| `Ctrl` + `Shift` + `←` | Preset anterior |
| `Ctrl` + `Shift` + `P` | Traz o painel de volta para a frente |
| `Ctrl` + `Shift` + `Q` | Abre o menu rápido do overlay |

Para trocar um atalho, clique no botão que mostra a combinação atual e **aperte a nova**.
`Esc` cancela. `Clear` deixa a ação sem atalho, `Default` devolve o original.

Todo atalho precisa de pelo menos um modificador (`Ctrl`, `Shift`, `Alt` ou `Win`). Uma tecla
sozinha registrada globalmente pararia de chegar em todos os outros programas do computador.

Se a linha disser **`unavailable`**, outro programa registrou aquela combinação antes. Escolha
outra, ou feche o programa que a está segurando — a ação continua acessível pelas abas do
painel de qualquer jeito. Se duas ações ficarem na mesma combinação, a aba avisa: o Windows
entrega a tecla para a que registrou primeiro.

O interruptor **`Global shortcuts enabled`** solta todas as combinações de uma vez, sem
esquecer o que estava configurado.

## Desempenho

Aba **Settings**.

- **FPS mode** — `Match source` acompanha o ritmo do emulador, limitado pelo cap operacional.
  `30 FPS cap` e `60 FPS cap` descartam frames antes de qualquer trabalho de GPU, então
  limitar realmente custa menos.
- **Pause while the source is minimized** — minimizou o emulador, o overlay para de trabalhar.

Consumo típico: entre 55 e 75 MB de RAM com a captura ativa.

Se o emulador estiver pausado, o overlay mantém o último quadro na tela em vez de apagá-lo.

## Quando algo dá errado

O log fica em `%APPDATA%\OverlayDesk\logs\overlaydesk.log`.

| Sintoma | O que acontece |
|---|---|
| A janela do emulador não aparece na lista | Ela precisa estar visível e ter título. `Refresh list` recarrega |
| Fechei o emulador | O overlay some, o aplicativo continua aberto, é só escolher outro alvo |
| O overlay não aparece | Verifique se `Overlay enabled` está ligado e se há um alvo selecionado |
| Configuração corrompida | O aplicativo abre com os defaults e registra o erro no log |

Se o programa fechar sozinho, ele grava `crash.log` e um `.dmp` na mesma pasta de logs.

## O que ele não faz

Gravar vídeo, transmitir, capturar áudio, injetar DLL no emulador ou modificar o jogo de
qualquer forma. Ele só lê a imagem da janela e desenha por cima.
