# Especificação — Filtros e Efeitos

## 1. Regra global

Todo módulo deve possuir:

```text
enabled
intensity
```

`intensity` deve ser normalizado internamente entre `0.0` e `1.0`.

A UI pode exibir `0–100%`.

## 2. Filtros

Filtros alteram continuamente a imagem.

### 2.1 Distortion

#### Objetivo

Simular deformação óptica.

#### Parâmetros

```text
enabled
intensity
amount
shape
```

`amount`:

```text
-1.0 = anti-fisheye
 0.0 = normal
+1.0 = fisheye
```

`intensity` multiplica o resultado final.

#### shape

```text
radial       lente esférica, circular
crt          separável por eixo, como a geometria de um tubo
cylindrical  só horizontal, como uma ultrawide curva
vertical     só vertical
corner       queda quártica: centro plano, cantos puxam forte
```

O `amount` bipolar vale para **todas** as formas, então cada uma traz junto o seu próprio
anti-fisheye. Em zero as duas metades valem exatamente 1, então a continuidade sem caso especial
sai da álgebra.

As duas metades não são a mesma expressão com o sinal trocado (ADR-0010):

```text
k >= 0   (1 + k*t) / (1 + k)    fisheye
k <  0   1 / (1 - k*t)          anti-fisheye
```

Na metade **positiva** o fator fica em `[0, 1]`: o mapa só lê para dentro, os extremos ficam
ancorados em si mesmos e nada é cortado.

Na metade **negativa** o fator nunca passa de 1, mas os extremos deixam de ser ancorados: o
anti-fisheye **corta**. Isso é obrigatório e não uma escolha de gosto — um pincushion puxa as
bordas para dentro, e o conteúdo que ocuparia o lugar delas está fora do que foi capturado.
Cortar é a alternativa a deixar uma falha transparente com o formato da distorção.

O que vale para as duas metades, e agora de fato vale: **nenhuma quantidade de distorção, em
nenhuma forma, abre uma falha transparente na borda.**

#### Requisitos

- centro estável;
- comportamento contínuo;
- sem saltos em zero;
- resize-safe.

### 2.2 Vignette

#### Objetivo

Adicionar bordas escurecidas como em editores de foto/vídeo.

#### Parâmetros

```text
enabled
intensity
size
softness
roundness
```

#### Comportamento

- `intensity`: força do escurecimento;
- `size`: área interna preservada;
- `softness`: suavidade da transição;
- `roundness`: de retangular para circular.

A vinheta deve ser aplicada depois da distorção para permanecer ancorada às bordas do overlay.

### 2.3 Scanlines

#### Parâmetros

```text
enabled
intensity
thickness
spacing
orientation
scaleMode
beamWidth
interlace
```

`beamWidth` é quanto uma área clara alarga a própria linha. Num tubo o feixe se espalha à
medida que é mais excitado, então realces têm linhas gordas que se fecham e sombras mantêm as
finas com preto entre elas. Em `0` o padrão é uniforme, que é o que lê como listras
sobrepostas em vez de tela. Ver ADR-0007 regra 2: é parâmetro do módulo, não filtro novo.

`interlace` alterna o padrão meia linha a cada campo, como o 480i é desenhado. Função de
`g_time` apenas — nenhum frame é guardado. Em `0` é progressivo.

#### orientation

```text
horizontal
vertical
grid
```

#### scaleMode

```text
relative
pixelPerfect
```

#### style

```text
hard             onda quadrada, bandas nítidas
soft             queda senoidal, suave
sharp            linhas escuras finas, claro entre elas
apertureGrille   tríades RGB verticais, estilo Trinitron
slotMask         tríades escalonadas, a máscara de sombra clássica
```

`apertureGrille` e `slotMask` colorem por canal em vez de escurecer uniformemente: o brilho
total cai menos do que em `hard` na mesma intensidade, porque é isso que uma máscara faz.

### 2.4 Chromatic Aberration

#### Objetivo

Separar canais RGB espacialmente.

#### Parâmetros

```text
enabled
intensity
mode
edgeBias
redShift
blueShift
```

#### Modes

```text
radial
horizontal
vertical
edge
prism        cada canal sai num ângulo próprio, como vidro separando luz
barrel       a separação segue r², como uma lente desfocando de verdade
```

#### Implementação conceitual

```text
R = sample(UV + redOffset)
G = sample(UV)
B = sample(UV + blueOffset)
```

### 2.5 Color Correction

#### Parâmetros

```text
enabled
intensity
brightness
contrast
saturation
gamma
tint (rgb)
tintAmount
posterize
```

Valores neutros devem resultar na imagem original.

`posterize` quantiza cada canal em degraus, do jeito que um display digital de poucos bits
faz. Em `0` é identidade; acima disso, quanto maior, menos degraus restam. É parâmetro deste
módulo e não filtro próprio porque é uma operação tonal, a mesma categoria de brilho,
contraste e gama. Ver ADR-0009.

#### tint

A tinta **multiplica** a cor, depois da saturação:

```text
corrected = lerp(corrected, corrected * tint, tintAmount)
```

Multiplicar preserva o preto — só o que estava aceso recebe a cor, que é o que um fósforo ou
um corante de LCD fazem. Somar lavaria a imagem inteira, inclusive as sombras.

Vem depois da saturação porque a receita que funciona é dessaturar e então tintar: tintar uma
imagem ainda colorida apenas a enviesa. Com `saturation = 0` e `tintAmount = 1` o resultado é
um monocromático real naquela cor — verde de Game Boy, fósforo de visão noturna, vermelho de
Virtual Boy.

Branco a `tintAmount = 0` é identidade. Ver ADR-0007.

### 2.6 Scope

#### Objetivo

O recorte de uma óptica: luneta, binóculo, olho mágico.

#### Parâmetros

```text
enabled
intensity
size
softness
magnification
reticle
shape
```

```text
size           raio da abertura, como fração da distância até o canto
softness       largura do esmaecimento na borda do vidro
magnification  1.0 = nenhuma; acima disso a vista estreita e amplia
reticle        0 = sem retículo, 1 = totalmente opaco
shape          circle | binocular | quadTube | tube
```

`quadTube` são quatro círculos sobrepostos, a vista panorâmica de um NVG de quatro tubos.
`tube` é o retângulo de cantos arredondados de um vidro de CRT, medido contra o quadro e não
contra o raio do canto — uma tela ocupa quase todo o overlay, ao contrário das ópticas.
Todas são a mesma abertura com outra geometria, não módulos novos (ADR-0009).

#### Comportamento

Este é o **único** módulo que escreve alpha. Fora da abertura o overlay fica opaco, não
apenas escuro:

```text
body  = (1 - glass) * intensity
rgb   = lerp(rgb, 0, body)
alpha = max(alpha, body)
```

Sem isso o corpo do óptico ficaria transparente sobre as barras de letterbox e o desktop
apareceria através do que deveria ser um tubo de aço. É por esse motivo que o scope não é um
preset do vignette. Ver ADR-0008.

A curvatura **não** pertence a este módulo: o estufamento continua sendo o filtro de
distorção, e os presets de óptica simplesmente ligam os dois.

`magnification` é aplicado antes do jitter, de forma que a trepidação é ampliada junto com a
imagem — que é o que torna um óptico de alta magnificação difícil de segurar firme.

### 2.7 Bloom

#### Objetivo

O florescimento de um sensor ou de um tubo intensificador saturado: luz forte espalha e engole
o detalhe em volta.

#### Parâmetros

```text
enabled
intensity
threshold
radius
```

```text
threshold  luminância a partir da qual um pixel floresce
radius     alcance do halo, como fração da imagem
tint (rgb) cor pela qual o halo é multiplicado
```

Branco em `tint` é identidade e deixa o halo com a cor da luz que o produziu. Esquentar o
tint é como se chega à **halation**: a luz que espalha dentro do vidro de um tubo volta
avermelhada, e é por isso que um realce branco num CRT tem borda quente e não branca.

#### Implementação

Uma espiral de amostras da textura de origem em torno do UV já distorcido, mantendo apenas a
parte acima do `threshold` e somando o resultado. É **glow, não uma gaussiana**: não há passe
dedicado nem render target intermediário, e o halo tem estrutura visível no `radius` máximo.
Amostra fora do conteúdo contribui zero, senão o halo vazaria para dentro do letterbox. Ver
ADR-0009 §2.

Aplicado **antes** da correção de cor: o florescimento acontece com a luz que chegou ao sensor,
não com a imagem já gradada. É isso que faz o halo de uma visão noturna ser gerado em
monocromático e só então receber o fósforo.

### 2.8 Lens Softness

#### Objetivo

Uma lente não resolve igual em todo o campo: é nítida no meio e se desfaz nos cantos, e quanto
mais aberta, mais cedo isso começa. É o que separa uma foto ultra-wide de um screenshot
deformado — a distorção descreve **onde** a luz vai parar, não a qualidade dela.

#### Parâmetros

```text
enabled
intensity
center
```

`center` é o raio da zona que permanece nítida, como fração da distância até o canto. Fora dela
o borrão cresce **quadraticamente** até a borda: um ramp linear deixaria o quadro inteiro
levemente mole, o quadrado mantém o centro intocado.

Seis amostras num anel. É um amolecimento de baixa frequência, não um bokeh, e aplicado **antes**
do bloom — o desfoque acontece na lente, o florescimento no sensor atrás dela. Ver ADR-0011.

### 2.9 Sharpen

#### Objetivo

O crunch que uma câmera de ação aplica na própria imagem. Um sensor pequeno atrás de uma lente
muito aberta resolve mal, e o ISP responde com um unsharp mask agressivo — forte o bastante para
que o halo em volta de uma borda de alto contraste apareça. Esse halo é tão característico do
formato quanto o barril.

#### Parâmetros

```text
enabled
intensity
radius
```

`radius` é a distância das amostras, como fração do quadro, e portanto a espessura do halo.

#### Implementação

Unsharp mask de quatro amostras numa cruz. O high-pass sai da **textura de origem**, não do
resultado já processado — este último carrega grão, scanlines e vinheta, e a derivada acharia as
bordas desses padrões. Amostra fora do conteúdo é ponderada por cobertura: sem isso a média
leria preto na fronteira do letterbox e a borda do conteúdo viraria uma linha brilhante.

Aplicado **depois** do bloom e **antes** de qualquer gradação: a lente desfoca, o sensor
floresce com a luz que chegou, o ISP afia o que leu, e só então a imagem é gradada. Afiar depois
da correção de cor faria a espessura do halo depender do contraste e da gama. Ver ADR-0013.

Combina com `lensSoftness`: cantos moles e um meio afiado é o que um sensor pequeno atrás de uma
lente muito aberta produz.

### 2.10 False Colour

#### Objetivo

Mapear luminância para uma paleta. É o que separa uma vista térmica de verdade de um
monocromático tingido.

#### Parâmetros

```text
enabled
intensity
palette
levels
```

#### palette

```text
whiteHot       escuro → claro, monocromático, o padrão de um IR
blackHot       o inverso: quente é escuro
ironbow        preto → roxo → vermelho → laranja → branco
phosphor       o verde P43 de um tubo intensificador
whitePhosphor  o branco-azulado dos tubos modernos
crossCom       a rampa ciano de um HUD de sensor
```

`levels` em `0` deixa a rampa contínua; acima disso ela é quantizada nesse número de degraus,
como o display de poucos bits de um equipamento de campo.

`intensity` interpola entre a imagem gradada e a paleta, o que permite um térmico parcial.

#### Por que não é parâmetro da correção de cor

`ironbow` não é monotônico em matiz e `blackHot` inverte a luminância — nenhum dos dois é
expressável por multiplicação, que é tudo que a tinta faz. E a paleta precisa vir depois de
toda a correção de cor, inclusive da tinta. Ver ADR-0009 §4.

### 2.11 Edge Glow

#### Objetivo

O contorno brilhante de uma vista de sensor: silhuetas destacadas por uma linha de luz.

#### Parâmetros

```text
enabled
intensity
width
tint (rgb)
```

```text
width  espessura da amostragem, e portanto do contorno
tint   a cor da linha
```

#### Implementação

Uma derivada espacial da **textura de origem**, não do resultado já processado — este último
carrega scanlines, grão e vinheta, e a detecção acharia as bordas desses padrões em vez das do
conteúdo. O contorno é somado por cima, depois da false colour, para que a paleta térmica não
o recolora.

### 2.12 Lens Dirt

#### Objetivo

Sujeira, gordura e respingos no elemento frontal — o que mais denuncia uma câmera que vive
presa a um colete.

#### Parâmetros

```text
enabled
intensity
density
smear
```

```text
density  quantas manchas existem
smear    quanto elas borram em vez de pontuar
```

Procedural, como o grão: sem textura, sem recurso permanente de GPU, sem padrão que se repita.

Aplicado depois da vinheta — a sujeira está na frente de tudo que a lente produziu — e antes do
flicker e do corpo do óptico.

## 3. Efeitos

Efeitos introduzem comportamento dinâmico.

### 3.1 Glitch

#### Parâmetros

```text
enabled
intensity
frequency
blockSize
jitter
rgbShift
style
```

#### style

```text
analog   bandas deslocadas na horizontal e canais separados: fita e sinal de RF
digital  macroblocos e degraus de banding: um downlink comprimido caindo aos pedaços
```

São duas formas do mesmo evento — um estouro de sinal danificado — e por isso um enum dentro do
módulo, não um filtro de compressão à parte (ADR-0009 §3). `frequency`, `intensity` e
`blockSize` significam a mesma coisa nos dois: em `digital`, `blockSize` é a aresta do
macrobloco.

#### Regras

- procedural;
- baseado em time;
- pseudo-random determinístico por frame/segmento;
- não armazenar histórico;
- frequência independente da intensidade.

#### Semântica

Intensity:
- quanto o glitch distorce quando ocorre.

Frequency:
- com que frequência ocorre.

### 3.2 Noise

#### Parâmetros

```text
enabled
intensity
grainSize
speed
colorAmount
```

`speed` é a frequência com que o padrão é **re-sorteado**, não a velocidade de um movimento:
o grão fica parado entre os sorteios, como grão de filme, em vez de rastejar.

`colorAmount` em 0 é grão monocromático; em 1 cada canal ganha o seu próprio salpico.

### 3.3 Flicker

#### Parâmetros

```text
enabled
intensity
speed
```

Duas taxas incomensuráveis são misturadas, de forma que a pulsação nunca vira um loop
reconhecível. A oscilação é limitada a metade do slider — a faixa inteira seria um estrobo,
não uma oscilação.

### 3.4 Jitter

#### Parâmetros

```text
enabled
intensity
speed
```

O quadro inteiro vagueia, como um sinal instável ou uma câmera na mão. Não é o mesmo que o
`jitter` interno do glitch, que só existe durante um estouro e desloca linhas individuais em
vez do quadro todo.

### 3.5 Shimmer

#### Parâmetros

```text
enabled
intensity
speed
scale
```

```text
speed  com que rapidez o campo ondula
scale  o tamanho das ondulações
```

Uma perturbação contínua e orgânica do UV: ar quente subindo, ou o campo de uma camuflagem
óptica dobrando a luz atrás dele. Diferente do `jitter`, que desloca o quadro rigidamente, e do
`glitch`, que desloca bandas — nenhum dos dois ondula.

Aplicado antes da distorção, porque a lente vê a cena já ondulada.

### 3.6 Rolling Shutter

#### Parâmetros

```text
enabled
intensity
speed
```

O cisalhamento de um sensor CMOS que lê a imagem linha a linha enquanto a câmera se move: a
imagem inclina, e a inclinação varia no tempo.

Não é parâmetro do `jitter` porque os dois deslocam a imagem no tempo de formas
incompatíveis — o jitter desloca o quadro rigidamente, este cisalha em função da linha, e um
`intensity` compartilhado ficaria ambíguo (ADR-0009).

### 3.7 Scan Sweep

#### Parâmetros

```text
enabled
intensity
speed
width
```

```text
speed  com que rapidez a barra atravessa
width  a espessura da barra, como fração da imagem
```

A barra de luz de um sensor que varre. Aplicada logo antes do flicker: é luz do próprio
display, então respira com ele.

## 4. Ordem do pipeline

```text
 1. scope zoom
 2. jitter UV
 3. shimmer UV
 4. rolling shutter UV
 5. glitch UV
 6. distortion
 7. chromatic aberration
 7b. lens softness
 8. bloom
 8b. sharpen
 9. color correction
10. false colour
11. edge glow
12. noise
13. scanlines
14. vignette
15. lens dirt
16. scan sweep
17. flicker
18. scope mask
```

Os seis estágios originais (5 a 7, 9, 13 e 14) mantêm a ordem relativa que o ADR-0003 fixou. Os
estágios novos foram acrescentados, nunca inseridos entre dois existentes de forma que
alterasse a relação entre eles. Justificativa de cada posição: ADR-0007, ADR-0008, ADR-0009,
ADR-0011 e ADR-0013.

## 5. Estado desligado

Quando `enabled = false`:

- módulo não altera output;
- controles avançados permanecem persistidos;
- usuário pode reativar sem perder parâmetros.

## 6. Valores default sugeridos

```text
Distortion
enabled=false
amount=0
intensity=1
shape=radial

Scope
enabled=false
intensity=1
size=0.62
softness=0.05
magnification=1.0
reticle=0

Vignette
enabled=false
intensity=0.35
size=0.70
softness=0.75
roundness=0.30

Scanlines
enabled=false
intensity=0.25
thickness=1
spacing=2
style=hard
beamWidth=0
interlace=0

Chromatic Aberration
enabled=false
intensity=0.10
mode=radial
edgeBias=0.70

Color Correction
enabled=false
brightness=0
contrast=1
saturation=1
gamma=1
tint=(1,1,1)
tintAmount=0
posterize=0

Lens Softness
enabled=false
intensity=0.40
center=0.45

Sharpen
enabled=false
intensity=0.35
radius=0.35

Bloom
enabled=false
intensity=0.35
threshold=0.75
radius=0.35
tint=(1,1,1)

False Colour
enabled=false
intensity=1.0
palette=whiteHot
levels=0

Edge Glow
enabled=false
intensity=0.45
width=0.50
tint=(0.35,0.80,1.00)

Lens Dirt
enabled=false
intensity=0.25
density=0.50
smear=0.40

Glitch
enabled=false
intensity=0.15
frequency=0.05
blockSize=0.10
jitter=0.05
rgbShift=0.10
style=analog

Noise
enabled=false
intensity=0.12
grainSize=0.50
speed=0.60
colorAmount=0

Flicker
enabled=false
intensity=0.15
speed=0.50

Jitter
enabled=false
intensity=0.10
speed=0.50

Shimmer
enabled=false
intensity=0.20
speed=0.50
scale=0.50

Rolling Shutter
enabled=false
intensity=0.15
speed=0.50

Scan Sweep
enabled=false
intensity=0.30
speed=0.40
width=0.15
```
