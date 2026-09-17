# ADR-0013 — Sharpen: o crunch de ISP como módulo próprio

## Status

Accepted

## Contexto

O conjunto de módulos descreve bem o **caminho óptico** de uma câmera: a lente deforma
(distortion), não resolve nos cantos (lens softness), separa canais (chromatic aberration), o
sensor floresce (bloom), ruidifica (noise) e cisalha (rolling shutter), e o elemento frontal
está sujo (lens dirt). Com isso já é possível montar um bodycam convincente — o ADR-0009 foi
escrito em parte para isso.

Falta uma etapa da cadeia: o **processador de imagem**. Toda câmera de ação responde ao próprio
sensor pequeno atrás de uma lente muito aberta com um unsharp mask agressivo, forte o bastante
para que o halo em volta de uma borda de alto contraste seja visível a olho nu. Esse halo é tão
característico do formato quanto o barril é, e é o que separa "um jogo com uma lente curva" de
"uma filmagem de GoPro".

Nenhum módulo atual chega perto disso:

- **Correção de cor** é um conjunto de operações tonais — cada pixel entra e sai sozinho.
  Nitidez lê os vizinhos. Foi por ser tonal que o `posterize` coube ali (ADR-0009 §3); pelo
  mesmo critério, isto não cabe.
- **Lens softness** é a operação inversa e no lugar errado do quadro: ela borra os cantos e
  deixa o centro em paz. Inverter o sinal do parâmetro dela produziria um afiamento que só
  existe nos cantos, que é o oposto do que uma lente e um ISP fazem juntos.
- **Edge glow** também é uma derivada espacial, mas soma uma linha colorida por cima em vez de
  devolver o high-pass à imagem. São efeitos diferentes com a mesma matemática de base.

A pergunta é a mesma dos ADR-0007 e ADR-0009: isso cabe no ADR-0003 ou o contradiz.

## Decisão

### 1. Módulo próprio, não parâmetro

Pela regra 2 do ADR-0007, variante de uma operação existente vira enum e operação nova vira
módulo. Nitidez é operação nova: uma derivada espacial somada de volta. Precisa do próprio
`enabled`/`intensity` para ser desligada sem perder o ajuste, e do próprio `radius`, que não
significa a mesma coisa que o `center` do lens softness nem que o `width` do edge glow.

```text
enabled
intensity   quanto do high-pass volta para a imagem
radius      distância das amostras, e portanto a espessura do halo
```

### 2. Posição 8b: depois do bloom, antes de qualquer gradação

O pipeline passa a ter dezenove posições. A cadeia física é o argumento:

```text
 7b. lens softness     a lente desfoca
 8.  bloom             o sensor floresce com a luz que chegou
 8b. sharpen           o ISP afia o que leu
 9.  color correction  a imagem é gradada
```

O estágio ocupa a lacuna que o próprio ADR-0009 abriu entre o bloom e a correção de cor, e
**não altera a ordem relativa de nenhum par já congelado** — a subsequência do ADR-0003
(`Glitch UV → Distortion → Chromatic Aberration → Color Correction → Scanlines → Vignette`)
continua intacta, e as posições fixadas pelo ADR-0007 e pelo ADR-0009 também. É exatamente o
movimento que o ADR-0011 fez ao inserir o lens softness em 7b.

Vir **antes** da gradação importa: afiar depois da correção de cor afiaria o contraste que o
usuário acabou de aplicar, e o halo mudaria de espessura ao mexer num slider que não é este.

### 3. O high-pass sai da textura de origem

Pelo mesmo motivo que o ADR-0009 dá para o edge glow: o resultado corrente já carrega grão,
scanlines e vinheta quando um estágio tardio olha para ele, e a derivada acharia as bordas
**desses padrões** em vez das do conteúdo. As amostras vêm de `SampleSourceTap`.

A consequência aceita é que o high-pass não conhece o amolecimento que o lens softness aplicou
logo antes: nos cantos o sharpen devolve um detalhe que a lente teria perdido. Na prática os
dois se equilibram na faixa em que são usados juntos, e a alternativa — amostrar o resultado já
processado — troca esse desvio por um defeito pior e sempre visível.

### 4. Ponderação por cobertura é obrigatória

Um tap fora do conteúdo devolve `w = 0`. Uma média sem peso leria preto ali, o high-pass iria
fortemente para o positivo e o overlay ganharia uma **borda brilhante** em volta da imagem,
exatamente na fronteira com o letterbox. É a mesma armadilha que o ADR-0011 registrou para o
anel do lens softness.

Isso está preso por teste: `TestSharpenDoesNotRimTheContentEdge` renderiza um campo chapado,
onde um unsharp mask não tem nada para afiar, e exige que o estágio seja um no-op. Sem a
ponderação o teste reprova com 127 níveis de diferença.

### 5. Quatro amostras numa cruz, não uma gaussiana

Um unsharp mask precisa de uma **média local**, não de um borrão suave. Uma cruz é a coisa mais
barata que produz uma sem viés direcional. É o quarto estágio de custo não constante por pixel,
depois do bloom (12 taps), do lens softness (6) e do edge glow (4); são 5 taps no total.

Nenhuma textura nova, nenhum render target intermediário, nenhum histórico de frame. O ADR-0003
e o orçamento do ADR-0005 continuam valendo sem exceção. Desligado custa um branch coerente e
nada mais, que é o que o AT-010 e `TestDisabledModulesAreFree` exigem.

O `radius` é expresso em unidades normalizadas e corrigido por aspecto, e não em pixels, para
que redimensionar o overlay não mude a espessura do halo (AT-007).

### 6. `schemaVersion` não é incrementado

Campos aditivos que nascem desligados. Vale integralmente o argumento do ADR-0009 §5:
incrementar faria qualquer build anterior descartar **toda** a configuração do usuário ao
encontrar o arquivo novo, em vez de apenas ignorar os campos que não entende.

## Consequências

- O constant buffer vai de **368 para 384 bytes** (24 registradores), com dois floats de padding
  no registrador novo. O `static_assert` de `RenderResources.h` sobe junto.

  > Correção de registro: o ADR-0009 diz 352 bytes e o ADR-0011 diz que o buffer "continua em
  > 352". Os dois ficaram desatualizados quando `scanlineBeamWidth`, `scanlineInterlace` e
  > `bloomTint` entraram. O valor correto antes desta mudança era 368; o `static_assert` sempre
  > foi a fonte da verdade.

- O `enabledMask` vai de 19 para **20 bits** (`1u << 20`).
- A regra de estilo HLSL do ADR-0007 continua obrigatória: **uma variável de resultado e um
  único `return` por função**, porque o `fxc` roda com `/WX` e o caminho `/Od` do build Debug
  reporta `return` antecipado dentro de branch como X4000.
- O preset `GoPro Bodycam` nasce com este módulo ligado. Ele é o primeiro a combinar lens
  softness e sharpen ao mesmo tempo — cantos moles e um meio afiado, que é como um sensor
  pequeno atrás de uma lente muito aberta de fato se comporta.
- `ShaderPipelineTests` ganha `--write-presets-from <bmp> <dir> <nome...>`, que roda um preset
  sobre uma imagem de verdade em vez da grade sintética. Sem isso não havia como comparar um
  look com a filmagem que ele imita, que é o único critério que um preset tem.

## Alternativas descartadas

**Parâmetro bipolar no lens softness** (`-1` afia, `+1` borra). Economizaria um módulo, mas as
duas metades não são a mesma operação com o sinal trocado: o borrão do lens softness cresce
quadraticamente **a partir de** `center` em direção aos cantos, e o afiamento de um ISP é
uniforme sobre o quadro inteiro. Um `center` compartilhado significaria coisas incompatíveis em
cada metade, que é a mesma objeção que o ADR-0009 fez à fusão de rolling shutter e jitter.

**Sharpen depois da correção de cor.** Mais simples de encaixar, e errado: o halo passaria a
depender do contraste e da gama, e mexer na gradação mudaria a espessura de um artefato que
pertence a outro estágio.

**Kernel 3x3 completo (8 taps).** Média local melhor, ao dobro do custo do que já é o quarto
estágio mais caro do shader. Numa cruz o viés que sobra é diagonal e não sobrevive ao grão que o
preset liga logo depois.

**Passe dedicado com um blur separável de verdade.** Recusado pelo mesmo motivo que o ADR-0009
recusou para o bloom: custa uma textura do tamanho da janela e duas passadas, e o ADR-0005 não
tem orçamento para isso. Fica registrado para quem vier: **se um dia a nitidez precisar de
qualidade fotográfica, isso é um passe novo e um ADR novo.**
