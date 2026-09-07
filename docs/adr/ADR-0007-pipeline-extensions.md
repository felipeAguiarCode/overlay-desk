# ADR-0007 — Extensões do pipeline: jitter, noise, flicker e variantes de forma

## Status

Accepted

## Contexto

O ADR-0003 fixa a ordem canônica do pipeline em seis estágios:

```text
Captured Texture → Glitch UV → Distortion → Chromatic Aberration
                 → Color Correction → Scanlines → Vignette → Output
```

e proíbe render targets intermediários — tudo acontece num único pixel shader.

Três demandas apareceram depois que essa base ficou pronta:

1. **Mais tipos dentro dos filtros existentes.** Uma única forma de distorção (radial) e um
   único desenho de scanline (onda quadrada) não cobrem nem os monitores que o produto quer
   imitar. Um tubo CRT não deforma como uma lente esférica, e uma máscara de abertura
   Trinitron não se parece com uma linha escura.

2. **Efeitos que o conjunto de filtros não consegue expressar.** Grão de sensor, oscilação de
   lâmpada e trepidação de câmera são dinâmicos e não são transformações contínuas da imagem.
   O glitch já ocupava essa categoria sozinho.

3. **Tinta de cor.** O modelo de correção de cor era brilho/contraste/saturação/gama, que não
   consegue produzir um monocromático *em uma cor*: verde de Game Boy, fósforo de visão
   noturna, vermelho de Virtual Boy. Isso não é ajuste de saturação, é uma tinta.

A pergunta arquitetural é se essas três coisas cabem no ADR-0003 ou o contradizem.

## Decisão

### 1. A ordem dos seis estágios originais não muda

Os estágios novos são **acrescentados**, nunca inseridos entre dois estágios existentes de
forma que altere a relação entre eles. O pipeline passa a ser:

```text
Captured Texture → Jitter UV → Glitch UV → Distortion → Chromatic Aberration
                 → Color Correction → Noise → Scanlines → Vignette → Flicker → Output
```

A subsequência `Glitch UV → Distortion → Chromatic Aberration → Color Correction →
Scanlines → Vignette` está intacta. O ADR-0003 continua valendo como escrito.

**Posição de cada estágio novo.**

- **Jitter** antes do glitch, porque é um deslocamento da imagem inteira: ele descreve onde a
  câmera está, e o glitch descreve o que aconteceu com o sinal depois disso.
- **Noise** depois da correção de cor e antes das scanlines. Grão é ruído de sensor: ele entra
  na imagem antes de ela chegar ao monitor, então o padrão de linhas do monitor incide sobre o
  grão, e não o contrário.
- **Flicker** por último, porque é a lâmpada ou o tubo variando de brilho — afeta tudo que já
  foi desenhado, inclusive as scanlines e o vinheteamento.

### 2. Variantes ficam dentro do filtro, não viram filtros novos

`DistortionShape`, `ScanlineStyle` e os modos adicionais de `ChromaticAberrationMode` são
parâmetros enumerados dos filtros que já existiam, não módulos separados.

**Motivação.** São formas diferentes da mesma operação. Separá-las multiplicaria o número de
módulos, duplicaria os controles de `enabled`/`intensity` e permitiria estados sem sentido
como duas distorções ativas ao mesmo tempo. Como enum, o shader escolhe o cálculo dentro de um
branch que a wave inteira concorda, que é o mesmo custo que o filtro já tinha.

O parâmetro bipolar da distorção (`-1` anti-fisheye … `0` neutro … `+1` fisheye) vale para
**todas** as formas: cada forma traz junto o seu próprio anti-fisheye, sem parâmetro extra.
A ancoragem dos cantos — dividir por `(1 + k)` — também vale para todas, o que garante que
nenhuma quantidade de distorção jogue conteúdo para fora da tela nem abra uma falha
transparente na borda.

### 3. Tinta multiplica, não soma

`ColorCorrectionSettings` ganha `tint` (RGB) e `tintAmount`, aplicados **por multiplicação**,
depois da saturação e antes da intensidade global do módulo:

```text
corrected = lerp(corrected, corrected * tint, tintAmount)
```

**Motivação.** Multiplicar preserva o preto: o que estava apagado continua apagado, e só o que
estava aceso recebe a cor. É o que um fósforo ou um corante de LCD fazem fisicamente. Somar
lavaria a imagem inteira na cor, inclusive as áreas escuras, que é o resultado errado.

Vem depois da saturação porque a receita que funciona é *dessaturar e então tintar*: tintar uma
imagem ainda colorida apenas a enviesa; tintar uma monocromática reproduz uma tela de um
corante só, exatamente.

Com `tintAmount = 0` a expressão é identidade, então o estágio continua custando zero quando
não é usado — o que o AT-010 exige.

### 4. Nenhum estágio novo guarda frame anterior

Ruído, flicker e jitter são funções puras de `g_time` e da coordenada. Nada é acumulado entre
frames, nada é lido de volta, nenhum render target intermediário é criado. A proibição de
histórico de frames do `CLAUDE.md` e do ADR-0003 continua respeitada.

O ruído quantiza a célula **e** o passo de tempo, de forma que o grão é re-sorteado em
intervalos discretos em vez de rastejar continuamente — que é como grão de filme se comporta e
como grão animado por tempo contínuo não se comporta.

## Consequências

- O constant buffer cresce. Ele é escrito uma vez por frame com `Map(WRITE_DISCARD)` num
  buffer criado uma única vez, então o custo é o tamanho da cópia, não uma alocação.
- O `enabledMask` ganha três bits. Cada estágio desligado custa um branch coerente e nada mais.
- Presets antigos continuam carregando: campos ausentes no JSON caem nos defaults, e os
  defaults dos estágios novos são "desligado".
- O shader ficou grande o suficiente para que a regra de estilo importe: **uma variável de
  resultado e um único `return` por função**. O caminho não otimizado do `fxc` (`/Od`, usado no
  build Debug) reporta `return` antecipado dentro de branch como X4000 "potentially
  uninitialized variable", e o build trata warning como erro. O caminho otimizado dissolve a
  diferença, então as duas formas custam o mesmo em Release.

## Alternativas descartadas

**Um render target por estágio.** Resolveria a legibilidade do shader ao custo de uma textura
do tamanho da janela por estágio e uma passada de leitura/escrita por estágio. O ADR-0003 já
recusou isso e o ADR-0005 não tem orçamento para as texturas.

**Grão vindo de uma textura de ruído.** Seria mais barato por pixel que o hash, mas
adicionaria um recurso de GPU permanente e um padrão que se repete. O hash é determinístico,
não ocupa memória e não tem período visível.

**Filtro de tinta separado.** Daria mais um `enabled`/`intensity` para gerenciar e permitiria
tintar antes da saturação, que é justamente a ordem que não funciona.
