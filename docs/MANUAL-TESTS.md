# Testes Manuais — Milestones 0 a 8

Roteiro de verificação para as Fases 1 a 4 do `CLAUDE.md`. Cada item aponta o teste de
aceite correspondente em `ACCEPTANCE-TESTS.md`.

A Definition of Done exige validação em **Release**. Debug não é referência de consumo.

## Build

```powershell
.\scripts\build.ps1 -Config Release -Test
```

O script localiza o CMake que acompanha o Visual Studio Build Tools (ele não fica no PATH),
configura o preset `vs`, compila e roda o CTest.

Critério: zero warning com `/W4`, em Debug e em Release.

## Preparação

1. Abra uma janela que repinte continuamente (um emulador; para um teste rápido, qualquer
   janela serve).
2. Execute `build\Release\OverlayDesk.exe`.
3. O log fica em `%APPDATA%\OverlayDesk\logs\overlaydesk.log`. Ele pode ser lido com a
   aplicação aberta, desde que o leitor peça compartilhamento de leitura **e** escrita
   (`Get-Content` sozinho falha; use `[IO.File]::Open($p,'Open','Read','ReadWrite')`).

---

## 1. Startup — AT-001

| Passo | Esperado |
|---|---|
| Abrir a aplicação sem target válido | O painel de controle aparece; **nenhum overlay é criado** |
| Olhar a barra de título, o Alt+Tab e a barra de tarefas | Mostram o ícone do aplicativo, não o ícone genérico do Windows |
| Olhar o `.exe` no Explorer | Mesmo ícone (o recurso é o id 1, que é o que o shell escolhe) |
| Ler o log | `Startup`, `D3D11: device on ...`, `Control: panel window created`, `Capture: subsystem initialised` |

## 2. Seleção de target — AT-002

| Passo | Esperado |
|---|---|
| Aba **Target** → `Refresh list` | Lista as janelas capturáveis, com executável e dimensões |
| Clicar em uma janela | Captura inicia **sem reiniciar a aplicação**; a barra de status mostra `Capturing`, a resolução da fonte e o FPS |
| `Stop capture` | A captura para; o botão fica desabilitado e `Start capture` habilita |
| `Start capture` depois de um stop | Retoma na mesma janela, sem precisar achá-la na lista de novo |
| Fechar a janela alvo e clicar `Start capture` | Não retoma; a barra de status diz que a janela não está mais aberta e a lista é recarregada |
| `Start capture` numa instalação nova, sem alvo anterior | Fica desabilitado |
| Ler o log | `Capture: started on '<título>' (LxA)`, `Capture: first frame arrived`, `first frame rendered` |

Janelas próprias da aplicação nunca aparecem na lista (evita o espelho infinito).

## 3. Overlay independente e always-on-top — AT-003, AT-004

| Passo | Esperado |
|---|---|
| Observar o overlay | Janela independente exibindo a fonte, com aspect-fit; as barras de letterbox são **transparentes** |
| Capturar uma fonte com proporção diferente do overlay (ex.: uma janela larga num overlay quadrado) | A imagem é **encaixada**, não esticada; sobram barras transparentes em cima e embaixo |
| Trazer outra janela para frente | Com `Always on top` ligado, o overlay permanece acima |
| Desligar `Always on top` | O overlay passa a se comportar como janela normal |

O aspect-fit e a transparência das barras são cobertos pelo teste automatizado
`shader.pipeline`, que renderiza o pixel shader compilado fora da tela e lê os pixels de volta:
uma fonte 4:1 num overlay quadrado tem de produzir alpha 0 nas barras e 255 na faixa central.
Vale a pena saber por que esse teste existe — o mapeamento de aspect-fit era calculado,
enviado ao shader e **nunca aplicado**, então a fonte era esticada para preencher o overlay e
as barras nunca existiam. Nada na verificação visual pegava isso quando as proporções eram
parecidas.

Verificação dos estilos (a janela deve ter `WS_EX_NOREDIRECTIONBITMAP` e **não** ter
`WS_EX_LAYERED`, conforme ADR-0006):

```powershell
# -20 = GWL_EXSTYLE. 0x00200000 = NOREDIRECTIONBITMAP, 0x00080000 = LAYERED
```

## 4. Click-through — AT-005

| Passo | Esperado |
|---|---|
| Com `Click-through` ligado, clicar sobre o overlay | O clique atinge a aplicação abaixo |
| Enviar `WM_NCHITTEST` (0x0084) ao overlay | Retorna `HTTRANSPARENT` (-1) |

`WindowFromPoint` **não** serve para esta verificação: ela não emula o hit-testing que o
sistema usa para input de mouse.

## 4b. Mover e redimensionar sem Edit Mode

Uma janela sem borda que aceita o mouse mas não pode ser arrastada é inerte — não há moldura
para agarrar. Por isso as regiões de arraste são as mesmas em Edit Mode e em Play Mode com
click-through desligado; o Edit Mode difere por desenhar a borda e por suspender o
click-through, não por ser o único jeito de mover a janela.

| Estado | `WM_NCHITTEST` no centro | Arrastar |
|---|---|---|
| Play Mode, click-through **ON** | `HTTRANSPARENT` (-1) | Não — o clique vai para baixo |
| Play Mode, click-through **OFF** | `HTCAPTION` (2) | Sim, move |
| Edit Mode | `HTCAPTION` (2) | Sim, move |

Nas bordas e cantos, os dois últimos casos devolvem `HTLEFT`/`HTRIGHT`/`HTBOTTOMRIGHT` etc.,
respeitando `Lock position`, `Lock size` e `Fullscreen`.

## 5. Edit Mode — AT-006

Atalho: `Ctrl+Shift+O` (letra) **ou** `Ctrl+Shift+0` (zero) — os dois são registrados, porque
são indistinguíveis por escrito e fáceis de confundir no teclado. Também há o botão
`Edit Overlay`. A aba Overlay mostra qual foi aceito pelo sistema, ou avisa quando nenhum foi.
Os dois são apenas os defaults desde o ADR-0009 — são reconfiguráveis na aba Hotkeys, coberta
pela seção 18.

O atalho alterna: entra no Edit Mode e sai dele, restaurando o click-through do Play Mode.
Verificado com teclas reais — `WM_NCHITTEST` no centro do overlay vai de `HTTRANSPARENT` para
`HTCAPTION` e volta, e `WS_EX_TRANSPARENT` acompanha.

| Passo | Esperado |
|---|---|
| Entrar em Edit Mode | Borda ciano pulsante + 8 handles de resize (4 cantos, 4 meios de aresta); a imagem capturada continua visível por baixo |
| Arrastar o meio do overlay | A janela se move (`WM_NCHITTEST` → `HTCAPTION`) |
| Arrastar um canto | A janela redimensiona (`HTBOTTOMRIGHT` etc.) |
| Sair do Edit Mode | Borda some e o click-through do Play Mode é restaurado |
| `Esc` duas vezes em menos de 600 ms | Overlay fecha e o painel volta ao topo, na aba Overlay |
| `Esc` uma vez só | Nada acontece |
| `Esc` duas vezes com o cursor num campo de texto do painel | Nada acontece (o `Esc` cancela o campo) |

A borda também aparece sobre uma **fonte estática** (um emulador pausado), porque o
repaint reusa a view do último frame — ver ADR-0006 item 6.

## 6. Resize e locks — AT-007, RF-005

| Passo | Esperado |
|---|---|
| Redimensionar repetidamente durante a captura | Sem crash, sem artefato permanente; a imagem continua com aspect-fit correto |
| **Arrastar** uma aresta devagar, olhando o overlay durante o arraste | A imagem acompanha o novo tamanho a cada quadro. Se ela ficar esticada ou presa no tamanho antigo até soltar o mouse, a swap chain voltou a ser redimensionada fora do loop modal de arraste |
| `Lock aspect ratio` ligado + arrastar aresta vertical | A altura acompanha a razão escolhida |
| `Lock position` | O overlay não se move, nem pelo mouse |
| `Lock size` | O overlay não redimensiona |
| `Match Target Window` | O overlay assume os bounds visíveis do alvo (sem a sombra invisível do DWM) |

## 7. Fullscreen e multi-monitor — RNF-008, AT-017

| Passo | Esperado |
|---|---|
| Ligar `Fullscreen` e escolher o monitor | O overlay ocupa o monitor escolhido, borderless |
| Arrastar o overlay para um monitor com DPI diferente | Render, resize, click-through e escala permanecem coerentes; o log registra `Overlay: dpi changed to N` |

## 8. Fonte minimizada — RNF-004, AT-015

| Passo | Esperado |
|---|---|
| Minimizar a janela de origem | Status vira `Paused`; o log registra `Tracker: target minimized`; CPU e GPU caem a praticamente zero |
| Restaurar | `Tracker: target restored`, captura volta |

## 9. Target fechado — AT-014, RNF-009

| Passo | Esperado |
|---|---|
| Fechar a janela de origem | A aplicação **continua aberta**; overlay é destruído; status vira `Target closed` |
| Ler o log | `capture item closed by the system` → `Capture: stopped` → `target closed; returning to idle` |
| Escolher um novo target | Captura reinicia normalmente |

## 10. FPS mode — RNF-003

| Passo | Esperado |
|---|---|
| Aba Settings → `30 FPS cap` | O FPS na barra de status estabiliza perto de 30 |
| `Match source` | O FPS acompanha a fonte, limitado pelo cap operacional |

Frames descartados pelo cap são fechados **antes** de qualquer trabalho de GPU.

## 11. Persistência — RF-018

| Passo | Esperado |
|---|---|
| Ajustar geometria e flags, fechar e reabrir | O estado é restaurado |
| Inspecionar `%APPDATA%\OverlayDesk\settings.json` | Nomes de campo idênticos a `config/settings.example.json` |

## 12. Configuração corrompida — AT-018

Três casos, todos devendo abrir com defaults, registrar o erro e **não** crashar:

| Arquivo | Log esperado |
|---|---|
| JSON sintaticamente inválido | `Settings: ... is not valid JSON; using defaults.` |
| `"schemaVersion": 99` | `Settings: schemaVersion 99 is newer than supported 1; using defaults.` |
| Campo com tipo errado (`"width": "texto"`) | Carrega normalmente; só aquele campo cai no default |

## 13. Filtros — AT-008 a AT-011, gates dos Milestones 4 e 5

Aba **Filters**. Cada módulo é um card com ON/OFF, `Intensity` e um `Advanced` recolhível.
Com o módulo OFF os parâmetros continuam visíveis mas desabilitados (PRD seção 9) e nunca
são perdidos.

Para enxergar os efeitos, use uma fonte com muito contraste e detalhe fino — um bloco de
notas com linhas de `|=|=|=` funciona bem.

### Regra geral — AT-009, AT-010, AT-011

| Passo | Esperado |
|---|---|
| Alternar qualquer módulo com ON/OFF | O overlay atualiza na hora, mesmo com a fonte parada |
| Desligar um módulo | A imagem volta exatamente ao estado anterior |
| Duplo clique em um slider | Restaura o default de `FILTERS-AND-EFFECTS.md` seção 6 |
| `Reset` no card / `Reset all` no topo | Restaura os defaults do módulo / de todos |

### Distortion — AT-008

| amount | Esperado |
|---|---|
| `+1.00` | Fisheye: centro magnificado, linhas retas arqueando para fora, o quadro preenche o overlay |
| `0.00` | **Idêntico** a ter o módulo desligado |
| `-1.00` | Anti-fisheye: imagem beliscada para dentro, bordas curvando ao centro |

A transição precisa ser contínua ao arrastar de -1 a +1, sem salto ao cruzar o zero. Os
cantos ficam ancorados em qualquer valor.

Verificado objetivamente nesta bancada: `amount=0` com o módulo ligado produziu
**0 pixels diferentes em 400.000** contra o módulo desligado.

### Vignette — AT-009

| Passo | Esperado |
|---|---|
| `Intensity` | Controla a força do escurecimento |
| `Size` | Área central preservada |
| `Softness` | Largura da transição |
| `Roundness` 0 → 1 | Forma vai do retângulo do overlay a uma elipse inscrita nele |
| Aplicar distortion junto | A vinheta permanece ancorada às bordas do overlay, não acompanha a deformação |

### Scanlines — AT-010

| Passo | Esperado |
|---|---|
| `Spacing` e `Thickness` | Alteram o padrão sem reiniciar o renderer |
| `Orientation` horizontal / vertical / grid | Linhas horizontais, verticais, ou as duas |
| `Scale` pixel perfect | Espaçamento em pixels do overlay; redimensionar o overlay mantém as linhas do mesmo tamanho |
| `Scale` relative to source | Espaçamento em pixels da fonte; ampliar o overlay amplia as linhas junto |

Com `spacing 4` e `thickness 2` metade dos pixels é escurecida — medido: 49,99%.

### Chromatic Aberration — AT-011

| Passo | Esperado |
|---|---|
| OFF | RGB perfeitamente alinhado |
| ON, modo `Radial` | Canais se separam ao longo do raio, mais forte nas bordas |
| `Horizontal` / `Vertical` | Separação ao longo de um eixo só |
| `Edge` | Centro limpo, separação concentrada na borda |
| `Edge bias` | Empurra a separação para a borda |
| `Red shift` / `Blue shift` | Invertem ou exageram cada canal independentemente |

O verde nunca se desloca — é ele que define onde o quadro está.

### Color Correction — gate do Milestone 5

| Passo | Esperado |
|---|---|
| Valores neutros (brightness 0, contrast 1, saturation 1, gamma 1) | Imagem **idêntica** à original, mesmo com o módulo ligado |
| `Saturation` 0 | Preto e branco |
| `Gamma`, `Contrast`, `Brightness` | Respondem no sentido esperado |
| `Intensity` | Mistura entre a imagem original e a corrigida |

Verificado objetivamente: módulo ligado com valores neutros produziu **0 pixels diferentes
em 400.000** contra o módulo desligado.

### Tint — ADR-0007

| Passo | Esperado |
|---|---|
| `Tint amount` 0%, com uma cor qualquer escolhida | Imagem **idêntica** à original |
| Cor (0,28 / 1,00 / 0,42) a 100% sobre branco | O branco vira exatamente 71 / 255 / 107 |
| A mesma cor a 50% | O ponto médio: 163 / 255 / 181 |
| `Saturation` 0 e depois `Tint amount` 100% | Monocromático real naquela cor, não um viés |
| Preto na fonte, qualquer tinta | Continua preto — a tinta multiplica, não soma |

Verificado objetivamente contra um alvo branco, medindo a média do centro do overlay:

| Configuração | R | G | B |
|---|---|---|---|
| Passthrough | 255,0 | 255,0 | 255,0 |
| Tinta configurada, `amount` 0% | 255,0 | 255,0 | 255,0 |
| Módulo desligado, tinta 100% configurada | 255,0 | 255,0 | 255,0 |
| Tinta 50% | 163,0 | 255,0 | 181,0 |
| Tinta 100% | 71,0 | 255,0 | 107,0 |

Os valores previstos e os medidos coincidem exatamente, e a média do quadro inteiro mantém as
mesmas razões (0,2785 e 0,4196 contra 0,28 e 0,42 previstos), o que confirma que a operação é
multiplicativa pixel a pixel.

### Scope — ADR-0008

| Passo | Esperado |
|---|---|
| Módulo desligado, com uma abertura apertada configurada | Imagem **idêntica** à original |
| Ligado com `Intensity` 0% | Imagem **idêntica** à original |
| Ligado, `Aperture` 60% | Círculo de imagem no centro, preto **sólido** em volta |
| Passar o mouse sobre a área preta em Play Mode | O clique atinge o que está embaixo (click-through intacto) |
| Área preta sobre uma barra de letterbox | Continua preta, e **não** deixa o desktop aparecer |
| `Shape` = `Binocular` | Dois círculos sobrepostos, mais vidro que um círculo do mesmo raio |
| `Magnification` 2x com `Jitter` ligado | A trepidação é ampliada junto com a imagem |
| `Reticle` 100% | Cruz duplex: fina no centro, engrossando na borda, com marcas descendo |
| Redimensionar o overlay | A abertura continua circular, não vira elipse |

Verificado objetivamente num overlay 600x394 sobre um alvo de grade, medindo a fração do
quadro que é quase preta:

| Configuração | Centro | Preto |
|---|---|---|
| Passthrough | 255/255/255 | 2,2% |
| Desligado, abertura 0,30 configurada | 255/255/255 | 2,2% |
| Ligado, `intensity` 0 | 255/255/255 | 2,2% |
| Ligado, abertura 0,60 | 255/255/255 | 41,6% |
| Idem, retículo 100% | 236,9 | 42,6% |
| Binóculo, abertura 0,52 | 255/255/255 | 28,4% |

41,6% confere com a geometria: com `cornerRadius` 0,911 e raio 0,547 o círculo é cortado em
cima e embaixo, sobrando 59,8% de vidro.

### Formas de distortion, estilos de scanline e modos de CA

| Passo | Esperado |
|---|---|
| Cada `Shape` do Distortion com `Amount` 0 | Imagem intacta em todas — a continuidade em zero não depende da forma |
| `CRT` contra `Radial` no mesmo `Amount` | Linhas no meio das bordas ficam mais retas no `CRT` |
| `Corner only` | Centro plano, deformação concentrada nos cantos |
| `Cylindrical` / `Vertical` | Deformam um eixo só |
| `Aperture grille` e `Slot mask` | Colorem por canal; escurecem menos que `Hard` na mesma intensidade |
| `Beam width` 0% → 100% sobre uma imagem com claros e escuros | As linhas engordam **só** onde a imagem é clara; nas sombras continuam finas |
| `Interlace` 100%, observando em movimento | O padrão alterna meia linha a cada campo; numa imagem parada nada muda |
| Bloom `Halo colour` quente sobre um realce branco | O halo fica com borda quente, e a imagem por baixo não muda de cor |
| Scope `Shape` = `Tube` | Retângulo de cantos arredondados, cortando os cantos em preto **sólido**, ocupando quase todo o overlay |
| `Prism` | Cada canal sai num ângulo diferente, não só para os lados |
| `Barrel` | Separação cresce com o quadrado da distância: centro limpo, borda forte |

### Bloom — ADR-0009

| Passo | Esperado |
|---|---|
| Desligado | Imagem idêntica à original |
| `Threshold` 100% | Nada floresce, por mais alta que esteja a intensidade |
| `Threshold` baixando | O halo aparece primeiro nas fontes de luz e vai descendo para os midtones |
| Fonte colorida forte | O halo tem a cor da luz, não branco |
| `Radius` no máximo | O padrão em espiral das amostras fica visível — é o trade do ADR-0009 seção 2, não um defeito |
| Overlay maior que a fonte (letterbox) | O halo **não** vaza para dentro das barras |

A última linha é a que verifica `SampleSourceRgb`: uma amostra fora do conteúdo contribui
zero. Se contribuísse a cor da borda, apareceria um halo dentro do letterbox, onde não há
imagem para florescer.

### False Colour — ADR-0009

| Passo | Esperado |
|---|---|
| Desligado | Imagem idêntica à original |
| `White hot` com `Saturation` 1 | O mapeamento lê o brilho contaminado pela cor da fonte — é por isso que a receita pede dessaturar antes |
| `Saturation` 0 e depois `White hot` | Monocromático limpo do escuro ao claro |
| `Black hot` | O inverso exato do anterior |
| `Ironbow` | Passa por roxo e vermelho antes do laranja e do branco — a matiz sobe **e desce** |
| `Phosphor green` / `White phosphor` | Verde P43 / branco levemente azulado; o preto continua preto nos dois |
| `Levels` 0% → 100% | A rampa contínua vira degraus visíveis |
| `Intensity` 50% | Mistura da paleta com a imagem gradada |

`Ironbow` é a verificação que importa: nenhuma tinta multiplicativa produz uma matiz não
monotônica, e é isso que justifica o módulo existir separado da correção de cor.

### Edge Glow — ADR-0009

| Passo | Esperado |
|---|---|
| Desligado | Imagem idêntica à original |
| Ligado sobre uma cena com contraste | Contornos brilhantes nas silhuetas |
| **Com Scanlines ligadas junto** | Os contornos seguem o conteúdo, **não** as linhas da máscara |
| **Com Noise ligado junto** | Idem: o grão não vira contorno |
| `Width` mínimo → máximo | Linha fina → linha grossa |
| `Colour` alterada | O contorno muda de cor; a imagem por baixo não |
| Com `False Colour` ligado | O contorno mantém a própria cor, não é recolorido pela paleta |

As duas linhas em negrito são o motivo de a derivada sair da textura de origem e não do
resultado processado. Se saísse do resultado, ligar scanlines encheria a tela de contorno.

### Lens Dirt — ADR-0009

| Passo | Esperado |
|---|---|
| Desligado | Imagem idêntica à original |
| Ligado, observado por 30 s com a fonte parada | As manchas **não se movem** — é sujeira em vidro, não chuva |
| `Density` mínimo → máximo | Poucas manchas grandes → muitas manchas |
| `Smear` mínimo → máximo | Pontos redondos → riscos alongados na vertical |
| Redimensionar o overlay | As manchas acompanham a janela sem esticar de forma anisotrópica |

## 14. Glitch — AT-012

Aba **Effects**. O glitch é procedural e dirigido por `g_time`: nenhum frame anterior é
guardado (ADR-0003). O tempo é fatiado em intervalos curtos e cada fatia sorteia um número;
a fatia dispara quando o número cai abaixo de `Frequency`.

| Passo | Esperado |
|---|---|
| OFF | Nenhum glitch; imagem idêntica à de antes |
| `Frequency` 0%, `Intensity` 100% | **Nunca dispara** — é a prova de que frequência independe da intensidade |
| `Frequency` 100% | Dispara continuamente, com severidade variando entre as rajadas |
| `Intensity` | Só muda o quanto distorce, nunca a cadência |
| `Block size` | Altura das bandas deslocadas — e, junto com ela, a granularidade do jitter |
| `Jitter` | Ruído fino por linha, mesmo nas bandas paradas |
| `RGB shift` | Separação horizontal dos canais durante a rajada |

O overlay continua animando o glitch mesmo com a fonte parada — o loop se auto-dirige a
~30 Hz quando há algo animado na tela.

Verificado objetivamente nesta bancada:

| Medição | Resultado |
|---|---|
| `frequency=0%` com `intensity=100%` vs. módulo desligado | **0 pixels diferentes em 400.000** |
| `frequency=100%` vs. módulo desligado | 39,7% dos pixels alterados |
| Dois frames sucessivos do mesmo run | 44,9% e 46,1% — confirma que anima sozinho |

### Calibração da intensidade

O ponto de referência: em `intensity` 15% (o default) a imagem fica visivelmente instável
mas totalmente legível; em 55% (o preset *Broken Signal*) fica claramente danificada e ainda
assim legível. Se em 55% a imagem virar ruído ilegível, algo regrediu.

As linhas de jitter derivam do `blockSize`, não de uma contagem fixa. Isso importa: com uma
grade fixa e fina demais, linhas vizinhas deslocando em direções opostas transformam o quadro
num pente ilegível em vez de num sinal instável.

## 14b. Noise, Flicker e Jitter — ADR-0007

Aba **Effects**. Como o glitch, os três são procedurais e função pura de `g_time`.

| Passo | Esperado |
|---|---|
| Os três desligados | Imagem idêntica à original |
| **Noise** `Speed` baixo | O grão fica parado e depois troca de uma vez, como grão de filme — não rasteja |
| **Noise** `Grain size` alto | Salpicos visivelmente mais grossos |
| **Noise** `Colour` 0% / 100% | Grão cinza / cada canal com o seu salpico |
| **Flicker** intensidade máxima | Pulsa, mas nunca chega a estrobo — a oscilação é limitada a metade do slider |
| **Flicker** observado por 30 s | Não entra num loop reconhecível |
| **Jitter** intensidade alta | O quadro vagueia e as bordas entram em vista; isso é esperado |
| **Jitter** com `Scope` `Magnification` 2x | A trepidação aparece ampliada |
| `Disable all` / `Reset all` | Desligam tudo / voltam aos defaults sem perder o resto |

## 14c. Shimmer, Rolling Shutter e Scan Sweep — ADR-0009

Aba **Effects**. Os três são função pura de `g_time` e da coordenada, como os anteriores.

| Passo | Esperado |
|---|---|
| Os três desligados | Imagem idêntica à original |
| **Shimmer** ligado | A imagem **ondula** — linhas retas viram onduladas; é o que o distingue do Jitter, que move o quadro rígido |
| **Shimmer** `Scale` baixo / alto | Fervura fina / ondulação larga e lenta |
| **Shimmer** observado por 30 s | Nenhuma grade ou período reconhecível aparece |
| **Rolling Shutter** ligado | O quadro **inclina** e a inclinação varia no tempo |
| **Rolling Shutter**, olhando a linha do meio | Ela fica parada; o cisalhamento pivota nela — se tudo deslocasse junto seria Jitter |
| **Scan Sweep** ligado | Uma barra atravessa de baixo para cima e reentra sem emenda |
| **Scan Sweep** olhando a barra de perto | Borda de ataque dura, rastro que desvanece atrás — não é simétrica |
| **Scan Sweep** `Width` | Barra fina → barra larga |
| **Glitch** `Style` = `Digital` | Perde macroblocos inteiros; **dentro** do bloco a imagem fica intacta |
| **Glitch** `Digital` com `Block size` alto | Blocos maiores, e nenhum cisalhamento entre eles |
| **Glitch** `Frequency` 0% com `Digital` | Nunca dispara, mesmo com intensidade no máximo |

## 15. Presets — AT-013, RF-017

Aba **Presets**. Os arquivos ficam em `%APPDATA%\OverlayDesk\presets\*.json`.

| Passo | Esperado |
|---|---|
| Primeira execução | Os 101 presets de fábrica são escritos; o log registra `Presets: wrote 101 built-in presets` |
| Deletar um preset e reabrir a aplicação | **Não** é recriado — deletar é definitivo |
| **AT-013**: aplicar um preset, mexer nos sliders, reaplicar | Os valores salvos voltam exatamente |
| Selecionar e `Apply` (ou duplo clique) | O overlay muda na hora, mesmo com a fonte parada |
| `Save As` com um nome já usado | Vira `Nome (2)`, sem sobrescrever nada |
| `Save` | Sobrescreve o preset selecionado com o look atual; um built-in sobrescrito deixa de ser marcado como built-in |
| `Duplicate` | Cria `Nome (2)` com os mesmos valores |
| `Rename` | Renomeia o arquivo junto, sem deixar órfão |
| `Delete` | Pede confirmação antes |
| `New` | Zera filtros e efeitos para os defaults, sem gravar nada |
| Clicar num cabeçalho de seção | Dobra e desdobra o grupo inteiro; o número ao lado é a contagem |
| `Save As` | O preset novo aparece na seção `Custom`, no fim da lista |
| `Rename` num preset de fábrica | Ele **continua** na seção dele — a categoria está no arquivo, não no nome |
| Atualizar de uma versão anterior | Os presets que já estavam no disco aparecem nas seções certas, mesmo sem o campo `category` no arquivo (recuperado pelo nome) |

A linha **Current look** mostra `Neutral`, `Neutral (edited)` ou `Custom` conforme os
parâmetros ao vivo batam ou não com o preset ativo. Aplicar qualquer um dos 101 tem de deixar
essa linha mostrando o **nome**, nunca `Custom` — é isso que prova que `PresetMatches` cobre
todo campo que os presets usam, inclusive os módulos do ADR-0009.

Um preset guarda **apenas filtros e efeitos**. Geometria da janela, target e FPS nunca
entram nele (CONFIGURATION.md), então o mesmo arquivo funciona em outra máquina.

O round-trip de todos os parâmetros, a tolerância a arquivo corrompido e a proteção contra
`../` no nome são cobertos pelo teste automatizado `presets.repository`.

## 16. Memória e estabilidade — RNF-002, AT-016

Automatizado:

```powershell
.\scripts\stability-test.ps1              # os 60 minutos do AT-016
.\scripts\stability-test.ps1 -Minutes 10  # verificação rápida
```

O script abre uma fonte que repinta continuamente (um console em loop, via `conhost.exe` —
o `powershell.exe` direto abre no Windows Terminal, cuja janela não é identificável pelo
matcher), liga todos os filtros e o glitch, e amostra o processo a cada 15 s: working set,
private bytes, handles, objetos GDI e USER, threads. No fim ajusta uma reta em cada série e
grava tudo num CSV em `build\`.

O veredito considera **private bytes, handles e objetos GDI/USER**. Working set é reportado
mas não decide sozinho: o Windows expande o working set de um processo saudável sempre que
há RAM livre, então ele sobe naturalmente e reprovaria toda corrida numa máquina ociosa. Ele
só conta contra a corrida quando a memória comprometida concorda.

Uma corrida interrompida sai com código 2 (`INCOMPLETE`), nunca como aprovada — o AT-016 pede
60 minutos, e relatar uma corrida parcial como PASS seria afirmar algo que não foi medido.

| Medida | Alvo |
|---|---|
| Working Set em 1080p | ideal `<150 MB`, investigar acima de `250 MB` |
| 60 minutos capturando | memória comprometida, handles e recursos D3D não crescem continuamente |

Medido nesta bancada (Intel Graphics, overlay 960x540, todos os filtros e o glitch ligados),
em **12,8 minutos** antes da corrida ser interrompida:

| Série | Início → fim | Veredito |
|---|---|---|
| Private bytes | 54,2 → 56,1 MB | estável |
| Handles | 355 → 366 (faixa 348–369) | estável |
| Objetos GDI | 12 → 13 | estável |
| Objetos USER | 16 → 16 | estável |
| Working set | 63,6 → 75,7 MB | subindo, com private bytes plano — não é vazamento |

**A corrida de 60 minutos ainda não foi concluída.** Ela precisa da máquina livre: o overlay
fica no topo durante todo o teste, e qualquer interação com o aplicativo encerra a medição.

## 17. Hot reload de shaders (só em Debug)

| Passo | Esperado |
|---|---|
| Editar `shaders/OverlayDeskPS.hlsl` e clicar em `Reload shaders` | O overlay aplica a mudança sem reiniciar |
| Introduzir um erro de sintaxe e recarregar | O shader anterior continua ativo; o erro aparece no log e na barra de status |

## 18. Hotkeys globais — ADR-0009

Aba **Hotkeys**. Os atalhos são globais: o teste só vale com o **foco em outra janela**, que é
a única situação em que eles importam.

### Defaults

| Passo | Esperado |
|---|---|
| Primeira execução | Dez ações vêm bound; a aba lista todas com a combinação |
| `Ctrl`+`Shift`+`O` e `Ctrl`+`Shift`+`0`, com o jogo em foco | As duas entram e saem do Edit Mode |
| `Ctrl`+`Shift`+`H` | Overlay some e volta; o alvo continua selecionado |
| `Ctrl`+`Shift`+`C` | Click-through alterna; o checkbox na aba Overlay acompanha |
| `Ctrl`+`Shift`+`T` | Always-on-top alterna |
| `Ctrl`+`Shift`+`F` | Fullscreen entra e sai, voltando ao tamanho anterior |
| `Ctrl`+`Shift`+`M` | O overlay encaixa na janela capturada |
| `Ctrl`+`Shift`+`→` / `←` | Percorre a lista de presets nos dois sentidos |
| `Ctrl`+`Shift`+`P` | O painel vem para a frente, mesmo minimizado |
| Segurar qualquer um deles | Dispara **uma vez** só — `MOD_NOREPEAT`; sem isso um toggle piscaria enquanto a tecla estivesse pressionada |

### Rebind

| Passo | Esperado |
|---|---|
| Clicar no botão da combinação | Vira `Press a key...` |
| Apertar `Ctrl`+`Shift`+`F7` | O botão passa a mostrar `Ctrl+Shift+F7` e o atalho novo funciona na hora |
| Apertar uma tecla **sem** modificador | Nada é gravado; o log registra o motivo. Uma tecla solta registrada globalmente sumiria de todos os outros programas |
| `Esc` durante a captura | Cancela sem mudar nada |
| `Clear` | Ação fica `Not bound` e o atalho para de funcionar |
| `Default` numa linha / `Restore defaults` no topo | Volta a combinação daquela linha / de todas |
| Bindar duas ações na mesma combinação | A aba avisa `Also bound to "..."` |
| Desmarcar `Global shortcuts enabled` | Todos param de funcionar **e as combinações são liberadas** para outros programas; as bindings continuam salvas |
| Remarcar | Tudo volta como estava |

Para provar a liberação: com o app aberto e os atalhos ligados, um segundo programa que tente
registrar `Ctrl`+`Shift`+`O` falha; desmarcando `Global shortcuts enabled`, ele consegue.

### Conflito com outro programa

| Passo | Esperado |
|---|---|
| Outro programa segurando a combinação | A linha mostra `unavailable` em laranja e o log registra qual |
| A mesma ação pelo botão do painel | Continua funcionando — nenhuma ação existe **só** no atalho |
| Fechar o outro programa e clicar `Default` | A combinação volta a registrar |

### Persistência

| Passo | Esperado |
|---|---|
| Rebindar, fechar, reabrir | A combinação nova volta |
| Abrir `%APPDATA%\OverlayDesk\settings.json` | O bloco `hotkeys` traz nomes legíveis (`"modifiers": ["ctrl","shift"], "key": "F7"`), não códigos numéricos |
| Editar o arquivo à mão com uma tecla válida | É respeitada na próxima abertura |
| Editar com um nome de tecla inexistente | A ação fica sem atalho; o resto do arquivo carrega normalmente |

O arquivo guarda nomes e não os códigos de tecla do Windows porque o ADR-0004 escolheu JSON
justamente para o arquivo poder ser lido e corrigido à mão, e um virtual-key em hexadecimal
anularia isso na seção que o usuário tem mais motivo para editar.
